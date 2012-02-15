#ifndef AGENT_H
#define AGENT_H

#include "wumpus_belief.h"
#include <cassert>
#include <iostream>
#include <queue>
#include <stdlib.h>
#include <limits.h>

#define MOVE       0
#define TURNR      1
#define TURNL      2
#define SHOOT      3
#define GRAB       4
#define EXIT       5

#define NORTH      0
#define EAST       1
#define SOUTH      2
#define WEST       3

#define UNKNOWN    -1
#define HAVE_GOLD  -2
#define OUTSIDE    -1

#define GLITTER    1
#define BREEZE     2
#define STENCH     4

const char* heading_string[] = { "north", "east", "south", "west" };

class state_t {
  protected:
    int rows_;
    int cols_;

    bool alive_;
    int pos_;
    int heading_;
    int gold_;

    int npits_;
    int nwumpus_;
    int narrows_;

    wumpus_belief_t belief_;

  public:
    state_t(int rows, int cols, int npits, int nwumpus, int narrows)
      : rows_(rows), cols_(cols),
        alive_(true), pos_(0), heading_(NORTH), gold_(UNKNOWN),
        npits_(npits), nwumpus_(nwumpus), narrows_(narrows) {
    }
    explicit state_t(const state_t &state)
      : rows_(state.rows_), cols_(state.cols_),
        alive_(state.alive_), pos_(state.pos_),
        heading_(state.heading_), gold_(state.gold_),
        npits_(state.npits_), nwumpus_(state.nwumpus_),
        narrows_(state.narrows_), belief_(state.belief_) {
    }
    ~state_t() { }

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    int ncells() const { return rows_ * cols_; }

    bool inconsistent() const { return belief_.inconsistent(); }
    bool alive() const { return alive_; }
    bool dead() const { return !alive_; }
    int pos() const { return pos_; }
    int heading() const { return heading_; }
    int gold() const { return gold_; }
    bool have_gold() const { return gold_ == HAVE_GOLD; }
    bool in_cave() const { return pos_ != OUTSIDE; }

    int npits() const { return npits_; }
    int nwumpus() const { return nwumpus_; }
    int narrows() const { return narrows_; }

    bool hazard_at(int cell) const {
        return belief_.hazard_at(cell);
    }
    bool no_hazard_at(int cell) const {
        return belief_.no_hazard_at(cell);
    }

    void set_as_unknown() {
        belief_.set_as_unknown();
    }

    int target_cell(int action) const {
        if( action != MOVE ) {
            return pos_;
        } else {
            int npos = pos_;
            int row = pos_ / cols_;
            int col = pos_ % cols_;
            if( heading_ == NORTH ) {
                if( row < rows_ - 1 ) npos = (row + 1) * cols_ + col;
            } else if( heading_ == EAST ) {
                if( col < cols_ - 1 ) npos = row * cols_ + col + 1;
            } else if( heading_ == SOUTH ) {
                if( row > 0 ) npos = (row - 1) * cols_ + col;
            } else {
                if( col > 0 ) npos = row * cols_ + col - 1;
            }
            return npos;
        }
    }

    bool applicable(int action) const {
        if( pos_ == OUTSIDE ) return false;
        if( (action == MOVE) || (action == TURNR) || (action == TURNL) ) {
            return true;
        } else if( action == SHOOT ) {
            return narrows_ > 0;
        } else if( action == GRAB ) {
            return pos_ == gold_;
        } else {
            assert(action == EXIT);
            return pos_ == 0;
        }
    }

    void apply(int action) {
        assert(applicable(action));
        if( action == MOVE ) {
            pos_ = target_cell(action);
            assert((0 <= pos_) && (pos_ < rows_ * cols_));
        } else if( (action == TURNR) || (action == TURNL) ) {
            heading_ += action == TURNR ? 1 : 5;
            heading_ = heading_ % 4;
        } else if( action == SHOOT ) {
            assert(0);
        } else if( action == GRAB ) {
            assert(pos_ == gold_);
            gold_ = HAVE_GOLD;
        } else {
            assert(action == EXIT);
            assert(pos_ == 0);
            pos_ = OUTSIDE;
        }
    }

    void update(int obs) {
        if( pos_ != OUTSIDE ) {
            if( obs & GLITTER ) {
                gold_ = pos_;
            }
            if( obs & BREEZE ) {
                belief_.pit_filter(pos_);
            } else {
                belief_.pit_filter(pos_, 0, false);
            }
            if( obs & STENCH ) {
                belief_.wumpus_filter(pos_);
            } else {
                belief_.wumpus_filter(pos_, 0, false);
            }
        }
    }

    void apply_action_and_update(int action, int obs) {
        apply(action);
        update(obs);
    }

    void print(std::ostream &os) const {
        os << "pos=(" << (pos_ % cols_) << "," << (pos_ / cols_) << ")"
           << ", heading=" << heading_string[heading_]
           << ", gold=" << gold_
           << ", alive=" << alive_
           << std::endl;
        os << belief_;
    }
};


struct open_list_cmp {
    bool operator()(const std::pair<int, int> &p1, const std::pair<int, int> &p2) {
        return p1.second > p2.second;
    }
};

struct base_policy_t {
    int rows_;
    int cols_;
    mutable std::vector<int> distances_;

    base_policy_t(int rows, int cols) : rows_(rows), cols_(cols) {
    }
    ~base_policy_t() { }

    int operator()(const state_t &state) const {
        if( state.have_gold() ) {
            // if have goal and at entry, just EXIT
            if( state.pos() == 0 ) return EXIT;

            // must go home (outside cave)
            compute_distances(state, 0, true);
            int min_dist = INT_MAX, best = -1;
            for( int p = 0; p < rows_ * cols_; ++p ) {
                if( adjacent(p, state.pos()) && (distances_[p] < min_dist) ) {
                    min_dist = distances_[p];
                    best = p;
                }
            }
            assert(min_dist < INT_MAX);
            assert(distances_[state.pos()] == 1 + distances_[best]);

            // move if right heading, else turn around
            if( state.heading() == heading(state.pos(), best) ) {
                return MOVE;
            } else {
                return TURNR;
            }
        } else {
            // move around safely
            std::vector<int> actions;
            actions.reserve(6);
            for( int action = 0; action <= EXIT; ++action ) {
                if( state.applicable(action) ) {
                    int target_cell = state.target_cell(action);
                    if( state.no_hazard_at(target_cell) )
                        actions.push_back(action);
                }
            }
            int action = actions[lrand48() % actions.size()];
            assert(state.no_hazard_at(state.target_cell(action)));
            return action;
        }
    }

    void compute_distances(const state_t &state, int goal, bool safe) const {
        distances_ = std::vector<int>(rows_ * cols_, INT_MAX);

        std::priority_queue<std::pair<int, int>,
                            std::vector<std::pair<int, int> >,
                            open_list_cmp> queue;

        distances_[goal] = 0;
        queue.push(std::make_pair(goal, 0));
        while( !queue.empty() ) {
            std::pair<int, int> p = queue.top();
            queue.pop();
            if( p.second <= distances_[p.first] ) {
                int cell = p.first;
                int row = cell / cols_, col = cell % cols_;
                for( int dr = -1; dr < 2; ++dr ) {
                    if( (row + dr < 0) || (row + dr >= rows_) ) continue;
                    for( int dc = -1; dc < 2; ++dc ) {
                        if( (dr != 0) && (dc != 0) ) continue;
                        if( (col + dc < 0) || (col + dc >= cols_) ) continue;
                        int ncell = (row + dr) * cols_ + (col + dc);
                        if( state.hazard_at(ncell) ) continue;
                        if( safe && !state.no_hazard_at(ncell) ) continue;

                        int cost = 1 + p.second;
                        if( cost < distances_[ncell] ) {
                            distances_[ncell] = cost;
                            queue.push(std::make_pair(ncell, cost));
                        }
                    }
                }
            }
        }
        //std::cout << "distances:";
        //for( int p = 0; p < rows_ * cols_; ++p )
        //    std::cout << " " << distances_[p];
        //std::cout << std::endl;
    }

    int heading(int from, int to) const {
        int from_row = from / cols_, from_col = from % cols_;
        int to_row = to / cols_, to_col = to % cols_;
        assert((from_row == to_row) || (from_col == to_col));
        if( from_row != to_row ) {
            return from_row < to_row ? NORTH : SOUTH;
        } else {
            return from_col < to_col ? EAST : WEST;
        }
    }

    bool adjacent(int pos1, int pos2) const {
        int row1 = pos1 / cols_, col1 = pos1 % cols_;
        int row2 = pos2 / cols_, col2 = pos2 % cols_;
        if( (row1 != row2) && (col1 != col2) ) return false;
        return (abs(row1 - row2) == 1) || (abs(col1 - col2) == 1);
    }

};

#endif

