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

// File: idiom/randsched_profiler.hpp - Define the controller for idiom
// Random profiler.

#ifndef IDIOM_RANDSCHED_PROFILER_HPP_
#define IDIOM_RANDSCHED_PROFILER_HPP_

#include "core/basictypes.h"
#include "randsched/scheduler.hpp"
#include "sinst/sinst.h"
#include "sinst/analyzer.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"
#include "idiom/observer.h"
#include "idiom/observer_new.h"
#include "idiom/predictor.h"
#include "idiom/predictor_new.h"

namespace idiom {

class RandSchedProfiler : public randsched::Scheduler {
 public:
  RandSchedProfiler();
  ~RandSchedProfiler() {}

 private:
  void HandlePreSetup();
  void HandlePostSetup();
  bool HandleIgnoreInstCount(IMG img);
  bool HandleIgnoreMemAccess(IMG img);
  void HandleProgramExit();

  iRootDB *iroot_db_;
  Memo *memo_;
  sinst::SharedInstDB *sinst_db_;
  sinst::SharedInstAnalyzer *sinst_analyzer_;
  Observer *observer_;
  ObserverNew *observer_new_;
  Predictor *predictor_;
  PredictorNew *predictor_new_;

  DISALLOW_COPY_CONSTRUCTORS(RandSchedProfiler);
};

} // namespace idiom

#endif


