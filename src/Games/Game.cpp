#include "Game.hpp"
#include "Gobang/Game.hpp"
#include "TicTacToe/Game.hpp"
#include <unordered_map>

template <typename T>
static std::unique_ptr<Game> CreateGame(const nlohmann::json &data) {
    return std::make_unique<T>(data);
}

using GameCreatorFunc = std::unique_ptr<Game> (*)(const nlohmann::json &);
static const std::unordered_map<std::string, GameCreatorFunc> GameCreatorMap = {
    {"tic_tac_toe", CreateGame<tic_tac_toe::Game>},
    {"gobang", CreateGame<gobang::Game>},
};

std::unique_ptr<Game> Game::Create(const std::string &type, const nlohmann::json &data) {
    Util::GetJsonValidator("games/" + type + ".schema.json").validate(data);
    const auto creator = GameCreatorMap.at(type);
    return creator(data);
}
