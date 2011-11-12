#include <iostream>
#include <iomanip>
#include <strings.h>

#include "ao2.h"
#include "ao3.h"
#include "ao4.h"
#include "mcts.h"

using namespace std;

unsigned policy = 0;
unsigned evaluation_trials = 200;
unsigned evaluation_depth = 70;

unsigned rollout_width = 50;
unsigned rollout_depth = 50;
unsigned rollout_nesting = 3;

unsigned uct_width = 32;
unsigned uct_depth = 50;
float uct_parameter = -0.15;

unsigned ao_width = 32;
unsigned ao_depth = 50;
float ao_parameter = 0.5;

template<typename T>
void evaluate_policy(const Policy::policy_t<T> &policy) {
    float start_time = Utils::read_time_in_seconds();
    cout << setprecision(5);
    cout << Policy::evaluation(policy, policy.problem().init(), evaluation_trials, evaluation_depth);
    cout << setprecision(2);
    cout << " ( " << Utils::read_time_in_seconds() - start_time << " secs)" << endl;
}
 
template<typename T>
void evaluate_hash_policy(const Problem::hash_t<T> *hash, const char *name) {
    if( hash == 0 ) {
        cout << name << "=<not-available>" << endl;
    } else {
        cout << name << "= " << flush;
        Policy::hash_policy_t<T> policy(*hash);
        evaluate_policy(policy);
    }
}

template<typename T>
void evaluate_rollout_policy(const Policy::policy_t<T> &base, const char *name) {
    Policy::nested_rollout_t<T> policy(base, rollout_width, rollout_depth, rollout_nesting);
    cout << "nrollout(" << name << ",width=" << rollout_width << ",nesting=" << rollout_nesting << ")= " << flush;
    evaluate_policy(policy);
}

template<typename T>
void evaluate_uct_policy(const Policy::policy_t<T> &base, const char *name) {
    Policy::mcts_t<T> policy(base, uct_width, uct_depth, uct_parameter); 
    cout << "uct(" << name << ",width=" << uct_width << ",depth=" << uct_depth << ",p=" << uct_parameter << ")= " << flush;
    evaluate_policy(policy);
}

template<typename T>
void evaluate_ao2_policy(const Policy::policy_t<T> &base, const char *name) {
    Policy::ao2_t<T> policy(base, ao_width, ao_depth); 
    cout << "ao2(" << name << ",width=" << ao_width << ",depth=" << ao_depth << ")= " << flush;
    evaluate_policy(policy);
}

template<typename T>
void evaluate_ao3_policy(const Policy::policy_t<T> &base, const char *name, const Problem::hash_t<T> *hash) {
    //Policy::ao3_t<T> policy(base, ao_width, ao_depth, ao_parameter); 
    Policy::ao4_t<T> policy(base, ao_width, ao_depth, ao_parameter, false); 
    policy.optimal_ = hash;
    cout << "ao3(" << name << ",width=" << ao_width << ",depth=" << ao_depth << ",p=" << ao_parameter << ")= " << flush;
    evaluate_policy(policy);
    policy.stats(std::cout);
}

template<typename T>
void evaluate_ao4_policy(const Policy::policy_t<T> &base, const char *name, const Problem::hash_t<T> *hash, bool delayed = true) {
    Policy::ao4_t<T> policy(base, ao_width, ao_depth, ao_parameter, delayed); 
    policy.optimal_ = hash;
    cout << "ao4(" << name << ",width=" << ao_width << ",depth=" << ao_depth << ",p=" << ao_parameter << ")= " << flush;
    evaluate_policy(policy);
    policy.stats(std::cout);
}

template<typename T>
void evaluate_policy(unsigned policy, const Problem::problem_t<T> &problem, const Problem::hash_t<T> *hash, const Heuristic::heuristic_t<T> *heuristic) {

    Policy::random_t<T> random_policy(problem);
    switch( policy ) {
        case 1:
            evaluate_hash_policy(hash, "optimal");
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            if( heuristic != 0 ) {
                Policy::greedy_t<T> greedy_policy(problem, *heuristic);
                switch( policy ) {
                    case 10:
                        cout << "greedy= " << flush;
                        evaluate_policy(greedy_policy);
                        break;
                    case 11:
                        evaluate_rollout_policy(greedy_policy, "greedy");
                        break;
                    case 12:
                        evaluate_uct_policy(greedy_policy, "greedy");
                        break;
                    case 13:
                        evaluate_ao2_policy(greedy_policy, "greedy");
                        break;
                    case 14:
                        evaluate_ao3_policy(greedy_policy, "greedy", hash);
                        break;
                    case 15:
                        evaluate_ao4_policy(greedy_policy, "greedy", hash);
                        break;
                }
            } else {
                cout << "<policy=" << policy << " is not available>" << endl;
            }
            break;
        case 20:
            cout << "random= " << flush;
            evaluate_policy(random_policy);
            break;
        case 21:
            evaluate_rollout_policy(random_policy, "random");
            break;
        case 22:
            evaluate_uct_policy(random_policy, "random");
            break;
        case 23:
            evaluate_ao2_policy(random_policy, "random");
            break;
        case 24:
            evaluate_ao3_policy(random_policy, "random", hash);
            break;
        case 25:
            evaluate_ao4_policy(random_policy, "random", hash);
            break;
    }
}

template<typename T>
void evaluate_all_policies(const Problem::problem_t<T> &problem, const Problem::hash_t<T> *hash, const Heuristic::heuristic_t<T> *heuristic) {
    evaluate_policy(1, problem, hash, heuristic);
    for( unsigned policy = 10; policy <= 15; ++policy ) {
        evaluate_policy(policy, problem, hash, heuristic);
    }
    for( unsigned policy = 20; policy <= 25; ++policy ) {
        evaluate_policy(policy, problem, hash, heuristic);
    }
}
