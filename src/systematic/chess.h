// Copyright 2011 The University of Michigan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: systematic/chess.h - The definition of the CHESS scheduler
// which systematically explore thread interleavings using iterative
// preemption bound.

#ifndef SYSTEMATIC_CHESS_H_
#define SYSTEMATIC_CHESS_H_

#include "core/basictypes.h"
#include "systematic/scheduler.h"
#include "systematic/search.h"
#include "systematic/fair.h"
#include "systematic/chess.pb.h"

namespace systematic {

// define the CHESS scheduler
class ChessScheduler : public Scheduler {
 public:
  explicit ChessScheduler(ControllerInterface *controller);
  ~ChessScheduler();

  // overrided virtual functions
  void Register();
  bool Enabled();
  void Setup();
  void ProgramStart();
  void ProgramExit();
  void Explore(State *init_state);

 protected:
  // define the hash value type (used for partial order reduction)
  typedef size_t hash_val_t;

  // define hash map for actions
  typedef std::tr1::unordered_map<hash_val_t, Action::List> ActionHashMap;

  // define the table for past executions. this is only useful
  // when performing partial order reduction. the reason we have
  // that is because we need to know whether two states match by
  // checking with past executions. the loading of past executions
  // can be lazy (demand driven)
  typedef std::tr1::unordered_map<int, Execution *> ExecutionTable;

  // define a visited state (used for partial order reduction)
  class VisitedState {
   public:
    typedef std::vector<VisitedState *> Vec;
    typedef std::tr1::unordered_map<hash_val_t, Vec> HashMap;

    VisitedState()
        : hash_val(0),
          preemptions(0),
          exec_id(0),
          state_idx(0) {}

    ~VisitedState() {}

    hash_val_t hash_val;
    int preemptions;
    int exec_id;
    size_t state_idx;
  };

  // helper functions
  void DivergenceRun();
  void UselessRun();
  Action *PickNext();
  Action *PickNextRandom();
  bool IsFrontier();
  bool IsPrefix();
  bool IsPreemptiveChoice(Action *action);
  void UpdateBacktrack();
  bool RandomChoice(double true_rate);
  hash_val_t Hash(Action *action);
  hash_val_t HashJoin(hash_val_t h1, hash_val_t h2) { return h1 ^ h2; }

  // fair related
  void FairUpdate();
  bool FairEnabled(Action *next_action);

  // preemption bound related
  void PbInit();
  void PbFini();
  void PbUpdate(Action *next_action);
  bool PbEnabled(Action *next_action);

  // partial order reduction related
  void PorInit();
  void PorFini();
  void PorUpdate(Action *next_action);
  bool PorVisited(Action *next_action);
  bool PorStateMatch(State *state, Action *action, State *vs_state);
  Execution *PorGetExec(int exec_id);
  void PorLoad();
  void PorSave();
  void PorPrepareDir();

  // settings and flags
  bool fair_enable_; // whether use the fair control module
  bool pb_enable_; // whether bound the number of preemptions
  bool por_enable_; // whether perform sleep-set based por
  int pb_limit_; // the bound of the number of preemptions
  std::string por_info_path_; // the dir storing por information

  // global analysis states
  bool useless_;
  bool divergence_;
  State *curr_state_;
  Action *curr_action_;
  SearchNode *curr_node_;
  SearchInfo search_info_;
  size_t prefix_size_;

  // fair control related
  FairControl fair_ctrl_;

  // preemption bound related
  int curr_preemptions_;

  // partial order reduction related
  hash_val_t curr_hash_val_;
  VisitedState::HashMap visited_states_;
  VisitedState::Vec curr_visited_states_; // visited states in this exec
  ExecutionTable loaded_execs_; // in-memory past executions
  int curr_exec_id_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(ChessScheduler);
};

} // namespace systematic

#endif

