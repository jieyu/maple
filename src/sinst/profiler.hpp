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

// File: sinst/profiler.hpp - Define the controller for shared instruction
// profiler. A shared instruction is an instruction that might access a
// shared location.

#ifndef SINST_PROFILER_HPP_
#define SINST_PROFILER_HPP_

#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "sinst/sinst.h"
#include "sinst/analyzer.h"

namespace sinst {

class Profiler : public ExecutionControl {
 public:
  Profiler() : sinst_db_(NULL), sinst_analyzer_(NULL) {}
  ~Profiler() {}

 private:
  void HandlePreSetup();
  void HandlePostSetup();
  void HandleProgramExit();

  SharedInstDB *sinst_db_;
  SharedInstAnalyzer *sinst_analyzer_;

  DISALLOW_COPY_CONSTRUCTORS(Profiler);
};

} // namespace sinst

#endif

