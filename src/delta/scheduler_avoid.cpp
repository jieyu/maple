// Eden - A dynamic program analysis framework.
//
// Copyright (C) 2011 Jie Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: delta/scheduler.hpp - Implementation of the reorder scheduler controller.

#include "delta/scheduler_avoid.hpp"

#include <sched.h>
#include <sys/resource.h>
#include <algorithm>

#include "delta/slist.h"
#include "core/atomic.h"
#include "core/debug_analyzer.h"

namespace delta {

Scheduler::Scheduler()
    : depth_(3),
      new_thread_priorities_cursor_(0),
      start_schedule_(false),
      curr_iroot_(NULL),
      fence_inst(0),
      highest_priority_(0),
      higher_priority_(0),
      normal_priority_(0),
      lower_priority_(0),
      lowest_priority_(0),
      observer_(NULL),
      iroot_db_(NULL),
      ilist_(NULL),
      slist_(NULL){
  //empty
  }

void Scheduler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterInt("lowest_realtime_priority", "The lowest realtime priority", "1");
  knob_->RegisterInt("highest_realtime_priority", "The highest realtime priority", "99");
  knob_->RegisterInt("cpu", "Specify which cpu to run on.", "0");
  knob_->RegisterBool("ignore_lib", "Ignore all the accesses in the common libraries.", "0");
  knob_->RegisterStr("iroot_in", "The input memorization database.", "iroot.db");
  knob_->RegisterStr("iroot_out", "The output memorization database.", "iroot.db");
  knob_->RegisterStr("ilist_out", "The output memorization database.", "ilist.db");  
  knob_->RegisterInt("suspect_iterator", "choose one pair from suspect list to replay.", "0");
  knob_->RegisterStr("suspect_list", "Candidate root causes", "slist.in");
  knob_->RegisterInt("idiom_type", "which idiom type of suspect candidates", "1");
  knob_->RegisterInt("target_iroot", "The target iroot.", "0");
  
  observer_ = new Observer(knob_);
  observer_->Register();
}

void Scheduler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  desc_.SetHookMainFunc();
  desc_.SetHookBeforeMem();
  desc_.SetHookAfterMem();

  iroot_db_ = new idiom::iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);
  ilist_ = new iList(CreateMutex());
  if (observer_->Enabled()){
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, ilist_);
    AddAnalyzer(observer_);
  }

  slist_ = new SuspectList(CreateMutex(), iroot_db_);
  //slist_->Load(knob_->ValueStr("suspect_list"), sinfo_);
  
  //if (slist_->Empty())
  //  DEBUG_FMT_PRINT_SAFE("___\"suspect.list\" is EMPTY___\n");
  //else {
  //address_t suspect_iterator = knob_->ValueInt("suspect_iterator");
  //suspect_ = slist_->GetSuspect(suspect_iterator);
  //for (std::vector<inst_id_type>::iterator it = suspect_.begin(); 
  //     it != suspect_.end(); ++it) {
  //	DEBUG_FMT_PRINT("id = %x\n", *it);
	//fence_inst = suspect_[0];
  //}
  //}

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

  idiom::iRootEvent *iroot_event_ = curr_iroot_->GetEvent(0);
  Inst *inst_ = iroot_event_->inst();
  fence_inst = inst_->id();

  printf("[Active Scheduler] Target iroot = %u, target idiom = %d\n",
         curr_iroot_->id(), (int)curr_iroot_->idiom());
  DEBUG_FMT_PRINT("fence_inst id is %d\n", fence_inst);

  Randomize();
}

bool Scheduler::HandleIgnoreMemAccess(IMG img) {
  if (!IMG_Valid(img))
    return true;

  if (IMG_Name(img).find("libpthread") != std::string::npos)
    return true;

  if (knob_->ValueBool("ignore_lib")) {
    if ((IMG_Name(img).find("libc") != std::string::npos) ||
        (IMG_Name(img).find("libpthread") != std::string::npos) ||
        (IMG_Name(img).find("librt") != std::string::npos) ||
        (IMG_Name(img).find("ld-") != std::string::npos) ||
        (IMG_Name(img).find("libstdc++") != std::string::npos) ||
        (IMG_Name(img).find("libgcc_s") != std::string::npos) ||
        (IMG_Name(img).find("libm") != std::string::npos) ||
        (IMG_Name(img).find("libnsl") != std::string::npos) ||
        (IMG_Name(img).find("librt") != std::string::npos) ||
        (IMG_Name(img).find("libdl") != std::string::npos) ||
        (IMG_Name(img).find("libz") != std::string::npos) ||
        (IMG_Name(img).find("libcrypt") != std::string::npos) ||
        (IMG_Name(img).find("libdb") != std::string::npos) ||
        (IMG_Name(img).find("libexpat") != std::string::npos) ||
        (IMG_Name(img).find("libnss") != std::string::npos) ||
        (IMG_Name(img).find("libresolv") != std::string::npos) ||
        (IMG_Name(img).find("libbz2") != std::string::npos)) 
      return true;
  }

  return false;
}

void Scheduler::HandlePreInstrumentTrace(TRACE trace) {
  ExecutionControl::HandlePreInstrumentTrace(trace);
  
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
      for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
          if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
            continue; // skip stack accesses

          Inst *inst = GetInst(INS_Address(ins));
	  uint32 curr_inst_id = inst->id();
	  if (curr_inst_id == fence_inst) {
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(__PriorityBeforeChange),
                         IARG_THREAD_ID,
                         IARG_PTR, inst,
                         IARG_END);
	  
	  	}
          }
       }
    }
}

void Scheduler::HandleProgramExit() {
  
  ExecutionControl::HandleProgramExit();

  // output iroot.db
  iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
  // output ilist.db
  ilist_->Save(knob_->ValueStr("ilist_out"), sinfo_);
}

void Scheduler::HandleThreadStart() {

  ExecutionControl::HandleThreadStart();
}

void Scheduler::HandleThreadExit() {
  ExecutionControl::HandleThreadExit();
  
}

void Scheduler::HandleMain(THREADID tid, CONTEXT * ctxt) {
  SetAffinity();

  //default assign higherpriority 51 to main thread
  SetPriority(HigherPriority());

  // start scheduling
  start_schedule_ = true;
  ExecutionControl::HandleMain(tid, ctxt);
}

void Scheduler::HandleThreadMain(THREADID tid,CONTEXT * ctxt) {
  //default assign NormalPriority 50 to child thread
  SetPriority(NormalPriority());
  
  ExecutionControl::HandleThreadMain(tid, ctxt);
}

void Scheduler::HandlePriorityBeforeChange(THREADID tid, Inst *inst) {
  if (!start_schedule_)
    return;

	uint32 curr_inst_id = inst->id();
	if (curr_inst_id== fence_inst) {
	  DEBUG_FMT_PRINT_SAFE("===>[T%d] Call Analyzer, Before inst_id = %x\n", tid, curr_inst_id);
	  SetPriority(LowestPriority());
	}

}

int Scheduler::NextNewThreadPriority() {
  int cursor = ATOMIC_FETCH_AND_ADD(&new_thread_priorities_cursor_, 1);
  return new_thread_priorities_[cursor % new_thread_priorities_.size()];
}

void Scheduler::Randomize() {
  int low = knob_->ValueInt("lowest_realtime_priority");
  int high = knob_->ValueInt("highest_realtime_priority");
  normal_priority_ = (low + high) / 2;
  highest_priority_ = normal_priority_ + 2;
  higher_priority_ = normal_priority_ +1;
  lower_priority_ = normal_priority_ - 1;
  lowest_priority_ = normal_priority_ -2;
  for (int i = 0; i < high - low - depth_ + 2 ; i++) {
    new_thread_priorities_.push_back(low + depth_ - 1 + i);
  }
  
  srand(unsigned(time(NULL)));
  random_shuffle(new_thread_priorities_.begin(), new_thread_priorities_.end());
}

void Scheduler::SetPriority(int priority) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set strict priority = %d\n", PIN_ThreadUid(), priority);
  struct sched_param param;
  param.sched_priority = priority;
  if (sched_setscheduler(0, SCHED_FIFO, &param)) {
    Abort("SetStrictPriority failed\n");
  }
}

void Scheduler::SetAffinity() {
  int cpu = knob_->ValueInt("cpu");
  if (cpu < 0 || cpu >= sysconf(_SC_NPROCESSORS_ONLN))
    cpu = 0;

  DEBUG_FMT_PRINT_SAFE("Setting affinity to cpu%d\n", cpu);

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)) {
    Abort("SetAffinity failed\n");
  }
}

void Scheduler::__PriorityBeforeChange(THREADID tid, Inst *inst) {
  ((Scheduler *)ctrl_)->HandlePriorityBeforeChange(tid, inst);
}

}// namespace
