#include "Default.hpp"
#include "../../../Utilities/Utilities.hpp"
#include "../../Game.hpp"
#include <cassert>

namespace tic_tac_toe::action_generator {
Default::Default(const ::Game &, const ::State &state, const nlohmann::json &data)
    : _State(static_cast<const State *>(&state)) {
    Util::GetJsonValidator("action_generators/tic_tac_toe/default.schema.json").validate(data);
}

std::unique_ptr<::Action> Default::FirstAction(const ::ActionGenerator::Data &data) const {
    auto action = std::make_unique<Action>(0, -1);
    const auto isValid = NextAction(data, *action);
    assert(isValid);
    return action;
}
} // namespace tic_tac_toe::action_generator
