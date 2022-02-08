#include "Server.hpp"
#include "../Utilities/Utilities.hpp"
#include <chrono>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

static const std::unordered_map<std::string, nlohmann::json (Server::*)(const nlohmann::json &)> ServiceMap = {
    {"echo", &Server::Echo},
    {"add_game", &Server::AddGame},
    {"add_state", &Server::AddState},
    {"add_player", &Server::AddPlayer},
    {"add_action_generator", &Server::AddActionGenerator},
    {"remove_game", &Server::RemoveGame},
    {"remove_state", &Server::RemoveState},
    {"remove_player", &Server::RemovePlayer},
    {"remove_action_generator", &Server::RemoveActionGenerator},
    {"generate_actions", &Server::GenerateActions},
    {"take_action", &Server::TakeAction},
    {"start_thinking", &Server::StartThinking},
    {"stop_thinking", &Server::StopThinking},
    {"get_best_action", &Server::GetBestAction},
    {"query_details", &Server::QueryDetails},
};

void Server::Run() {
    std::string reqStr;
    while (true) {
        std::getline(m_InputStream, reqStr);
        std::thread(Serve, this, std::move(reqStr)).detach();
    }
}

void Server::Serve(Server *self, std::string &&reqStr) {
    nlohmann::json response;
    try {
        const auto request = nlohmann::json::parse(reqStr);
        if (request.contains("id"))
            response["id"] = request["id"];
        Util::GetJsonValidator("request.schema.json").validate(request);
        const std::string &type = request["type"];
        const nlohmann::json &data = request["data"];
        const auto service = ServiceMap.at(type);
        response["data"] = (self->*service)(data);
        response["success"] = true;
    } catch (const std::exception &e) {
        response["errMsg"] = e.what();
        response["success"] = false;
    }
    const std::scoped_lock lock(self->m_MtxOutputStream);
    self->m_OutputStream << response << std::endl;
}

nlohmann::json Server::Echo(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/echo.schema.json").validate(data);
    const std::chrono::duration<double> time(data["sleepTime"]);
    std::this_thread::sleep_for(time);
    if (!data.contains("data"))
        return nlohmann::json::object();
    return {{"data", data["data"]}};
}

nlohmann::json Server::AddGame(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/add_game.schema.json").validate(data);
    auto game = Game::Create(data["type"], data["data"]);
    const auto id = m_GameMap.Emplace(std::move(game));
    return {{"gameID", id}};
}

nlohmann::json Server::AddState(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/add_state.schema.json").validate(data);
    unsigned int id;
    AccessGame(data, [&](GameRecord &gameRecord) {
        const auto &game = *gameRecord.GamePtr;
        auto state = data.contains("data") ? State::Create(game, data["data"]) : State::Create(game);
        id = gameRecord.SubStates.Emplace(std::move(state));
    });
    return {{"stateID", id}};
}

nlohmann::json Server::AddPlayer(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/add_player.schema.json").validate(data);
    unsigned int id;
    AccessState(data, [&](const GameRecord &gameRecord, StateRecord &stateRecord) {
        auto player = Player::Create(data["type"], *gameRecord.GamePtr, *stateRecord.StatePtr, data["data"]);
        id = stateRecord.SubPlayers.Emplace(std::move(player));
    });
    return {{"playerID", id}};
}

nlohmann::json Server::AddActionGenerator(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/add_action_generator.schema.json").validate(data);
    unsigned int id;
    AccessState(data, [&](const GameRecord &gameRecord, StateRecord &stateRecord) {
        auto actionGenerator =
            ActionGenerator::Create(data["type"], *gameRecord.GamePtr, *stateRecord.StatePtr, data["data"]);
        auto actionGeneratorData = ActionGenerator::Data::Create(*actionGenerator);
        id = stateRecord.SubActionGenerators.Emplace(std::move(actionGenerator), std::move(actionGeneratorData));
    });
    return {{"actionGeneratorID", id}};
}

nlohmann::json Server::RemoveGame(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/remove_game.schema.json").validate(data);
    m_GameMap.Erase(data["gameID"]);
    return nlohmann::json::object();
}

nlohmann::json Server::RemoveState(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/remove_state.schema.json").validate(data);
    AccessGame(data, [&](GameRecord &gameRecord) { gameRecord.SubStates.Erase(data["stateID"]); });
    return nlohmann::json::object();
}

nlohmann::json Server::RemovePlayer(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/remove_player.schema.json").validate(data);
    AccessState(data,
                [&](const GameRecord &, StateRecord &stateRecord) { stateRecord.SubPlayers.Erase(data["playerID"]); });
    return nlohmann::json::object();
}

nlohmann::json Server::RemoveActionGenerator(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/remove_action_generator.schema.json").validate(data);
    AccessState(data, [&](const GameRecord &, StateRecord &stateRecord) {
        stateRecord.SubActionGenerators.Erase(data["actionGeneratorID"]);
    });
    return nlohmann::json::object();
}

nlohmann::json Server::GenerateActions(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/generate_actions.schema.json").validate(data);
    nlohmann::json actions;
    AccessActionGenerator(data, [&](const GameRecord &, const StateRecord &stateRecord,
                                    const ActionGeneratorRecord &actionGeneratorRecord) {
        const auto &actionGenerator = *actionGeneratorRecord.ActionGeneratorPtr;
        const auto &actionGeneratorData = *actionGeneratorRecord.ActionGeneratorDataPtr;
        // Always lock state before locking player or action generator
        const std::shared_lock lockState(stateRecord.MtxState);
        const std::shared_lock lockActionGenerator(actionGeneratorRecord.MtxActionGeneratorData);
        actionGenerator.ForEach(actionGeneratorData,
                                [&](const Action &action) { actions.push_back(action.GetJson()); });
    });
    return {{"actions", actions}};
}

nlohmann::json Server::TakeAction(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/take_action.schema.json").validate(data);
    std::optional<std::vector<double>> result;
    nlohmann::json stateJson;
    AccessState(data, [&](const GameRecord &gameRecord, const StateRecord &stateRecord) {
        const auto &game = *gameRecord.GamePtr;
        auto &state = *stateRecord.StatePtr;
        const auto action = Action::Create(game, data["action"]);
        if (!game.IsValidAction(state, *action))
            throw std::invalid_argument("The action is invalid");
        const std::scoped_lock lock(stateRecord.MtxState);
        result = game.TakeAction(state, *action);
        stateJson = state.GetJson();
        // Concurrently update players and action generators,
        // do not use tbb::parallel_invoke as those functions are most likely not CPU intensive
        std::thread threadUpdatePlayers([&]() {
            stateRecord.SubPlayers.ForEachParallel([&](const PlayerRecord &playerRecord) {
                const std::scoped_lock lock(playerRecord.MtxPlayer);
                playerRecord.PlayerPtr->Update(*action);
            });
        });
        std::thread threadUpdateActionGenerators([&]() {
            stateRecord.SubActionGenerators.ForEachParallel([&](const ActionGeneratorRecord &actionGeneratorRecord) {
                const std::scoped_lock lock(actionGeneratorRecord.MtxActionGeneratorData);
                actionGeneratorRecord.ActionGeneratorPtr->Update(*actionGeneratorRecord.ActionGeneratorDataPtr,
                                                                 *action);
            });
        });
        threadUpdatePlayers.join();
        threadUpdateActionGenerators.join();
    });
    nlohmann::json response = {
        {"finished", result.has_value()},
        {"state", std::move(stateJson)},
    };
    if (result)
        response["result"] = std::move(*result);
    return response;
}

nlohmann::json Server::StartThinking(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/start_thinking.schema.json").validate(data);
    AccessPlayer(data, [&](const GameRecord &, const StateRecord &stateRecord, const PlayerRecord &playerRecord) {
        // Always lock state before locking player or action generator
        const std::shared_lock lockState(stateRecord.MtxState);
        const std::scoped_lock lockPlayer(playerRecord.MtxPlayer);
        playerRecord.PlayerPtr->StartThinking();
    });
    return nlohmann::json::object();
}

nlohmann::json Server::StopThinking(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/stop_thinking.schema.json").validate(data);
    AccessPlayer(data, [&](const GameRecord &, const StateRecord &stateRecord, const PlayerRecord &playerRecord) {
        // Always lock state before locking player or action generator
        const std::shared_lock lockState(stateRecord.MtxState);
        const std::scoped_lock lockPlayer(playerRecord.MtxPlayer);
        playerRecord.PlayerPtr->StopThinking();
    });
    return nlohmann::json::object();
}

nlohmann::json Server::GetBestAction(const nlohmann::json &data) {
    Util::GetJsonValidator("requests/get_best_action.schema.json").validate(data);
    std::optional<std::chrono::duration<double>> time;
    if (data.contains("maxThinkTime"))
        time = std::chrono::duration<double>(data["maxThinkTime"]);
    std::unique_ptr<Action> bestAction;
    AccessPlayer(data, [&](const GameRecord &, const StateRecord &stateRecord, const PlayerRecord &playerRecord) {
        // Always lock state before locking player or action generator
        const std::shared_lock lockState(stateRecord.MtxState);
        const std::scoped_lock lock(playerRecord.MtxPlayer);
        bestAction = playerRecord.PlayerPtr->GetBestAction(time);
    });
    return {{"action", bestAction->GetJson()}};
}

nlohmann::json Server::QueryDetails(const nlohmann::json &) {
    // TODO
    return {};
}
