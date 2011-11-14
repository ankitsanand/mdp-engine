#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include <limits>

#define  DISCOUNT  .95

#include "parsing.h"
#include "algorithm.h"
#include "parameters.h"
#include "heuristic.h"

#include "policy.h"
#include "rollout.h"
#include "mcts.h"
#include "dispatcher.h"

struct state_t {
    int current_;
    unsigned long known_;
    unsigned long blocked_;

  public:
    state_t(int current = -1) : current_(current), known_(0), blocked_(0) { }
    state_t(const state_t &s) : current_(s.current_), known_(s.known_), blocked_(s.blocked_) { }
    ~state_t() { }

    size_t hash() const { return current_ + (known_ ^ blocked_); }

    bool known(int e) const {
        int mask = 1 << e;
        return known_ & mask;
    }
    bool traversable(int e) const {
        int mask = 1 << e;
        return (blocked_ & mask) != 0 ? false : true;
    }
    void set(int e, int status) {
        int mask = 1 << e;
        known_ |= mask;
        if( status == 1 )
            blocked_ |= mask;
        else
            blocked_ &= ~mask;
    }

    const state_t& operator=(const state_t &s) {
        current_ = s.current_;
        known_ = s.known_;
        blocked_ = s.blocked_;
        return *this;
    }
    bool operator==(const state_t &s) const {
        return (current_ == s.current_) && (known_ == s.known_) && (blocked_ == s.blocked_);
    }
    bool operator!=(const state_t &s) const {
        return (current_ != s.current_) || (known_ != s.known_) || (blocked_ != s.blocked_);
    }
    bool operator<(const state_t &s) const {
        return (current_ < s.current_) ||
               ((current_ == s.current_) && (known_ < s.known_)) ||
               ((current_ == s.current_) && (known_ == s.known_) && (blocked_ == s.blocked_));
    }
    void print(std::ostream &os) const {
        os << "("
           << current_
           << "," << known_
           << "," << blocked_
           << ")";
    }
};

inline std::ostream& operator<<(std::ostream &os, const state_t &s) {
    s.print(os);
    return os;
}

bool cmp_function(const std::pair<state_t, float> &p1, const std::pair<state_t, float> &p2) {
    return p1.second > p2.second;
}

class problem_t : public Problem::problem_t<state_t> {
  public:
    const CTP::graph_t &graph_;
    const state_t init_;
    int start_, goal_;

  public:
    problem_t(CTP::graph_t &graph) : graph_(graph), init_(-1), start_(0), goal_(graph_.num_nodes_ - 1) { }
    virtual ~problem_t() { }

    virtual Problem::action_t number_actions(const state_t &s) const {
        return s.current_ == -1 ? 1 : graph_.at_[s.current_].size();
    }
    virtual bool applicable(const state_t &s, Problem::action_t a) const {
        return ((s.current_ == -1) && (a == 0)) ||
               ((s.current_ != -1) && s.traversable(graph_.at_[s.current_][a]));
    }
    virtual const state_t& init() const { return init_; }
    virtual bool terminal(const state_t &s) const { return s.current_ == goal_; }
    virtual float cost(const state_t &s, Problem::action_t a) const {
        return s.current_ == -1 ? 0 : graph_.cost(graph_.at_[s.current_][a]);
    }
    virtual void next(const state_t &s, Problem::action_t a, std::vector<std::pair<state_t, float> > &outcomes) const {
        ++expansions_;
        outcomes.clear();

        //std::cout << "next" << s << " w a=" << a << " is:" << std::endl;

        int to_node = -1;
        if( s.current_ == -1 ) {
            to_node = start_;
        } else {
            // determine node at the other side of the edge (action)
            int e = graph_.at_[s.current_][a];
            to_node = graph_.to(e) == s.current_ ? graph_.from(e) : graph_.to(e);
            assert(to_node != s.current_);
        }

        // collect edges adjacent at to_node of unknown status
        int k = 0;
        std::vector<int> unknown_edges;
        unknown_edges.reserve(graph_.num_edges_);
        for( int i = 0, isz = graph_.at_[to_node].size(); i < isz; ++i ) {
            int e = graph_.at_[to_node][i];
            if( !s.known(e) ) {
                unknown_edges.push_back(e);
                ++k;
            }
        }

        // generate subsets of unknowns edges and update weathers
        outcomes.reserve(1 << k);
        for( int i = 0, isz = 1 << k; i < isz; ++i ) {
            state_t next(s);
            float p = 1;
            int subset = i;
            for( int j = 0; j < k; ++j ) {
                int e = unknown_edges[j];
                p *= (subset & 1) ? 1 - graph_.prob(e) : graph_.prob(e);
                next.set(e, subset & 1);
                subset = subset >> 1;
            }
            next.current_ = to_node;
            if( p > 0 ) {
                //std::cout << "    " << next << " w.p. " << p << std::endl;
                outcomes.push_back(std::make_pair(next, p));
            }
        }
    }
    virtual void print(std::ostream &os) const { }
};

inline std::ostream& operator<<(std::ostream &os, const problem_t &p) {
    p.print(os);
    return os;
}

