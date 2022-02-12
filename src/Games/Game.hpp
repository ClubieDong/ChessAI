#pragma once

#include "../Utilities/Helpers.hpp"
#include <memory>
#include <nlohmann/json.hpp>

class Game;

class State : public ClonableEqualable<State> {
public:
    virtual nlohmann::json GetJson() const = 0;

    static std::unique_ptr<State> Create(const Game &game);
    static std::unique_ptr<State> Create(const Game &game, const nlohmann::json &data);
};

class Action : public ClonableEqualable<Action> {
public:
    virtual nlohmann::json GetJson() const = 0;

    static std::unique_ptr<Action> Create(const Game &game, const nlohmann::json &data);
};

class Game : public NonCopyableNonMoveable {
public:
    virtual ~Game() = default;
    virtual bool IsValidAction(const State &state, const Action &action) const = 0;
    virtual unsigned int GetNextPlayer(const State &state) const = 0;
    // TODO: Small vector optimization
    virtual std::optional<std::vector<double>> TakeAction(State &state, const Action &action) const = 0;

    static std::unique_ptr<Game> Create(const std::string &type, const nlohmann::json &data);
};
