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

// File: systematic/fair.h - The definition of the fair schedule control
// module. Please refer to the paper "Madanlal Musuvathi, Shaz Qadeer:
// Fair stateless model checking. PLDI 2008: 362-371" for more info.

#ifndef SYSTEMATIC_FAIR_H_
#define SYSTEMATIC_FAIR_H_

#include "core/basictypes.h"
#include "systematic/program.h"

namespace systematic {

// the fair schedule control module
class FairControl {
 public:
  FairControl() {}
  ~FairControl() {}

  bool Enabled(State *state, Action *action);
  void Update(State *curr_state);
  std::string ToString();

 protected:
  typedef std::set<Thread *> ThreadSet;
  typedef std::map<Thread *, ThreadSet> ThreadSetMap;
  typedef std::pair<Thread *, Thread *> ThreadPair;
  typedef std::list<ThreadPair> ThreadRelation;

  // E[t] is the set of threads that have been continuously enabled
  // since the last yield by thread t
  ThreadSetMap e_;

  // D[t] is the set of threads that have been disabled by some
  // transition of thread t since the last yield by thread t
  ThreadSetMap d_;

  // S[t] is the set fo threads that have been scheduled since
  // the last yield by thraed t
  ThreadSetMap s_;

  // represent a priority ordering on threads. specifically, if
  // (t, u) \in P, then t will be scheduled only when t is enabled
  // and u is not enabled at the current state (i.e. t has a
  // relatively low priority to u)
  ThreadRelation p_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(FairControl);
};

} // namespace systematic

#endif

