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

// File: idiom/scheduler_keep.hpp - Define the active scheduler controller.

#ifndef IDIOM_SCHEDULER_KEEP_HPP_
#define IDIOM_SCHEDULER_KEEP_HPP_

//#include "sinst/sinst.h"
//#include "sinst/analyzer.h"
//#include "idiom/memo.h"
#include "idiom/scheduler_common.hpp"
#include "delta/observer.h"
#include "ilist.h"
#include "slist.h"

namespace delta {

// The controller for the idiom driven active scheduler.
class SchedulerKeep : public idiom::SchedulerCommon {
 public:
  SchedulerKeep();
  ~SchedulerKeep() {}

 protected:
  void HandlePreSetup();
  void HandlePostSetup();
  void HandleProgramExit();

  // functions to override
  void Choose();
  void TestSuccess();
  void TestFail();
  bool UseDecreasingPriorities();
  bool YieldWithDelay();

  Observer *observer_;
  iList *ilist_; 
  SuspectList *slist_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(SchedulerKeep);
};

} // namespace delta

#endif

