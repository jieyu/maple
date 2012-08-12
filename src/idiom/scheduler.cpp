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

#include "idiom/scheduler.hpp"

namespace idiom {

Scheduler::Scheduler()
    : memo_(NULL),
      sinst_db_(NULL),
      sinst_analyzer_(NULL),
      observer_(NULL),
      observer_new_(NULL) {
  // empty
}

void Scheduler::HandlePreSetup() {
  SchedulerCommon::HandlePreSetup();

  knob_->RegisterBool("ignore_ic_pthread", "do not count instructions in pthread", "1");
  knob_->RegisterBool("ignore_lib", "whether ignore accesses from common libraries", "0");
  knob_->RegisterBool("memo_failed", "whether memoize fail-to-expose iroots", "1");
  knob_->RegisterStr("memo_in", "the input memoization database path", "memo.db");
  knob_->RegisterStr("memo_out", "the output memoization database path", "memo.db");
  knob_->RegisterStr("sinst_in", "the input shared inst database path", "sinst.db");
  knob_->RegisterStr("sinst_out", "the output shared inst database path", "sinst.db");
  knob_->RegisterInt("target_idiom", "the target idiom (0 means any idiom)", "0");

  sinst_analyzer_ = new sinst::SharedInstAnalyzer;
  sinst_analyzer_->Register();
  observer_ = new Observer;
  observer_new_ = new ObserverNew;
  observer_->Register();
  observer_new_->Register();
}

void Scheduler::HandlePostSetup() {
  SchedulerCommon::HandlePostSetup();

  // load memoization db
  memo_ = new Memo(CreateMutex(), iroot_db_);
  memo_->Load(knob_->ValueStr("memo_in"), sinfo_);
  // load shared inst db
  sinst_db_ = new sinst::SharedInstDB(CreateMutex());
  sinst_db_->Load(knob_->ValueStr("sinst_in"), sinfo_);

  if (sinst_analyzer_->Enabled()) {
    // add sinst analyzer
    sinst_analyzer_->Setup(CreateMutex(), sinst_db_);
    AddAnalyzer(sinst_analyzer_);
  }

  // make sure that we only use one type of observer
  if (observer_->Enabled() && observer_new_->Enabled())
    Abort("Please choose an observer.\n");

  if (observer_->Enabled()) {
    // add observer
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, memo_, sinst_db_);
    AddAnalyzer(observer_);
  }

  if (observer_new_->Enabled()) {
    // create iRoot observer (NEW)
    observer_new_->Setup(CreateMutex(), sinfo_, iroot_db_, memo_, sinst_db_);
    AddAnalyzer(observer_new_);
  }
}

bool Scheduler::HandleIgnoreInstCount(IMG img) {
  if (knob_->ValueBool("ignore_ic_pthread")) {
    if (!IMG_Valid(img))
      return false;
    Image *image = sinfo_->FindImage(IMG_Name(img));
    DEBUG_ASSERT(image);
    if (image->IsPthread())
      return true;
  }
  return false;
}

bool Scheduler::HandleIgnoreMemAccess(IMG img) {
  if (!IMG_Valid(img))
    return true;
  Image *image = sinfo_->FindImage(IMG_Name(img));
  DEBUG_ASSERT(image);
  if (image->IsPthread())
    return true;
  if (knob_->ValueBool("ignore_lib")) {
    if (image->IsCommonLib())
      return true;
  }
  return false;
}

void Scheduler::HandleProgramExit() {
  SchedulerCommon::HandleProgramExit();

  // save memoization
  memo_->RefineCandidate(knob_->ValueBool("memo_failed"));
  memo_->Save(knob_->ValueStr("memo_out"), sinfo_);
  // save shared instruction db
  sinst_db_->Save(knob_->ValueStr("sinst_out"), sinfo_);
}

void Scheduler::Choose() {
  // set current iroot to test
  int target_iroot_id = knob_->ValueInt("target_iroot");
  int target_idiom_int = knob_->ValueInt("target_idiom");
  if (target_iroot_id) {
    curr_iroot_ = memo_->ChooseForTest((iroot_id_t)target_iroot_id);
  } else {
    if (target_idiom_int) {
      curr_iroot_ = memo_->ChooseForTest((IdiomType)target_idiom_int);
    } else {
      curr_iroot_ = memo_->ChooseForTest();
    }
  }

  if (!curr_iroot_) {
    printf("No iRoot to test, exit...\n");
    exit(0);
  }
}

void Scheduler::TestSuccess() {
  SchedulerCommon::TestSuccess();
  if (!knob_->ValueInt("target_iroot")) {
    memo_->TestSuccess(curr_iroot_, true);
  }
}

void Scheduler::TestFail() {
  SchedulerCommon::TestFail();
  if (!knob_->ValueInt("target_iroot")) {
    memo_->TestFail(curr_iroot_, false);
  }
}

bool Scheduler::UseDecreasingPriorities() {
  if (knob_->ValueInt("target_iroot")) {
    return history_->TotalTestRuns(curr_iroot_) % 2 == 0;
  } else {
    return memo_->TotalTestRuns(curr_iroot_, true) % 2 == 0;
  }
}

bool Scheduler::YieldWithDelay() {
  if (knob_->ValueBool("yield_with_delay")) {
    if (memo_->Async(curr_iroot_, true)) {
      return true;
    }
  }
  return false;
}

} // namespace idiom

