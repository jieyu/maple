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

// File: systematic/scheduler.cc - The abstract implementation of scheduler
// for systematic concurrency testing.

#include "systematic/scheduler.h"

#include "core/logging.h"

namespace systematic {

void Scheduler::Main(State *init_state) {
  // set counters for enabled actions of the initial state
  SetActionCounters(init_state);
  // call explore function
  Explore(init_state);
}

State *Scheduler::Execute(State *state, Action *action) {
  // update the stored counters
  if (action->obj()) {
    Action::idx_t &tc_idx = GetThreadCounter(action->thd());
    Action::idx_t &oc_idx = GetObjectCounter(action->obj());
    tc_idx = action->tc();
    oc_idx = action->oc();
  }
  // update the taken field
  state->set_taken(action);
  // execute the action, and get the next state
  State *next_state = controller_->Execute(state, action);
  // set counters for enabled actions
  SetActionCounters(next_state);
  return next_state;
}

void Scheduler::SetActionCounters(State *state) {
  for (Action::Map::iterator it = state->enabled()->begin();
       it != state->enabled()->end(); ++it) {
    Action *action = it->second;
    if (action->obj()) {
      // update tc counter in action
      Action::idx_t &tc_idx = GetThreadCounter(action->thd());
      action->set_tc(tc_idx + 1);
      // update oc counter in action
      Action::idx_t &oc_idx = GetObjectCounter(action->obj());
      if (action->IsWrite())
        action->set_oc(oc_idx + 1);
      else
        action->set_oc(oc_idx);
    }
  }
}

Action::idx_t &Scheduler::GetThreadCounter(Thread *thd) {
  std::map<Thread *, Action::idx_t>::iterator it = tc_map_.find(thd);
  if (it == tc_map_.end()) {
    Action::idx_t &idx = tc_map_[thd];
    idx = 0;
    return idx;
  } else {
    return it->second;
  }
}

Action::idx_t &Scheduler::GetObjectCounter(Object *obj) {
  std::map<Object *, Action::idx_t>::iterator it = oc_map_.find(obj);
  if (it == oc_map_.end()) {
    Action::idx_t &idx = oc_map_[obj];
    idx = 0;
    return idx;
  } else {
    return it->second;
  }
}

} // namespace systematic

