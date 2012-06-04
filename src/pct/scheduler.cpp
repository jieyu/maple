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

// File: pct/scheduler.cpp - Implementation of the PCT scheduler
// controller.

#include "pct/scheduler.hpp"

#include <errno.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>

#include <algorithm>

namespace pct {

Scheduler::Scheduler()
    : history_(NULL),
      depth_(1),
      change_points_cursor_(0),
      change_priorities_cursor_(0),
      new_thread_priorities_cursor_(0),
      total_inst_count_(0),
      total_num_threads_(0),
      curr_num_threads_(0),
      start_inst_count_(false) {
  // empty
}

Scheduler::~Scheduler() {
  // empty
}

void Scheduler::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("strict", "whether use non-preemptive priorities", "1");
  knob_->RegisterInt("lowest_realtime_priority", "the lowest realtime priority", "1");
  knob_->RegisterInt("highest_realtime_priority", "the highest realtime priority", "99");
  knob_->RegisterInt("lowest_nice_value", "the lowest nice value (high priority)", "-20");
  knob_->RegisterInt("highest_nice_value", "the highest nice value (low priority)", "19");
  knob_->RegisterInt("cpu", "which cpu to run on", "0");
  knob_->RegisterInt("depth", "the target bug depth", "3");
  knob_->RegisterBool("count_mem", "whether use the number of memory accesses as thread counter", "1");
  knob_->RegisterStr("pct_history", "the pct history file path", "pct.histo");
}

void Scheduler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // set analysis desc
  if (knob_->ValueBool("strict")) {
    desc_.SetHookSyscall();
  }

  // load pct history
  history_ = new History;
  history_->Load(knob_->ValueStr("pct_history"));

  // setup depth
  if (history_->Empty())
    depth_ = 1;
  else
    depth_ = knob_->ValueInt("depth");

  Randomize();
}

void Scheduler::HandlePostInstrumentTrace(TRACE trace) {
  ExecutionControl::HandlePostInstrumentTrace(trace);

  if (knob_->ValueBool("count_mem")) {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
      for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
          if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
            continue; // skip stack accesses

          INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(__PriorityChange),
                         IARG_UINT32, 1,
                         IARG_END);
        }
      }
    }
  } else {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
      BBL_InsertCall(bbl, IPOINT_BEFORE, AFUNPTR(__PriorityChange),
                     IARG_UINT32, BBL_NumIns(bbl),
                     IARG_END);
    }
  }
}

void Scheduler::HandleSyscallEntry(THREADID tid, CONTEXT *ctxt,
                                   SYSCALL_STANDARD std) {
  ExecutionControl::HandleSyscallEntry(tid, ctxt, std);

  int syscall_num = tls_syscall_num_[tid];
  switch (syscall_num) {
    case SYS_sched_yield:
      if (knob_->ValueBool("strict"))
        SetStrictPriority(knob_->ValueInt("lowest_realtime_priority"));
      break;
    default:
      break;
  }
}

void Scheduler::HandleProgramExit() {
  history_->Update(total_inst_count_, total_num_threads_);
  history_->Save(knob_->ValueStr("pct_history"));

  ExecutionControl::HandleProgramExit();
}

void Scheduler::HandleThreadStart() {
  ATOMIC_ADD_AND_FETCH(&curr_num_threads_, 1);
  ATOMIC_ADD_AND_FETCH(&total_num_threads_, 1);

  if (main_thread_started_) {
    // child thread
    start_inst_count_ = true;
    int priority = NextNewThreadPriority();
    SetPriority(priority);
  } else {
    // main thread
    SetAffinity(); // force all the threads to be executed on one processor
    int priority = NextNewThreadPriority();
    SetPriority(priority);
  }

  ExecutionControl::HandleThreadStart();
}

void Scheduler::HandleThreadExit() {
  if (ATOMIC_SUB_AND_FETCH(&curr_num_threads_, 1) <= 1)
    start_inst_count_ = false;

  ExecutionControl::HandleThreadExit();
}

void Scheduler::HandlePriorityChange(UINT32 c) {
  if (start_inst_count_) {
    unsigned long k = ATOMIC_ADD_AND_FETCH(&total_inst_count_, c);
    if (NeedPriorityChange(k)) {
      // change priority
      int priority = NextChangePriority();
      SetPriority(priority);
    }
  }
}

bool Scheduler::NeedPriorityChange(unsigned long k) {
  int cursor = change_points_cursor_;
  if (cursor < depth_ - 1) {
    if (k >= priority_change_points_[cursor]) {
      ATOMIC_ADD_AND_FETCH(&change_points_cursor_, 1);
      return true;
    }
  }
  return false;
}

int Scheduler::NextNewThreadPriority() {
  int cursor = ATOMIC_FETCH_AND_ADD(&new_thread_priorities_cursor_, 1);
  return new_thread_priorities_[cursor % new_thread_priorities_.size()];
}

int Scheduler::NextChangePriority() {
  int cursor = ATOMIC_FETCH_AND_ADD(&change_priorities_cursor_, 1);
  return change_priorities_[cursor % change_priorities_.size()];
}

void Scheduler::Randomize() {
  srand(unsigned(time(NULL)));

  if (knob_->ValueBool("strict")) {
    // fill change priorities
    int low = knob_->ValueInt("lowest_realtime_priority");
    int high = knob_->ValueInt("highest_realtime_priority");
    for (int i = depth_ - 2; i >= 0; i--) {
      change_priorities_.push_back(low + i);
    }

    // fill new thread priorities
    for (int i = 0; i < high - low - depth_ + 2; i++) {
      new_thread_priorities_.push_back(low + depth_ - 1 + i);
    }
  } else {
    // fill change priorities
    int low = knob_->ValueInt("lowest_nice_value");
    int high = knob_->ValueInt("highest_nice_value");
    for (int i = depth_ - 2; i >= 0; i--) {
      change_priorities_.push_back(high - i);
    }

    // fill new thread priorities
    for (int i = 0; i < high - low - depth_ + 2; i++) {
      new_thread_priorities_.push_back(low + i);
    }
  }

  random_shuffle(new_thread_priorities_.begin(), new_thread_priorities_.end());

  // randomize priority change points
  unsigned long avg_inst_count = history_->AvgInstCount();
  for (int i = 1; i < depth_; i++) {
    unsigned long inst_count =
        (unsigned long)((double)avg_inst_count * (rand() / (RAND_MAX + 1.0)));
    priority_change_points_.push_back(inst_count);
  }
  sort(priority_change_points_.begin(), priority_change_points_.end());
}

void Scheduler::SetPriority(int priority) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set priority = %d\n", PIN_ThreadUid(), priority);
  if (knob_->ValueBool("strict")) {
    SetStrictPriority(priority);
  } else {
    SetRelaxPriority(priority);
  }
}

void Scheduler::SetStrictPriority(int priority) {
  struct sched_param param;
  param.sched_priority = priority;
  if (sched_setscheduler(0, SCHED_FIFO, &param)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetStrictPriority failed\n");
  }
}

void Scheduler::SetRelaxPriority(int priority) {
  if (setpriority(PRIO_PROCESS, 0, priority)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetRelaxPriority failed\n");
  }
}

void Scheduler::SetAffinity() {
  int cpu = knob_->ValueInt("cpu");
  if (cpu < 0 || cpu >= sysconf(_SC_NPROCESSORS_ONLN))
    cpu = 0;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);

  if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset)) {
    Abort("SetAffinity failed\n");
  }
}

void Scheduler::__PriorityChange(UINT32 c) {
  ((Scheduler *)ctrl_)->HandlePriorityChange(c);
}

} // namespace pct

