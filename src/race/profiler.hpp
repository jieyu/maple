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

// File: race/profiler.hpp - Define the profiler for data race detectors.

#ifndef RACE_PROFILER_HPP_
#define RACE_PROFILER_HPP_

#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "race/race.h"
#include "race/djit.h"

namespace race {

class Profiler : public ExecutionControl {
 public:
  Profiler() : race_db_(NULL), djit_analyzer_(NULL) {}
  ~Profiler() {}

 protected:
  void HandlePreSetup();
  void HandlePostSetup();
  bool HandleIgnoreMemAccess(IMG img);
  void HandleProgramExit();

  RaceDB *race_db_;
  Djit *djit_analyzer_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Profiler);
};

} // namespace race

#endif

