/*
 *  Copyright (C) 2011 Universidad Simon Bolivar
 * 
 *  Permission is hereby granted to distribute this software for
 *  non-commercial research purposes, provided that this copyright
 *  notice is included with any such distribution.
 *  
 *  THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 *  EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.  THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
 *  SOFTWARE IS WITH YOU.  SHOULD THE PROGRAM PROVE DEFECTIVE, YOU
 *  ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION.
 *  
 *  Blai Bonet, bonet@ldc.usb.ve
 *
 */

#ifndef AOT_H
#define AOT_H

#include "policy.h"
#include "bdd_priority_queue.h"

#include <iostream>
#include <cassert>
#include <limits>
#include <vector>
#include <queue>

#define USE_BDD_PQ
//#define DEBUG

// TODO: implement bounded priority queue

namespace Policy {

namespace AOT {

template<typename T> class aot_t; // Forward reference

////////////////////////////////////////////////
//
// AND/OR Tree
//

template<typename T> struct node_t {
    float value_;
    float delta_;
    unsigned nsamples_;
    bool in_best_policy_;
    bool in_queue_;
    bool in_pq_;

    node_t(float value = 0, float delta = 0)
      : value_(value), delta_(delta), nsamples_(0),
        in_best_policy_(false), in_queue_(false), in_pq_(false) { }
    virtual ~node_t() { }

    virtual void print(std::ostream &os, bool indent = true) const = 0;
    virtual void expand(const aot_t<T> *policy,
                        std::vector<node_t<T>*> &to_propagate) = 0;
    virtual void propagate(const aot_t<T> *policy) = 0;
};

template<typename T> struct state_node_t;
template<typename T> struct action_node_t : public node_t<T> {
    Problem::action_t action_;
    float action_cost_;
    state_node_t<T> *parent_;
    std::vector<std::pair<float, state_node_t<T>*> > children_;

    action_node_t(Problem::action_t action) : action_(action) { }
    virtual ~action_node_t() { }

    bool is_leaf() const { return children_.empty(); }
    void update_value(float discount) {
        node_t<T>::value_ = 0;
        for( unsigned i = 0, isz = children_.size(); i < isz; ++i ) {
            node_t<T>::value_ +=
              children_[i].first * children_[i].second->value_;
        }
        node_t<T>::value_ =
          action_cost_ + discount * node_t<T>::value_;
    }

    virtual void print(std::ostream &os, bool indent = true) const {
        if( indent ) os << std::setw(2 * parent_->depth_) << "";
        os << "[action=" << action_
           << ",value=" << node_t<T>::value_
           << ",delta=" << node_t<T>::delta_
           << "]";
    }
    virtual void expand(const aot_t<T> *policy,
                        std::vector<node_t<T>*> &to_propagate) {
        policy->expand(this, to_propagate);
    }
    virtual void propagate(const aot_t<T> *policy) {
        policy->propagate(this);
    }
};

template<typename T> struct state_node_t : public node_t<T> {
    const T state_;
    bool is_goal_;
    bool is_dead_end_;
    unsigned depth_;
    int best_action_;
    std::vector<std::pair<int, action_node_t<T>*> > parents_;
    std::vector<action_node_t<T>*> children_;

    state_node_t(const T &state, unsigned depth = 0)
      : state_(state),
        is_goal_(false), is_dead_end_(false),
        depth_(depth), best_action_(Problem::noop) { }
    virtual ~state_node_t() { }

    Problem::action_t best_action() const {
        return best_action_ == Problem::noop ?
          Problem::noop : children_[best_action_]->action_;
    }

    bool is_leaf() const {
        return is_dead_end_ || (!is_goal_ && children_.empty());
    }
    void update_value() {
        assert(!is_goal_);
        if( !is_dead_end_ ) {
            node_t<T>::value_ = std::numeric_limits<float>::max();
            for( unsigned i = 0, isz = children_.size(); i < isz; ++i ) {
                float child_value = children_[i]->value_;
                if( child_value < node_t<T>::value_ ) {
                    node_t<T>::value_ = child_value;
                    best_action_ = i;
                }
            }
        }
    }

    virtual void print(std::ostream &os, bool indent = true) const {
        if( indent ) os << std::setw(2 * depth_) << "";
        os << "[state=" << state_
           << ",depth=" << depth_
           << ",best_action=" << best_action()
           << ",#pa=" << parents_.size()
           << ",#chld=" << children_.size()
           << ",value=" << node_t<T>::value_
           << ",delta=" << node_t<T>::delta_
           << "]";
    }
    virtual void expand(const aot_t<T> *policy,
                        std::vector<node_t<T>*> &to_propagate) {
        policy->expand(this, to_propagate);
    }
    virtual void propagate(const aot_t<T> *policy) {
        policy->propagate(this);
    }
};

////////////////////////////////////////////////
//
// Hash Table
//

template<typename T> struct map_functions_t {
    bool operator()(const std::pair<const T*, unsigned> &p1,
                    const std::pair<const T*, unsigned> &p2) const {
        return (p1.second == p2.second) && (*p1.first == *p2.first);
    }
    size_t operator()(const std::pair<const T*, unsigned> &p) const {
        return p.first->hash();
    }
};

template<typename T> class hash_t :
  public std::tr1::unordered_map<std::pair<const T*, unsigned>,
                                 state_node_t<T>*,
                                 map_functions_t<T>,
                                 map_functions_t<T> > {
  public:
    typedef typename std::tr1::unordered_map<std::pair<const T*, unsigned>,
                                             state_node_t<T>*,
                                             map_functions_t<T>,
                                             map_functions_t<T> >
            base_type;
    typedef typename base_type::const_iterator const_iterator;
    const_iterator begin() const { return base_type::begin(); }
    const_iterator end() const { return base_type::end(); }

  public:
    hash_t() { }
    virtual ~hash_t() { }
    void print(std::ostream &os) const {
        for( const_iterator it = begin(); it != end(); ++it ) {
            os << "(" << it->first.first << "," << it->first.second << ")" << std::endl;
        }
    }
    void clear() {
        for( const_iterator it = begin(); it != end(); ++it ) {
            state_node_t<T> *s_node = it->second;
            for( int i = 0, isz = s_node->children_.size(); i < isz; ++i ) {
                delete s_node->children_[i];
            }
            delete it->second;
        }
        base_type::clear();
    }
};

////////////////////////////////////////////////
//
// Priority Queues
//

template<typename T> struct min_priority_t {
    bool operator()(const node_t<T> *n1, const node_t<T> *n2) const {
        return fabs(n1->delta_) > fabs(n2->delta_);
    }
};

template<typename T> struct max_priority_t {
    bool operator()(const node_t<T> *n1, const node_t<T> *n2) const {
        return fabs(n2->delta_) > fabs(n1->delta_);
    }
};

template<typename T> class priority_queue_t :
  public std::priority_queue<node_t<T>*,
                             std::vector<node_t<T>*>,
                             min_priority_t<T> > {
};

template<typename T> class bdd_priority_queue_t :
  public std::bdd_priority_queue<node_t<T>*,
                                 min_priority_t<T>,
                                 max_priority_t<T> > {
  public:
    bdd_priority_queue_t(unsigned capacity)
      : std::bdd_priority_queue<node_t<T>*,
                                min_priority_t<T>,
                                max_priority_t<T> >(capacity) { }
};

////////////////////////////////////////////////
//
// Policy
//

template<typename T> class aot_t : public improvement_t<T> {
  protected:
    unsigned width_;
    unsigned depth_bound_;
    float parameter_;
    bool delayed_evaluation_;
    unsigned expansions_per_iteration_;
    unsigned leaf_nsamples_;
    unsigned delayed_evaluation_nsamples_;
    mutable unsigned num_nodes_;
    mutable AOT::state_node_t<T> *root_;
#ifdef USE_BDD_PQ
    mutable AOT::bdd_priority_queue_t<T> inside_bdd_priority_queue_;
    mutable AOT::bdd_priority_queue_t<T> outside_bdd_priority_queue_;
#else
    mutable AOT::priority_queue_t<T> inside_priority_queue_;
    mutable AOT::priority_queue_t<T> outside_priority_queue_;
#endif
    mutable AOT::hash_t<T> table_;
    mutable float from_inside_;
    mutable float from_outside_;
    mutable unsigned total_number_expansions_;
    mutable unsigned total_evaluations_;

  public:
    aot_t(const policy_t<T> &base_policy,
          unsigned width,
          unsigned depth_bound,
          float parameter,
          bool delayed_evaluation,
          unsigned expansions_per_iteration,
          unsigned leaf_nsamples,
          unsigned delayed_evaluation_nsamples)
      : improvement_t<T>(base_policy),
        width_(width),
        depth_bound_(depth_bound),
        parameter_(parameter),
        delayed_evaluation_(delayed_evaluation),
        expansions_per_iteration_(expansions_per_iteration),
        leaf_nsamples_(leaf_nsamples),
        delayed_evaluation_nsamples_(delayed_evaluation_nsamples),
        num_nodes_(0),
        root_(0),
#ifdef USE_BDD_PQ
        inside_bdd_priority_queue_(expansions_per_iteration),
        outside_bdd_priority_queue_(expansions_per_iteration),
#endif
        from_inside_(0),
        from_outside_(0) {
        total_number_expansions_ = 0;
        total_evaluations_ = 0;
    }
    virtual ~aot_t() { }
    virtual const policy_t<T>* clone() const {
        return new aot_t(improvement_t<T>::base_policy_,
                         width_,
                         depth_bound_,
                         parameter_,
                         delayed_evaluation_,
                         expansions_per_iteration_,
                         leaf_nsamples_,
                         delayed_evaluation_nsamples_);
    }

    virtual Problem::action_t operator()(const T &s) const {
        // initialize tree and priority queue
        ++policy_t<T>::decisions_;
        clear();
        root_ = fetch_node(s, 0).first;
        insert_into_priority_queue(root_);

        // expand leaves and propagate values
        unsigned expanded = 0;
        std::vector<AOT::node_t<T>*> to_propagate;
        for( unsigned i = 0; (i < width_) && !empty_priority_queues(); ) {
            unsigned expanded_in_iteration = 0;
            while( (i < width_) &&
                   (expanded_in_iteration < expansions_per_iteration_) &&
                   !empty_priority_queues() ) {
                expand(to_propagate);
                for( int j = 0, jsz = to_propagate.size(); j < jsz; ++j )
                    propagate(to_propagate[j]);
                to_propagate.clear();
                ++expanded_in_iteration;
                ++i;
            }
            expanded += expanded_in_iteration;
            clear_priority_queues();
            recompute_delta(root_);
        }
        assert((width_ == 0) ||
               ((root_ != 0) &&
                policy_t<T>::problem().applicable(s, root_->best_action())));
        assert(expanded <= width_);

        // select best action
        return width_ == 0 ?
          improvement_t<T>::base_policy_(s) : root_->best_action();
    }
    virtual void print_stats(std::ostream &os) const {
        os << "stats: policy-type=aot::aot(width="
           << width_ << ",depth="
           << depth_bound_ << ",par="
           << parameter_ << ")" << std::endl;
        os << "stats: decisions=" << policy_t<T>::decisions_ << std::endl;
        os << "stats: %in=" << from_inside_ / (from_inside_ + from_outside_)
           << ", %out=" << from_outside_ / (from_inside_ + from_outside_)
           << std::endl;
        os << "stats: #expansions=" << total_number_expansions_
           << ", #evaluations=" << total_evaluations_
           << std::endl;
        improvement_t<T>::base_policy_.print_stats(os);
    }

    void print_tree(std::ostream &os) const {
        std::cout << "error: not yet implemented" << std::endl;
        assert(0);
    }

    // clear data structures
    void clear_table() const {
        table_.clear();
    }
    void clear() const {
        clear_priority_queues();
        clear_table();
        num_nodes_ = 0;
        root_ = 0;
    }

    // lookup a node in hash table; if not found, create a new entry.
    std::pair<AOT::state_node_t<T>*, bool> fetch_node(const T &state,
                                                      unsigned depth) const {
        typename AOT::hash_t<T>::iterator it =
          table_.find(std::make_pair(&state, depth));
        if( it == table_.end() ) {
            ++num_nodes_;
            AOT::state_node_t<T> *node = new AOT::state_node_t<T>(state, depth);
            table_.insert(std::make_pair(std::make_pair(&node->state_, depth),
                                         node));
            if( policy_t<T>::problem().terminal(state) ) {
                node->value_ = 0;
                node->is_goal_ = true;
            } else if( policy_t<T>::problem().dead_end(state) ) {
                node->value_ = policy_t<T>::problem().dead_end_value();
                node->is_dead_end_ = true;
            } else {
                node->value_ = evaluate(state, depth);
                node->nsamples_ = leaf_nsamples_;
            }
            return std::make_pair(node, false);
        } else {
            bool re_evaluated = false;
            if( it->second->is_leaf() && !it->second->is_dead_end_ ) {
                // resample: throw other rollouts to get better estimation
                float oval = it->second->value_;
                float nval = oval * it->second->nsamples_ + evaluate(state, depth);
                it->second->nsamples_ += leaf_nsamples_;
                it->second->value_ = nval / it->second->nsamples_;
                re_evaluated = true;
            }
            return std::make_pair(it->second, re_evaluated);
        }
    }

    // expansion of state and action nodes. The binding of appropriate method
    // is done at run-time with virtual methods
    void expand(std::vector<AOT::node_t<T>*> &to_propagate) const {
        ++total_number_expansions_;
        AOT::node_t<T> *node = select_from_priority_queue();
        node->expand(this, to_propagate);
    }
    void expand(AOT::action_node_t<T> *a_node,
                std::vector<AOT::node_t<T>*> &to_propagate,
                bool picked_from_queue = true) const {
        assert(a_node->is_leaf());
        assert(!a_node->parent_->is_dead_end_);
        std::vector<std::pair<T, float> > outcomes;
        policy_t<T>::problem().next(a_node->parent_->state_,
                                    a_node->action_,
                                    outcomes);
        a_node->children_.reserve(outcomes.size());
        for( int i = 0, isz = outcomes.size(); i < isz; ++i ) {
            const T &state = outcomes[i].first;
            float prob = outcomes[i].second;
            std::pair<AOT::state_node_t<T>*, bool> p =
              fetch_node(state, 1 + a_node->parent_->depth_);
            if( p.second ) {
                assert(p.first->is_leaf());
                to_propagate.push_back(p.first);
            }
            p.first->parents_.push_back(std::make_pair(i, a_node));
            a_node->children_.push_back(std::make_pair(prob, p.first));
            a_node->value_ += prob * p.first->value_;
        }
        a_node->value_ = a_node->action_cost_ +
          policy_t<T>::problem().discount() * a_node->value_;
        to_propagate.push_back(a_node);

        // re-sample sibling action nodes that are still leaves
        if( picked_from_queue ) {
            AOT::state_node_t<T> *parent = a_node->parent_;
            const T &state = parent->state_;
            unsigned depth = 1 + parent->depth_;
            for( int i = 0, isz = parent->children_.size(); i < isz; ++i ) {
                AOT::action_node_t<T> *sibling = parent->children_[i];
                if( sibling->is_leaf() ) {
                    float oval = (sibling->value_ - sibling->action_cost_) /
                      policy_t<T>::problem().discount();
                    float eval = evaluate(state, sibling->action_, depth);
                    float nval = oval * sibling->nsamples_ + eval;
                    sibling->nsamples_ +=
                      delayed_evaluation_nsamples_ * leaf_nsamples_;
                    sibling->value_ = sibling->action_cost_ +
                      policy_t<T>::problem().discount()*nval/sibling->nsamples_;

#ifdef DEBUG
                    std::cout << "sibling re-sampled: "
                              << "num=" << sibling->nsamples_
                              << std::endl;
#endif
                }
            }
        }
    }
    void expand(AOT::state_node_t<T> *s_node,
                std::vector<AOT::node_t<T>*> &to_propagate) const {
        assert(s_node->is_leaf());
        assert(!s_node->is_dead_end_);
        s_node->children_.reserve(policy_t<T>::problem().number_actions(s_node->state_));
        for( Problem::action_t a = 0; a < policy_t<T>::problem().number_actions(s_node->state_); ++a ) {
            if( policy_t<T>::problem().applicable(s_node->state_, a) ) {
                // create node for this action
                ++num_nodes_;
                AOT::action_node_t<T> *a_node = new AOT::action_node_t<T>(a);
                a_node->action_cost_ = policy_t<T>::problem().cost(s_node->state_, a);
                a_node->parent_ = s_node;
                s_node->children_.push_back(a_node);

                // expand node
                if( !delayed_evaluation_ ) {
                    expand(a_node, to_propagate, false);
                } else {
                    // instead of full-width expansion to calculate value,
                    // estimate it by sampling states and applying rollouts
                    // of base policy
                    float eval = evaluate(s_node->state_, a, 1+s_node->depth_);
                    a_node->value_ = a_node->action_cost_ +
                      policy_t<T>::problem().discount() * eval;
                    a_node->nsamples_
                      = delayed_evaluation_nsamples_ * leaf_nsamples_;
                }
            }
        }
        to_propagate.push_back(s_node);
    }

    // propagate new values bottom-up using BFS and stopping when values changes no further
    void propagate(AOT::node_t<T> *node) const {
        node->propagate(this);
    }
    void propagate(AOT::action_node_t<T> *a_node) const {
        assert(a_node->parent_ != 0);
        propagate(a_node->parent_);
    }
    void propagate(AOT::state_node_t<T> *s_node) const {
        std::deque<AOT::state_node_t<T>*> queue;
        queue.push_back(s_node);
        s_node->in_queue_ = true;
        while( !queue.empty() ) {
            AOT::state_node_t<T> *s_node = queue.front();
            queue.pop_front();
            s_node->in_queue_ = false;
            float old_value = s_node->value_;
            if( !s_node->is_leaf() ) s_node->update_value();
            if( s_node->is_leaf() || (old_value != s_node->value_) ) {
                for( int i = 0, isz = s_node->parents_.size(); i < isz; ++i ) {
                    AOT::action_node_t<T> *a_node = s_node->parents_[i].second;
                    float old_value = a_node->value_;
                    a_node->update_value(policy_t<T>::problem().discount());
                    assert(a_node->parent_ != 0);
                    if( !a_node->parent_->in_queue_ && (a_node->value_ != old_value) ) {
                        queue.push_back(a_node->parent_);
                        a_node->parent_->in_queue_ = true;
                    }
                }
            }
        }
    }

    // recompute delta values for nodes in top-down BFS manner
    void recompute_delta(AOT::state_node_t<T> *root) const {
        assert(!root->is_goal_);
        assert(!root->is_dead_end_);

        std::deque<AOT::state_node_t<T>*> s_queue;
        bool expanding_from_s_queue = true;
        std::deque<AOT::action_node_t<T>*> a_queue;
        bool expanding_from_a_queue = false;

        root->delta_ = std::numeric_limits<float>::max();
        root->in_best_policy_ = true;
        s_queue.push_back(root);

        while( !s_queue.empty() || !a_queue.empty() ) {
            // expand from the state queue
            if( expanding_from_s_queue ) {
                while( !s_queue.empty() ) {
                    AOT::state_node_t<T> *s_node = s_queue.back();
                    s_queue.pop_back();
                    s_node->in_queue_ = false;
                    recompute(s_node, a_queue);
                }
                expanding_from_s_queue = false;
                expanding_from_a_queue = true;
            }

            // expand from the action queue
            if( expanding_from_a_queue ) {
                while( !a_queue.empty() ) {
                    AOT::action_node_t<T> *a_node = a_queue.back();
                    a_queue.pop_back();
                    recompute(a_node, s_queue);
                }
                expanding_from_a_queue = false;
                expanding_from_s_queue = true;
            }
        }
    }
    void recompute(AOT::state_node_t<T> *s_node,
                   std::deque<AOT::action_node_t<T>*> &a_queue) const {
        assert(!s_node->is_goal_);
        assert(!s_node->is_dead_end_);
        if( s_node->is_leaf() ) {
            // insert tip node into priority queue
            if( !s_node->is_dead_end_ && (s_node->depth_ < depth_bound_) ) {
                insert_into_priority_queue(s_node);
            }
        } else {
            assert(!s_node->children_.empty());
            float best_value = s_node->children_[s_node->best_action_]->value_;
            if( s_node->in_best_policy_ ) {
                assert(s_node->delta_ >= 0);

                // compute Delta
                float Delta = std::numeric_limits<float>::max();
                for( int i = 0, isz = s_node->children_.size(); i < isz; ++i ) {
                    if( i != s_node->best_action_ ) {
                        AOT::action_node_t<T> *a_node = s_node->children_[i];
                        float d = a_node->value_ - best_value;
                        Delta = Utils::min(Delta, d);
                    }
                }

                // compute delta
                for( int i = 0, isz = s_node->children_.size(); i < isz; ++i ) {
                    AOT::action_node_t<T> *a_node = s_node->children_[i];
                    if( i == s_node->best_action_ ) {
                        a_node->delta_ = Utils::min(s_node->delta_, Delta);
                        a_node->in_best_policy_ = true;
                        assert(a_node->delta_ >= 0);
                    } else {
                        a_node->delta_ = best_value - a_node->value_;
                        a_node->in_best_policy_ = false;
                        assert(a_node->delta_ <= 0);
                    }
                    a_queue.push_back(a_node);
                }
            } else {
                assert(s_node->delta_ <= 0);
                for( int i = 0, isz = s_node->children_.size(); i < isz; ++i ) {
                    AOT::action_node_t<T> *a_node = s_node->children_[i];
                    a_node->delta_ = s_node->delta_ + best_value - a_node->value_;
                    a_node->in_best_policy_ = false;
                    assert(a_node->delta_ <= 0);
                    a_queue.push_back(a_node);
                }
            }
        }
    }
    void recompute(AOT::action_node_t<T> *a_node,
                   std::deque<AOT::state_node_t<T>*> &s_queue) const {
        if( a_node->is_leaf() ) {
            // insert tip node into priority queue
            if( a_node->parent_->depth_ < depth_bound_ ) {
                insert_into_priority_queue(a_node);
            }
        } else {
            for( int i = 0, isz = a_node->children_.size(); i < isz; ++i ) {
                AOT::state_node_t<T> *s_node = a_node->children_[i].second;
                if( !s_node->in_queue_ && !s_node->is_goal_ && !s_node->is_dead_end_ ) {
                    float delta = std::numeric_limits<float>::max();
                    bool in_best_policy = false;
                    for( int j = 0, jsz = s_node->parents_.size(); j < jsz; ++j ) {
                        int child_index = s_node->parents_[j].first;
                        AOT::action_node_t<T> *parent = s_node->parents_[j].second;
                        assert(parent->children_[child_index].second == s_node);
                        float d = parent->delta_ /
                                  (policy_t<T>::problem().discount() *
                                   parent->children_[child_index].first);
                        delta = Utils::min(delta, fabsf(d));
                        in_best_policy = in_best_policy || parent->in_best_policy_;
                    }
                    s_node->delta_ = in_best_policy ? delta : -delta;
                    s_node->in_best_policy_ = in_best_policy;
                    s_queue.push_back(s_node);
                    s_node->in_queue_ = true;
                }
            }
        }
    }

    // evaluate a state with base policy, and evaluate an action node by
    // sampling states
    float evaluate(const T &s, unsigned depth) const {
        total_evaluations_ += leaf_nsamples_;
        return depth < depth_bound_ ?
          Evaluation::evaluation(improvement_t<T>::base_policy_,
                                 s,
                                 leaf_nsamples_, depth_bound_ - depth) : 0;
    }
    float evaluate(const T &state,
                   Problem::action_t action,
                   unsigned depth) const {
        float value = 0;
        for( unsigned i = 0; i < delayed_evaluation_nsamples_; ++i ) {
            std::pair<T, bool> sample =
              policy_t<T>::problem().sample(state, action);
            value += evaluate(sample.first, depth);
        }
        return value / delayed_evaluation_nsamples_;
    }

    // implementation of priority queue for storing the deltas
    unsigned size_priority_queues() const {
#ifdef USE_BDD_PQ
        return inside_bdd_priority_queue_.size() + outside_bdd_priority_queue_.size();
#else
        return inside_priority_queue_.size() + outside_priority_queue_.size();
#endif
    }
    bool empty_inside_priority_queue() const {
#ifdef USE_BDD_PQ
        return inside_bdd_priority_queue_.empty();
#else
        return inside_priority_queue_.empty();
#endif
    }
    bool empty_outside_priority_queue() const {
#ifdef USE_BDD_PQ
        return outside_bdd_priority_queue_.empty();
#else
        return outside_priority_queue_.empty();
#endif
    }
    bool empty_priority_queues() const {
        return empty_inside_priority_queue() && empty_outside_priority_queue();
    }
    void clear(AOT::priority_queue_t<T> &pq) const {
        while( !pq.empty() ) {
            AOT::node_t<T> *node = pq.top();
            pq.pop();
            assert(node->in_pq_);
            node->in_pq_ = false;
        }
    }
    void clear(AOT::bdd_priority_queue_t<T> &pq) const {
        while( !pq.empty() ) {
            AOT::node_t<T> *node = pq.top();
            pq.pop();
            assert(node->in_pq_);
            node->in_pq_ = false;
        }
    }
    void clear_priority_queues() const {
#ifdef USE_BDD_PQ
        clear(inside_bdd_priority_queue_);
        clear(outside_bdd_priority_queue_);
#else
        clear(inside_priority_queue_);
        clear(outside_priority_queue_);
#endif
    }
    void insert_into_inside_priority_queue(AOT::node_t<T> *node) const {
#ifdef USE_BDD_PQ
        std::pair<bool, bool> p = inside_bdd_priority_queue_.push(node);
        node->in_pq_ = p.first;
        if( p.second ) {
            AOT::node_t<T> *removed = inside_bdd_priority_queue_.removed_element();
            assert(removed != 0);
            assert(removed->in_pq_);
            removed->in_pq_ = false;
        }
#else
        inside_priority_queue_.push(node);
        node->in_pq_ = true;
#endif
    }
    void insert_into_outside_priority_queue(AOT::node_t<T> *node) const {
#ifdef USE_BDD_PQ
        std::pair<bool, bool> p = outside_bdd_priority_queue_.push(node);
        node->in_pq_ = p.first;
        if( p.second ) {
            AOT::node_t<T> *removed = outside_bdd_priority_queue_.removed_element();
            assert(removed != 0);
            assert(removed->in_pq_);
            removed->in_pq_ = false;
        }
#else
        outside_priority_queue_.push(node);
        node->in_pq_ = true;
#endif
    }
    void insert_into_priority_queue(AOT::node_t<T> *node) const {
        if( !node->in_pq_ ) {
#ifdef DEBUG
            std::cout << "push ";
            node->print(std::cout, false);
            std::cout << std::endl;
#endif

            if( node->delta_ >= 0 )
                insert_into_inside_priority_queue(node);
            else
                insert_into_outside_priority_queue(node);
        }
    }
    AOT::node_t<T>* select_from_inside() const {
        AOT::node_t<T> *node = 0;
#ifdef USE_BDD_PQ
        node = inside_bdd_priority_queue_.top();
        inside_bdd_priority_queue_.pop();
#else
        node = inside_priority_queue_.top();
        inside_priority_queue_.pop();
#endif
        assert(node->in_pq_);
        node->in_pq_ = false;
        ++from_inside_;
        return node;
    }
    AOT::node_t<T>* select_from_outside() const {
       AOT::node_t<T> *node = 0;
#ifdef USE_BDD_PQ
        node = outside_bdd_priority_queue_.top();
        outside_bdd_priority_queue_.pop();
#else
        node = outside_priority_queue_.top();
        outside_priority_queue_.pop();
#endif
        assert(node->in_pq_);
        node->in_pq_ = false;
        ++from_outside_;
        return node;
    }
    AOT::node_t<T>* select_from_priority_queue() const {
        AOT::node_t<T> *node = 0;
        if( empty_inside_priority_queue() ) {
            node = select_from_outside();
        } else if( empty_outside_priority_queue() ) {
            node = select_from_inside();
        } else {
            if( Random::real() < parameter_ ) {
                node = select_from_inside();
            } else {
                node = select_from_outside();
            }
        }

#ifdef DEBUG
        std::cout << "pop ";
        node->print(std::cout, false);
        std::cout << std::endl;
#endif

        return node;
    }
};

}; // namespace AOT

template<typename T>
inline const policy_t<T>* make_aot(const policy_t<T> &base_policy,
                                   unsigned width,
                                   unsigned depth_bound,
                                   float parameter,
                                   bool delayed_evaluation = true,
                                   unsigned expansions_per_iteration = 100,
                                   unsigned leaf_nsamples = 1,
                                   unsigned delayed_evaluation_nsamples = 1) {
    return new AOT::aot_t<T>(base_policy,
                             width,
                             depth_bound,
                             parameter,
                             delayed_evaluation,
                             expansions_per_iteration,
                             leaf_nsamples,
                             delayed_evaluation_nsamples);
}

}; // namespace Policy

#undef DEBUG

#endif

