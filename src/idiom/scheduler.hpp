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

// File: idiom/scheduler.hpp - Define the active scheduler controller.

#ifndef IDIOM_SCHEDULER_HPP_
#define IDIOM_SCHEDULER_HPP_

#include "sinst/sinst.h"
#include "sinst/analyzer.h"
#include "idiom/memo.h"
#include "idiom/observer.h"
#include "idiom/observer_new.h"
#include "idiom/scheduler_common.hpp"

namespace idiom {

// The controller for the idiom driven active scheduler.
class Scheduler : public SchedulerCommon {
 public:
  Scheduler();
  ~Scheduler() {}

 protected:
  void HandlePreSetup();
  void HandlePostSetup();
  bool HandleIgnoreInstCount(IMG img);
  bool HandleIgnoreMemAccess(IMG img);
  void HandleProgramExit();

  // functions to override
  void Choose();
  void TestSuccess();
  void TestFail();
  bool UseDecreasingPriorities();
  bool YieldWithDelay();

  Memo *memo_;
  sinst::SharedInstDB *sinst_db_;
  sinst::SharedInstAnalyzer *sinst_analyzer_;
  Observer *observer_;
  ObserverNew *observer_new_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Scheduler);
};

} // namespace idiom

#endif

