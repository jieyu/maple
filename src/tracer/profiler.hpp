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

// File: tracer/profiler.hpp - Define tracer online profiler.

#ifndef TRACER_PROFILER_HPP_
#define TRACER_PROFILER_HPP_

#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "tracer/recorder.h"

namespace tracer {

class TraceLog;

class Profiler : public ExecutionControl {
 public:
  Profiler() : recorder_(NULL) {}
  ~Profiler() {}

 private:
  void HandlePreSetup();
  void HandlePostSetup();
  bool HandleIgnoreInstCount(IMG img);
  bool HandleIgnoreMemAccess(IMG img);

  RecorderAnalyzer *recorder_;

  DISALLOW_COPY_CONSTRUCTORS(Profiler);
};

} // namespace tracer

#endif

