#pragma once

#include "../../Games/ActionGenerator.hpp"
#include "../../Games/Game.hpp"
#include "../Player.hpp"
#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>

namespace random_move {
class Player : public ::Player {
private:
    const State *_State;
    std::unique_ptr<ActionGenerator> _ActionGenerator;
    std::unique_ptr<ActionGenerator::Data> _ActionGeneratorData;

public:
    explicit Player(const Game &game, const State &state, const nlohmann::json &data);

    virtual std::unique_ptr<Action> GetBestAction(std::optional<std::chrono::duration<double>> maxThinkTime) override;
};
} // namespace random_move