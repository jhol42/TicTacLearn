// CheckersAI.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdlib.h>
#include <cinttypes>
#include <gsl/gsl>
#include <iostream>
#include <map>
#include <random>
#include <array>

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
    static const int initial_weight = 500;
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
                 [&]()
        {
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
    array<int, 9> weights;

    uint32_t getPos(size_t pos)
    {
        // return last 2 bits
        auto result = positions >> shift[pos];
        result &= 0x00000003;
        return result;
    }

    uint32_t setPos(size_t pos, pieces::piece_mask_t mask)
    {
        uint32_t clearing_mask = 0xFFFFFFFC;

        clearing_mask = _rotl(clearing_mask, shift[pos]);
        positions &= clearing_mask;
        positions |= (mask << shift[pos]);
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
        while (temp != 0)
        {
            if ((temp & 0x03) == 0)
            {
                return false;
            }
            temp >>= 2;
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
            if ( p1 == p2 && 
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

    int pickMove()
    {
        int sum = accumulate(begin(weights), end(weights), 0);

        // Pick a number between 0 and sum

        std::random_device seeder;
        std::mt19937 rng(seeder());
        std::uniform_int_distribution<int> gen(1, sum); // uniform, unbiased
        int r = gen(rng);
        int rc = r;

        int pos = 0;
        rc -= weights[0];
        while (rc > 0)
        {
            ++pos;
            rc -= weights[pos];
        }
        assert(weights[pos] >  0);


        return pos;
    }

    int pickBestMove()
    {
        int pos = 0;
        int heigh_weight = weights[pos];
        for(int i = 0; i < weights.size(); ++i)
        {
            if(weights[i] > heigh_weight)
            {
                pos = i;
                heigh_weight = weights[i];
            }
        }

        return pos;
    }

    inline static const uint32_t shift[9] = { 0, 2, 4, 6, 8, 10, 12, 14, 16 };
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

map<uint32_t, BoardState> states;

struct Move
{
    decltype(states)::iterator move_state;
    size_t move_pos;
};


int main()
{

    for(size_t i = 0; i < 1024*1024; ++i)
    {
        vector<Move> moves;
        moves.reserve(9);
        BoardState board;

        pieces::piece_mask_t turn = pieces::X;
        pieces::piece_mask_t winner = pieces::empty;

        size_t turn_number = 0;
        while (!board.isFull() &&
            ((winner = board.getWin()) == pieces::empty))
        {
            // Search for move weights for current board state.
            auto move_state = states.find(board.positions);
            if (move_state == end(states))
            {
                // Create a new state
                move_state = states.insert(begin(states), make_pair(board.positions, BoardState(board.positions)));
            }

            // pick a move and update the board.
            size_t move_pos = move_state->second.pickMove();
            moves.push_back(Move{ move_state, move_pos });
            board.setPos(move_pos, turn);
            turn ^= 0x03;
        }

        //cout << "Training.\n" << "Ties: " << ties << "\n" << "X: " << x_wins << "\n" << "O: " << o_wins << "\n\n";

        //board.print();
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
                weight = std::clamp(weight, 1, 10000);
                adjustment *= -1;
            }
        }
    }

    size_t x_wins = 0;
    size_t o_wins = 0;
    size_t ties = 0;

    for(int i = 0; i < 100; ++i)
    {

        BoardState board;

        pieces::piece_mask_t turn = pieces::X;
        pieces::piece_mask_t winner = pieces::empty;

        size_t turn_number = 0;
        while (!board.isFull() &&
            ((winner = board.getWin()) == pieces::empty))
        {
            // Search for move weights for current board state.
            auto move_state = states.find(board.positions);
            if (move_state == end(states))
            {
                // Create a new state
                move_state = states.insert(begin(states), make_pair(board.positions, BoardState(board.positions)));
            }

            // pick a move and update the board.
            size_t move_pos = move_state->second.pickBestMove();
            board.setPos(move_pos, turn);
            turn ^= 0x03;
        }

        if (winner == pieces::O)
        {
            o_wins++;
        }
        else if (winner == pieces::X)
        {
            x_wins++;
        }
        else
        {
            ties++;
        }
        
        board.print();
        cout << "Ties: " << ties << "\n" << "X: " << x_wins << "\n" << "O: " << o_wins << "\n\n";
    }

    //while(true)
    //{

    //    BoardState board;

    //    pieces::piece_mask_t turn = pieces::X;
    //    pieces::piece_mask_t winner = pieces::empty;

    //    size_t turn_number = 0;
    //    while (!board.isFull() &&
    //        ((winner = board.getWin()) == pieces::empty))
    //    {
    //        int ppos = 0;
    //        board.print();

    //        do
    //        {
    //            cout << "Enter Move: ";
    //            cin >> ppos;
    //        }while(board.getPos(ppos) != pieces::empty)



    //        // Search for move weights for current board state.
    //        auto move_state = states.find(board.positions);
    //        if (move_state == end(states))
    //        {
    //            // Create a new state
    //            move_state = states.insert(begin(states), make_pair(board.positions, BoardState(board.positions)));
    //        }

    //        // pick a move and update the board.
    //        size_t move_pos = move_state->second.pickBestMove();
    //        board.setPos(move_pos, turn);
    //        turn ^= 0x03;
    //    }

    //    if (winner == pieces::O)
    //    {
    //        o_wins++;
    //    }
    //    else if (winner == pieces::X)
    //    {
    //        x_wins++;
    //    }
    //    else
    //    {
    //        ties++;
    //    }

    //}

    // While not converted
        // While no win and not draw
        //  get board state
        //  SHA1 board state
        //  Look in database for weighted moves.
        //  if not there
        //      Create board state with default weights.
        //      Insert into database
        //  Select move based on weights
        //  Save SHA1 and which move to list of moves for this player.
        //  Switch player

        //  For losing player
        //      for each sha1 in player list decrease weight of selected move
        //      update database
        //      
        //  For winding player
        //      for each sha1 in player list increase weight of selected move
        //      update database
        //  If draw, do nothing.

    return 0;
}

