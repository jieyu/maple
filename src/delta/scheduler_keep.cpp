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

// File: idiom/scheduler.cpp - Implementation of the active scheduler
// controller.

#include "delta/scheduler_keep.hpp"

namespace delta {

SchedulerKeep::SchedulerKeep()
    : observer_(NULL),
      ilist_(NULL),
      slist_(NULL){
  // empty
}

void SchedulerKeep::HandlePreSetup() {
  idiom::SchedulerCommon::HandlePreSetup();

  knob_->RegisterStr("ilist_out", "The output memorization database.", "ilist.db");  
  knob_->RegisterStr("suspect_list", "Candidate root causes", "slist.in");
  knob_->RegisterInt("idiom_type", "what is the idiom type of suspect candidates", "1");
  knob_->RegisterInt("target_idiom", "The target idiom.", "0");

  observer_ = new Observer(knob_);
  observer_->Register();
}

void SchedulerKeep::HandlePostSetup() {
  idiom::SchedulerCommon::HandlePostSetup();

  ilist_ = new iList(CreateMutex());

  if (observer_->Enabled()) {
    // add observer
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, ilist_);
    AddAnalyzer(observer_);
  }

  slist_ = new SuspectList(CreateMutex(), iroot_db_);
  slist_->Load(knob_->ValueStr("suspect_list"), sinfo_);
}

void SchedulerKeep::HandleProgramExit() {
  idiom::SchedulerCommon::HandleProgramExit();

}

void SchedulerKeep::Choose() {
  // set current iroot to test
  int target_iroot_id = knob_->ValueInt("target_iroot");
  if (target_iroot_id) {
    curr_iroot_ = slist_->ChooseForTest((idiom::iroot_id_t)target_iroot_id);
  } else {
	printf("No iRoot to test, exit...\n");
	exit(0);
  }

  if (!curr_iroot_) {
    printf("No iRoot to test, exit...\n");
    exit(0);
  }

  printf("[Active Scheduler] Target iroot = %u, target idiom = %d\n",
         curr_iroot_->id(), (int)curr_iroot_->idiom());
  RecordTargetiRoot(curr_iroot_);
}

void SchedulerKeep::TestSuccess() {
  idiom::SchedulerCommon::TestSuccess();
  //memo_->TestSuccess(curr_iroot_, true);
}

void SchedulerKeep::TestFail() {
  idiom::SchedulerCommon::TestFail();
  //memo_->TestFail(curr_iroot_, false);
}

bool SchedulerKeep::UseDecreasingPriorities() {
  //return memo_->TotalTestRuns(curr_iroot_, true) % 2 == 0;
  return 0;
}

bool SchedulerKeep::YieldWithDelay() {
  if (knob_->ValueBool("yield_with_delay")) {
    //if (memo_->Async(curr_iroot_, true)) {
    //  return true;
    //}
  }
  return false;
}

} // namespace idiom

