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

// File: systematic/random.cc - The implementation of the random scheduler
// which picks a random thread to run at each schedule point.

#include "systematic/random.h"

#include <cstdlib>
#include "core/logging.h"

namespace systematic {

RandomScheduler::RandomScheduler(ControllerInterface *controller)
    : Scheduler(controller) {
  // empty
}

RandomScheduler::~RandomScheduler() {
  // empty;
}

void RandomScheduler::Register() {
  knob()->RegisterBool("enable_random_scheduler", "whether use the random scheduler", "0");
}

bool RandomScheduler::Enabled() {
  return knob()->ValueBool("enable_random_scheduler");
}

void RandomScheduler::Setup() {
  // seed the random number generator
  srand((unsigned int)time(NULL));
}

void RandomScheduler::ProgramStart() {
  // empty
}

void RandomScheduler::ProgramExit() {
  // empty
}

void RandomScheduler::Explore(State *init_state) {
  // start with the initial state
  State *state = init_state;
  // run until no enabled thread
  while (!state->IsTerminal()) {
    // randomly pick the next thread to run
    Action *action = PickNextRandom(state);
    // execute the action and move to next state
    state = Execute(state, action);
  }
}

bool RandomScheduler::RandomChoice(double true_rate) {
  double val = rand() / (RAND_MAX + 1.0);
  if (val < true_rate)
    return true;
  else
    return false;
}

Action *RandomScheduler::PickNextRandom(State *state) {
  Action::Map *enabled = state->enabled();
  Action *target = NULL;
  int counter = 1;
  for (Action::Map::iterator it = enabled->begin(); it != enabled->end(); ++it){
    Action *current = it->second;
    // decide whether to pick the current one
    if (RandomChoice(1.0 / (double)counter)) {
      target = current;
    }
    // increment the counter
    counter += 1;
  }
  DEBUG_ASSERT(target);
  return target;
}

} // namespace systematic

