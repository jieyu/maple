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

// File: randsched/scheduler.cpp - Implementation of the Random scheduler
// controller.

#include "randsched/scheduler.hpp"

#include <errno.h>
#include <sched.h>
#include <sys/resource.h>

#include <cassert>
#include <algorithm>

namespace randsched {

static unsigned long next = 1;

static double random_number() {
  next = next * 1103515245 + 12345;
  unsigned val = (unsigned)(next / 65536) % 32768;
  return (double)val / 32768.0;
}

static void seed_random_number(unsigned long seed) {
  next = seed;
}

Scheduler::Scheduler()
    : history_(NULL),
      delay_(false),
      float_(false),
      chg_pts_cursor_(0),
      delay_unit_(0),
      total_inst_count_(0),
      total_num_threads_(0),
      curr_num_threads_(0),
      start_sched_(false) {
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
  knob_->RegisterBool("delay", "whether inject delay instead of changing priorities at each change point", "0");
  knob_->RegisterBool("float", "whether the number of change points depends on execution length", "1");
  knob_->RegisterInt("float_interval", "average number of memory accesses between two change points", "50000");
  knob_->RegisterInt("num_chg_pts", "number of change points (when float is set to False))", "3");
  knob_->RegisterStr("rand_history", "the rand history file path", "rand.histo");
}

void Scheduler::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // load rand history
  history_ = new History;
  history_->Load(knob_->ValueStr("rand_history"));

  // set flags
  delay_ = knob_->ValueBool("delay");
  float_ = knob_->ValueBool("float");

  Randomize();
}

void Scheduler::HandlePostInstrumentTrace(TRACE trace) {
  ExecutionControl::HandlePostInstrumentTrace(trace);

  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
        if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
          continue; // skip stack accesses

        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(__Change),
                       IARG_UINT32, 1,
                       IARG_END);
      }
    }
  }
}

void Scheduler::HandleProgramExit() {
  history_->Update(total_inst_count_, total_num_threads_);
  history_->Save(knob_->ValueStr("rand_history"));

  ExecutionControl::HandleProgramExit();
}

void Scheduler::HandleThreadStart() {
  ATOMIC_ADD_AND_FETCH(&curr_num_threads_, 1);
  ATOMIC_ADD_AND_FETCH(&total_num_threads_, 1);

  if (main_thread_started_) {
    // child thread
    start_sched_ = true;
    if (!delay_) {
      int priority = RandomPriority();
      SetPriority(priority);
    }
  } else {
    // main thread
    if (!delay_) {
      SetAffinity(); // force all the threads to be executed on one processor
      int priority = RandomPriority();
      SetPriority(priority);
    }
  }

  ExecutionControl::HandleThreadStart();
}

void Scheduler::HandleThreadExit() {
  if (ATOMIC_SUB_AND_FETCH(&curr_num_threads_, 1) <= 1)
    start_sched_ = false;

  ExecutionControl::HandleThreadExit();
}

void Scheduler::HandleChange(UINT32 c) {
  if (start_sched_) {
    unsigned long k = ATOMIC_ADD_AND_FETCH(&total_inst_count_, c);
    if (NeedChange(k)) {
      if (delay_) {
        // inject delay
        RandomDelay();
      } else {
        // change priority
        int priority = RandomPriority();
        SetPriority(priority);
      }
    }
  }
}

bool Scheduler::NeedChange(unsigned long k) {
  int cursor = chg_pts_cursor_;
  if (cursor < (int)chg_pts_vec_.size()) {
    if (k >= chg_pts_vec_[cursor]) {
      ATOMIC_ADD_AND_FETCH(&chg_pts_cursor_, 1);
      return true;
    }
  }
  return false;
}

int Scheduler::RandomPriority() {
  size_t size = prio_vec_.size();
  assert(size > 0);
  size_t idx = (size_t)((double)size * random_number());
  return prio_vec_[idx];
}

void Scheduler::RandomDelay() {
  useconds_t t = (useconds_t)((double)delay_unit_ * random_number());
  DEBUG_FMT_PRINT_SAFE("[T%lx] Delay time = %lu microseconds\n",
                       PIN_ThreadUid(), (unsigned long)t);
  usleep(t);
}

void Scheduler::Randomize() {
  srand(unsigned(time(NULL)));
  seed_random_number(unsigned(time(NULL)));

  if (!delay_) {
    if (knob_->ValueBool("strict")) {
      int low = knob_->ValueInt("lowest_realtime_priority");
      int high = knob_->ValueInt("highest_realtime_priority");
      for (int prio = low; prio <= high; prio++) {
        prio_vec_.push_back(prio);
      }
    } else {
      int low = knob_->ValueInt("lowest_nice_value");
      int high = knob_->ValueInt("highest_nice_value");
      for (int prio = high; prio >= low; prio--) {
        prio_vec_.push_back(prio);
      }
    }
  }

  if (!history_->Empty()) {
    unsigned long avg_inst_count = history_->AvgInstCount();
    DEBUG_FMT_PRINT_SAFE("Average mem inst count = %lu\n", avg_inst_count);

    int num_chg_pts = 0;
    if (float_) {
      int float_interval = knob_->ValueInt("float_interval");
      num_chg_pts = avg_inst_count / float_interval + 1;
      delay_unit_ = 50 * 1000;
    } else {
      num_chg_pts = knob_->ValueInt("num_chg_pts");
      delay_unit_ = avg_inst_count / 500 + 1;
    }
    DEBUG_FMT_PRINT_SAFE("Number of change points = %d\n", num_chg_pts);
    DEBUG_FMT_PRINT_SAFE("Max delay unit = %d microseconds\n", delay_unit_);

    // set change points vector
    for (int i = 0; i < num_chg_pts; i++) {
      unsigned long inst_count =
          (unsigned long)((double)avg_inst_count * (rand() / (RAND_MAX + 1.0)));
      chg_pts_vec_.push_back(inst_count);
    }
    sort(chg_pts_vec_.begin(), chg_pts_vec_.end());
  }
}

void Scheduler::SetPriority(int priority) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set priority = %d\n",
                       PIN_ThreadUid(), priority);
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
    Abort("SetStrictPriority failed\n");
  }
}

void Scheduler::SetRelaxPriority(int priority) {
  if (setpriority(PRIO_PROCESS, 0, priority)) {
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

void Scheduler::__Change(UINT32 c) {
  ((Scheduler *)ctrl_)->HandleChange(c);
}

} // namespace randsched

