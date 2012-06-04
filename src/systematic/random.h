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

// File: systematic/random.h - The definition of the random scheduler
// which picks a random thread to run at each schedule point.

#ifndef SYSTEMATIC_RANDOM_H_
#define SYSTEMATIC_RANDOM_H_

#include "core/basictypes.h"
#include "systematic/scheduler.h"

namespace systematic {

class RandomScheduler : public Scheduler {
 public:
  explicit RandomScheduler(ControllerInterface *controller);
  ~RandomScheduler();

  // overrided virtual functions
  void Register();
  bool Enabled();
  void Setup();
  void ProgramStart();
  void ProgramExit();
  void Explore(State *init_state);

 protected:
  // helper functions
  bool RandomChoice(double true_rate);
  Action *PickNextRandom(State *state);

 private:
  DISALLOW_COPY_CONSTRUCTORS(RandomScheduler);
};

} // namespace systematic

#endif

