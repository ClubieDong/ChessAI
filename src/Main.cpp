#include "Controller.hpp"
#include "Chesses/Gobang/Gobang.hpp"
#include "Chesses/Gobang/ActGens/NeighborActGen.hpp"
#include "Players/HumanPlayer/HumanPlayer.hpp"
#include "Players/MCTS/ParallelMCTS.hpp"
#include "Utilities/Literals.hpp"

int main()
{
    using Game = Gobang<15, 5>;
    using Player1 = HumanPlayer<Game>;
    using Player2 = ParallelMCTS<Game, gobang::NeighborActGen<Game>, 10_sec, 15_GB, 12>;
    Controller<Game, Player1, Player2> controller;
    auto result = controller.Start();
    std::cout << "Game over! " << result[0] << ':' << result[1] << '\n';
    return 0;
}
