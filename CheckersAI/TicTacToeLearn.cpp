// CheckersAI.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdlib.h>
#include <cinttypes>
//#include <gsl/gsl>
#include <iostream>
#include <map>
#include <random>
#include <array>
#include <numeric>
#include <boost/timer/timer.hpp>

using namespace std;

namespace pieces
{
    // Bit masks
    using piece_mask_t = uint32_t;
    const piece_mask_t empty = 0x00000000;
    const piece_mask_t X = 0x00000001;
    const piece_mask_t O = 0x00000002;

    const char symbol[] = { ' ', 'X', 'O' };

    constexpr inline bool isX(uint32_t val)
    {
        return (val & X) != 0;
    }

    constexpr inline bool isY(uint32_t val)
    {
        return (val & O) != 0;
    }

    constexpr inline bool isEmpty(uint32_t val)
    {
        return (val & empty) != 0;
    }
}

struct BoardState
{
    static const size_t initial_weight = 100000;
    BoardState() :
        positions(0)
    {
        fill(begin(weights), end(weights), initial_weight);
    }

    BoardState(uint32_t positions) :
        positions(positions)
    {
        // set weight to zero if board position is occupied.
        int position = 0;
        generate(begin(weights), end(weights),
                 [&]() {
                    int w = 0;
                    if (getPos(position) != pieces::empty)
                        w = 0;
                    else
                        w = initial_weight;
                    ++position;
                    return w;
                 });
    }

    uint32_t positions = 0;
    array<size_t, 9> weights;

    uint32_t getPos(size_t pos)
    {
        // return last 2 bits
        auto result = positions >> position_bit_shift[pos];
        result &= 0x00000003;
        return result;
    }

    uint32_t setPos(size_t pos, pieces::piece_mask_t mask)
    {
        uint32_t clearing_mask = 0xFFFFFFFC;

        clearing_mask = _rotl(clearing_mask, position_bit_shift[pos]);
        positions &= clearing_mask;
        positions |= (mask << position_bit_shift[pos]);
        return positions;
    }

    bool isEmpty()
    {
        return positions == 0;
    }

    bool isFull()
    {
        auto temp = positions;
        if (temp == 0)
            return false;

        // shift positions until a position is zero.
        int count = 0;
        while (count < 9)
        {
            if ((temp & 0x03) == 0)
            {
                return false;
            }
            // each position is 2 bits.
            temp >>= 2;
            count++;
        }

        // No zeros.
        return true;
    }

    bool isTie()
    {
        return getWin() == pieces::empty && isFull();
    }

    pieces::piece_mask_t getWin()
    {
        for (int i = 0; i < 8; ++i)
        {
            auto p1 = getPos(winpos[i][0]);
            auto p2 = getPos(winpos[i][1]);
            auto p3 = getPos(winpos[i][2]);
            if (p1 == p2 &&
                p2 == p3 &&
                p3 != pieces::empty)
            {
                return getPos(winpos[i][0]);
            }
        }
        return pieces::empty;
    }

    void print()
    {
        int i = 0;
        cout << "----------\n";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << "\n";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << "\n";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << ", ";
        cout << pieces::symbol[getPos(i++)] << "\n";
        cout << "----------\n";

    }

    size_t pickMove()
    {
        size_t sum = std::accumulate(begin(weights), end(weights),
                                static_cast<size_t>(0));

        // Pick a number between 0 and sum as penetration into cummulative distribution function
        // of weighted values.
        //static std::random_device seeder;
        //static std::mt19937 rng(seeder());
        //std::uniform_int_distribution<size_t> gen(1ul, sum); // uniform, unbiased
        //size_t penetration_point = gen(rng);

        size_t penetration_point = sum / 2;
        size_t accum = penetration_point;

        size_t board_pos = 0;
        while (accum > weights[board_pos])
        {
            accum -= weights[board_pos];
            ++board_pos;
        }
        //assert(weights[board_pos] > 0);


        return board_pos;
    }

    size_t pickBestMove()
    {
        size_t pos = 0;
        size_t heigh_weight = weights[pos];
        for (int i = 0; i < weights.size(); ++i)
        {
            if (weights[i] > heigh_weight)
            {
                pos = i;
                heigh_weight = weights[i];
            }
        }

        return pos;
    }

    inline static const uint32_t position_bit_shift[9] = { 0, 2, 4, 6, 8, 10, 12, 14, 16 };
    inline static const size_t winpos[][3] =
    {
    { 0,1,2 },
    { 3,4,5 },
    { 6,7,8 },
    { 0,3,6 },
    { 1,4,7 },
    { 2,5,8 },
    { 0,4,8 },
    { 2,4,6 }
    };
};

map<uint32_t, BoardState> states_and_weights;

struct Move
{
    // Structure to keep track of moves made during a game.
    // Pointer to the BoardState and its move weights.
    decltype(states_and_weights)::iterator move_state;
    // Move that was taken.
    size_t move_pos;
};

void
computeAI()
{
    boost::timer::auto_cpu_timer t;

    uint64_t tmoves = 0;
    vector<Move> moves;
    moves.reserve(9);
    BoardState board;

    for (size_t i = 0; i < 2000000; ++i)
    {
        moves.clear();
        board.positions = 0;

        pieces::piece_mask_t turn = pieces::X;
        pieces::piece_mask_t winner = pieces::empty;

        int nmoves = 9;
        while (nmoves != 0 &&
              (winner == pieces::empty))
        {
            tmoves++;
            // Search for move weights for current board state.
            auto move_state = states_and_weights.find(board.positions);
            if (move_state == end(states_and_weights))
            {
                // Create a new state.
                move_state = states_and_weights.insert(
                    begin(states_and_weights), 
                    make_pair(board.positions, BoardState(board.positions)));
            }

            // pick a move and update the board.
            size_t move_pos = move_state->second.pickMove();
            moves.push_back(Move{ move_state, move_pos });
            board.setPos(move_pos, turn);

            turn ^= 0x03;
            winner = board.getWin();
            nmoves--;
        }

        // adjust weights
        if (winner != pieces::empty)
        {
            int adjustment = 10;
            if (winner == pieces::O)
                adjustment = -10;
            for (auto& move : moves)
            {
                auto& weight = move.move_state->second.weights[move.move_pos];
                weight += adjustment;
                weight = std::clamp(weight, static_cast<size_t>(1), static_cast<size_t>(100000));
                adjustment *= -1;
            }
        }
        else
        {
            // Getting a tie is a little better than losing?
            int adjustment = 1;
            for (auto& move : moves)
            {
                auto& weight = move.move_state->second.weights[move.move_pos];
                weight += adjustment;
                weight = std::clamp(weight, static_cast<size_t>(1), static_cast<size_t>(100000));
            }
        }
    }
    cout << "Tmoves: " << tmoves << "\n";
}

void
letComputerPlay()
{
    size_t first_move_pos = 0;
    while (first_move_pos < 9)
    {

        cout << "================================================";

        BoardState board;

        pieces::piece_mask_t turn = pieces::X;
        pieces::piece_mask_t winner = pieces::empty;

        size_t turn_number = 0;
        bool first_move = true;
        while (!board.isFull() &&
               winner == pieces::empty)
        {
            // Search for move weights for current board state.
            auto move_state = states_and_weights.find(board.positions);
            if (move_state == end(states_and_weights))
            {
                // Create a new state
                move_state = states_and_weights.insert(begin(states_and_weights), make_pair(board.positions, BoardState(board.positions)));
            }

            // pick a move and update the board.
            size_t move_pos;
            if(first_move)
                move_pos = first_move_pos;
            else
                move_pos = move_state->second.pickBestMove();
            board.setPos(move_pos, turn);
            board.print();
            turn ^= 0x03;
            first_move = false;
            winner = board.getWin();
        }
        cout << "================================================";
        first_move_pos++;
    }
}

int main()
{
    computeAI();
    cout << states_and_weights.size() << "\n";

    //letComputerPlay();

    //do
    //{
    //cout << "Select [X,O]?";
    //char player_input;
    //cin >> player_input;

    //if(player_input == 'x' 
    //}while(

    return 0;
}

