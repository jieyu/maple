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

// File: idiom/scheduler_common.cpp - Implementation of the common active
// scheduler controller.

#include "idiom/scheduler_common.hpp"

#include <errno.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <cstdlib>

#include "core/pin_util.hpp"
#include "core/stat.h"

#define __OVERLAP(addr,size,addr2,size2) \
    ((addr) < (addr2) + (size2) && (addr2) < (addr) + (size))
#define OVERLAP(addr,size,addr2,size2) __OVERLAP(addr,size,addr2,size2)
#if 0
#define OVERLAP(addr,size,addr2,size2) \
    __OVERLAP(UNIT_DOWN_ALIGN(addr,unit_size_), \
              UNIT_UP_ALIGN(addr+size,unit_size_) \
                  - UNIT_DOWN_ALIGN(addr,unit_size_), \
              UNIT_DOWN_ALIGN(addr2,unit_size_), \
              UNIT_UP_ALIGN(addr2+size2,unit_size_) \
                  - UNIT_DOWN_ALIGN(addr2,4))
#endif

namespace idiom {

#ifndef _USING_SYS_RAND_GEN
static unsigned long next = 1;

static double random_number() {
  next = next * 1103515245 + 12345;
  unsigned val = (unsigned)(next / 65536) % 32768;
  return (double)val / 32768.0;
}

static void seed_random_number(unsigned long seed) {
  next = seed;
}
#endif

Idiom1SchedStatus::Idiom1SchedStatus() {
  state_ = IDIOM1_STATE_INIT;
  thd_id_[0] = INVALID_THD_ID;
  thd_id_[1] = INVALID_THD_ID;
  addr_[0] = 0;
  addr_[1] = 0;
  size_[0] = 0;
  size_[1] = 0;
}

Idiom2SchedStatus::Idiom2SchedStatus() {
  state_ = IDIOM2_STATE_INIT;
  thd_id_[0] = INVALID_THD_ID;
  thd_id_[1] = INVALID_THD_ID;
  thd_id_[2] = INVALID_THD_ID;
  addr_[0] = 0;
  addr_[1] = 0;
  addr_[2] = 0;
  size_[0] = 0;
  size_[1] = 0;
  size_[2] = 0;
  window_ = 0;
}

Idiom3SchedStatus::Idiom3SchedStatus() {
  state_ = IDIOM3_STATE_INIT;
  thd_id_[0] = INVALID_THD_ID;
  thd_id_[1] = INVALID_THD_ID;
  thd_id_[2] = INVALID_THD_ID;
  thd_id_[3] = INVALID_THD_ID;
  addr_[0] = 0;
  addr_[1] = 0;
  addr_[2] = 0;
  addr_[3] = 0;
  size_[0] = 0;
  size_[1] = 0;
  size_[2] = 0;
  size_[3] = 0;
  window_ = 0;
}

Idiom4SchedStatus::Idiom4SchedStatus() {
  state_ = IDIOM4_STATE_INIT;
  thd_id_[0] = INVALID_THD_ID;
  thd_id_[1] = INVALID_THD_ID;
  thd_id_[2] = INVALID_THD_ID;
  thd_id_[3] = INVALID_THD_ID;
  addr_[0] = 0;
  addr_[1] = 0;
  addr_[2] = 0;
  addr_[3] = 0;
  size_[0] = 0;
  size_[1] = 0;
  size_[2] = 0;
  size_[3] = 0;
  window_ = 0;
}

Idiom5SchedStatus::Idiom5SchedStatus() {
  state_ = IDIOM5_STATE_INIT;
  thd_id_[0] = INVALID_THD_ID;
  thd_id_[1] = INVALID_THD_ID;
  thd_id_[2] = INVALID_THD_ID;
  thd_id_[3] = INVALID_THD_ID;
  addr_[0] = 0;
  addr_[1] = 0;
  addr_[2] = 0;
  addr_[3] = 0;
  size_[0] = 0;
  size_[1] = 0;
  size_[2] = 0;
  size_[3] = 0;
  window_[0] = 0;
  window_[1] = 0;
}

SchedulerCommon::SchedulerCommon()
    : iroot_db_(NULL),
      history_(NULL),
      normal_priority_(0),
      lower_priority_(0),
      higher_priority_(0),
      min_priority_(0),
      max_priority_(0),
      new_thread_priorities_cursor_(0),
      unit_size_(0),
      vw_(0),
      curr_iroot_(NULL),
      idiom1_sched_status_(NULL),
      idiom2_sched_status_(NULL),
      idiom3_sched_status_(NULL),
      idiom4_sched_status_(NULL),
      idiom5_sched_status_(NULL),
      sched_status_lock_(NULL),
      misc_lock_(NULL),
      start_schedule_(false),
      test_success_(false) {
  // empty
}

void SchedulerCommon::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("strict", "whether use non-preemptive priorities", "1");
  knob_->RegisterInt("lowest_realtime_priority", "the lowest realtime priority", "1");
  knob_->RegisterInt("highest_realtime_priority", "the highest realtime priority", "99");
  knob_->RegisterInt("lowest_nice_value", "the lowest nice value (high priority)", "-20");
  knob_->RegisterInt("highest_nice_value", "the highest nice value (low priority)", "19");
  knob_->RegisterInt("cpu", "which cpu to run on", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("vw", "the vulnerability window (# dynamic inst)", "1000");
  knob_->RegisterStr("iroot_in", "the input iroot database path", "iroot.db");
  knob_->RegisterStr("iroot_out", "the output iroot database path", "iroot.db");
  knob_->RegisterStr("test_history", "the test history file path", "test.histo");
  knob_->RegisterInt("target_iroot", "the target iroot (0 means choosing any)", "0");
  knob_->RegisterBool("yield_with_delay", "whether inject delays for async iroots", "1");
  knob_->RegisterInt("yield_delay_unit", "the unit time of each delay (in millisecond)", "100");
  knob_->RegisterInt("yield_delay_min_each", "the minimal delay each event is guaranteed (in millisecond)", "1000");
  knob_->RegisterInt("yield_delay_max_total", "the maximum delay for all events (in millisecond)", "5000");
  knob_->RegisterBool("ordered_new_thread_prio", "whether assign ordered priority to each new thread", "1");
  knob_->RegisterInt("random_seed", "the random seed (0 means using current time)", "0");
}

void SchedulerCommon::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // set analysis desc
  desc_.SetHookMainFunc();
  desc_.SetHookSyscall();

  // load iroot db
  iroot_db_ = new iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);
  // load test history
  history_ = new TestHistory;
  history_->Load(knob_->ValueStr("test_history"));

  // calculate priorities
  CalculatePriorities();

  // init random number generator
  random_seed_ = (unsigned int)knob_->ValueInt("random_seed");
  if (random_seed_ == 0) {
    random_seed_ = (unsigned int)time(NULL);
#ifdef _USING_SYS_RAND_GEN
    srand(random_seed_);
#else
    seed_random_number(random_seed_);
#endif
  } else {
#ifdef _USING_SYS_RAND_GEN
    srand(random_seed_);
#else
    seed_random_number(random_seed_);
#endif
  }

  // set unit size
  unit_size_ = knob_->ValueInt("unit_size");

  // set vulnerability window
  vw_ = knob_->ValueInt("vw");

  // create mutexes
  sched_status_lock_ = CreateMutex();
  misc_lock_ = CreateMutex();
}

void SchedulerCommon::HandlePreInstrumentTrace(TRACE trace) {
  ExecutionControl::HandlePreInstrumentTrace(trace);

  // no need to instrument if no memory iroot event exists
  if (curr_iroot_->HasMem()) {
    InstrumentMemiRootEvent(trace);
    InstrumentWatchInstCount(trace);
    InstrumentWatchMem(trace);
  } else {
    InstrumentWatchInstCount(trace);
  }
}

void SchedulerCommon::HandleImageLoad(IMG img, Image *image) {
  if (!desc_.HookPthreadFunc()) {
    // no need to wrap mutex functions if no sync iroot event exists
    if (curr_iroot_->HasSync()) {
      ReplacePthreadMutexWrappers(img);
    }
  }

  ExecutionControl::HandleImageLoad(img, image);
}

void SchedulerCommon::HandleSyscallEntry(THREADID tid, CONTEXT *ctxt,
                                   SYSCALL_STANDARD std) {
  ExecutionControl::HandleSyscallEntry(tid, ctxt, std);

  int syscall_num = tls_syscall_num_[tid];
  switch (syscall_num) {
    case SYS_sched_yield:
      HandleSchedYield();
      break;
    default:
      break;
  }
}

void SchedulerCommon::HandleProgramStart() {
  // set current iroot to test
  Choose();
  DEBUG_ASSERT(curr_iroot_);
  history_->CreateEntry(curr_iroot_);
  history_->UpdateSeed(random_seed_);
  history_->Save(knob_->ValueStr("test_history"));

  // set sched status
  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      idiom1_sched_status_ = new Idiom1SchedStatus;
      break;
    case IDIOM_2:
      idiom2_sched_status_ = new Idiom2SchedStatus;
      break;
    case IDIOM_3:
      idiom3_sched_status_ = new Idiom3SchedStatus;
      break;
    case IDIOM_4:
      idiom4_sched_status_ = new Idiom4SchedStatus;
      break;
    case IDIOM_5:
      idiom5_sched_status_ = new Idiom5SchedStatus;
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleProgramExit() {
  ExecutionControl::HandleProgramExit();

  if (!test_success_) {
    TestFail();
    history_->UpdateResult(false);
  }

  // save test history
  history_->Save(knob_->ValueStr("test_history"));
  // save iroot db
  iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
}

void SchedulerCommon::HandleThreadStart() {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  OS_THREAD_ID os_tid = PIN_GetTid();
  DEBUG_FMT_PRINT_SAFE("[T%lx] Thread start\n", curr_thd_id);
  LockMisc();
  thd_id_os_tid_map_[curr_thd_id] = os_tid;
  UnlockMisc();

  // set initial priorities for child thread
  if (main_thread_started_) {
    // get child thread init priority
    int priority = NextNewThreadPriority();
    // save the priority
    LockMisc();
    ori_priority_map_[curr_thd_id] = priority;
    UnlockMisc();
    SetPriority(curr_thd_id, priority);
  }

  ExecutionControl::HandleThreadStart();
}

void SchedulerCommon::HandleThreadExit() {
  ExecutionControl::HandleThreadExit();
  thread_id_t curr_thd_id = PIN_ThreadUid();
  LockMisc();
  thd_id_os_tid_map_.erase(curr_thd_id);
  UnlockMisc();
  DEBUG_FMT_PRINT_SAFE("[T%lx] Thread exit\n", PIN_ThreadUid());
}

void SchedulerCommon::HandleMain(THREADID tid, CONTEXT *ctxt) {
  ExecutionControl::HandleMain(tid, ctxt);
  // start scheduling
  start_schedule_ = true;
  // force all the threads to be executed on one processor
  SetAffinity();
  // set new thread priorities cursor
  InitNewThreadPriority();
  // get main thread priority
  int priority = MainThreadPriority();
  thread_id_t curr_thd_id = PIN_ThreadUid();
  // save the priority
  LockMisc();
  ori_priority_map_[curr_thd_id] = priority;
  UnlockMisc();
  SetPriority(curr_thd_id, priority);
}

void SchedulerCommon::HandleThreadMain(THREADID tid, CONTEXT *ctxt) {
  ExecutionControl::HandleThreadMain(tid, ctxt);
}

void SchedulerCommon::Choose() {
  // this function should setup the curr_iroot_ field
  int target_iroot_id = knob_->ValueInt("target_iroot");
  curr_iroot_ = iroot_db_->FindiRoot((iroot_id_t)target_iroot_id, false);
  if (!curr_iroot_) {
    Abort("target iroot invalid\n");
  }
}

bool SchedulerCommon::UseDecreasingPriorities() {
  return history_->TotalTestRuns(curr_iroot_) % 2 == 0;
}

bool SchedulerCommon::YieldWithDelay() {
  if (knob_->ValueBool("yield_with_delay"))
    return true;
  else
    return false;
}

void SchedulerCommon::InstrumentMemiRootEvent(TRACE trace) {
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    int idx = size - 1 - i;
    iRootEvent *e = curr_iroot_->GetEvent(idx);
    if (e->IsMem())
      InstrumentMemiRootEvent(trace, idx);
  }
}

void SchedulerCommon::InstrumentMemiRootEvent(TRACE trace, UINT32 idx) {
  iRootEvent *e = curr_iroot_->GetEvent(idx);

  DEBUG_ASSERT(e->IsMem());

  Inst *inst = e->inst();
  DEBUG_ASSERT(inst);
  Image *image = inst->image();
  DEBUG_ASSERT(image);
  IMG img = GetImgByTrace(trace);

  // no need to proceed if image does not match
  if (IMG_Valid(img)) {
    if (image->name().compare(IMG_Name(img)) != 0)
      return;
  } else {
    if (image->name().compare(PSEUDO_IMAGE_NAME) != 0)
      return;
  }

  ADDRINT img_low_addr = IMG_Valid(img) ? IMG_LowAddress(img) : 0;
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      ADDRINT offset = INS_Address(ins) - img_low_addr;
      // if instruction matches
      if (offset == inst->offset()) {
        if (INS_IsMemoryRead(ins)) {
          INS_InsertCall(ins, IPOINT_BEFORE,
                         (AFUNPTR)__BeforeiRootMemRead,
                         IARG_UINT32, idx,
                         IARG_MEMORYREAD_EA,
                         IARG_MEMORYREAD_SIZE,
                         IARG_END);
        }

        if (INS_IsMemoryWrite(ins)) {
          INS_InsertCall(ins, IPOINT_BEFORE,
                         (AFUNPTR)__BeforeiRootMemWrite,
                         IARG_UINT32, idx,
                         IARG_MEMORYWRITE_EA,
                         IARG_MEMORYWRITE_SIZE,
                         IARG_END);
        }

        if (INS_HasMemoryRead2(ins)) {
          INS_InsertCall(ins, IPOINT_BEFORE,
                         (AFUNPTR)__BeforeiRootMemRead,
                         IARG_UINT32, idx,
                         IARG_MEMORYREAD2_EA,
                         IARG_MEMORYREAD_SIZE,
                         IARG_END);
        }

        if (INS_HasFallThrough(ins)) {
          INS_InsertCall(ins, IPOINT_AFTER,
                         (AFUNPTR)__AfteriRootMem,
                         IARG_UINT32, idx,
                         IARG_END);
        }

        if (INS_IsBranchOrCall(ins)) {
          INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
                         (AFUNPTR)__AfteriRootMem,
                         IARG_UINT32, idx,
                         IARG_END);
        }

      } // end of if (offset == inst->offset())
    } // end of for ins
  } // end of for bbl
}

void SchedulerCommon::ReplacePthreadMutexWrappers(IMG img) {
  ACTIVATE_WRAPPER_HANDLER(PthreadMutexLock);
  ACTIVATE_WRAPPER_HANDLER(PthreadMutexUnlock);
}

void SchedulerCommon::CheckiRootBeforeMutexLock(Inst *inst, address_t addr) {
  bool is_candidate = false;
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    int idx = size - 1 - i;
    iRootEvent *e = curr_iroot_->GetEvent(idx);
    if (e->type() == IROOT_EVENT_MUTEX_LOCK && e->inst() == inst) {
      HandleBeforeiRootMutexLock(idx, addr);
      is_candidate = true;
    }
  }
  if (!is_candidate)
    HandleWatchMutexLock(addr);
}

void SchedulerCommon::CheckiRootAfterMutexLock(Inst *inst, address_t addr) {
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    int idx = size - 1 - i;
    iRootEvent *e = curr_iroot_->GetEvent(idx);
    if (e->type() == IROOT_EVENT_MUTEX_LOCK && e->inst() == inst) {
      HandleAfteriRootMutexLock(idx, addr);
    }
  }
}

void SchedulerCommon::CheckiRootBeforeMutexUnlock(Inst *inst, address_t addr) {
  bool is_candidate = false;
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    int idx = size - 1 - i;
    iRootEvent *e = curr_iroot_->GetEvent(idx);
    if (e->type() == IROOT_EVENT_MUTEX_UNLOCK && e->inst() == inst) {
      HandleBeforeiRootMutexUnlock(idx, addr);
      is_candidate = true;
    }
  }
  if (!is_candidate)
    HandleWatchMutexUnlock(addr);
}

void SchedulerCommon::CheckiRootAfterMutexUnlock(Inst *inst, address_t addr) {
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    int idx = size - 1 - i;
    iRootEvent *e = curr_iroot_->GetEvent(idx);
    if (e->type() == IROOT_EVENT_MUTEX_UNLOCK && e->inst() == inst) {
      HandleAfteriRootMutexUnlock(idx, addr);
    }
  }
}

void SchedulerCommon::InstrumentWatchMem(TRACE trace) {
  switch (curr_iroot_->idiom()) {
      case IDIOM_1:
        Idiom1InstrumentWatchMem(trace);
        break;
      case IDIOM_2:
        Idiom2InstrumentWatchMem(trace);
        break;
      case IDIOM_3:
        Idiom3InstrumentWatchMem(trace);
        break;
      case IDIOM_4:
        Idiom4InstrumentWatchMem(trace);
        break;
      case IDIOM_5:
        Idiom5InstrumentWatchMem(trace);
        break;
      default:
        break;
  }
}

void SchedulerCommon::InstrumentWatchInstCount(TRACE trace) {
  switch (curr_iroot_->idiom()) {
      case IDIOM_1:
        // no need to instrumet
        break;
      case IDIOM_2:
        Idiom2InstrumentWatchInstCount(trace);
        break;
      case IDIOM_3:
        Idiom3InstrumentWatchInstCount(trace);
        break;
      case IDIOM_4:
        Idiom4InstrumentWatchInstCount(trace);
        break;
      case IDIOM_5:
        Idiom5InstrumentWatchInstCount(trace);
        break;
      default:
        break;
  }
}

void SchedulerCommon::Idiom1InstrumentWatchMem(TRACE trace) {
  Idiom1SchedStatus *s = idiom1_sched_status_;

  if (ContainCandidates(trace)) {
    __InstrumentWatchMem(trace, true);
    return;
  }

  switch (s->state_) {
    case IDIOM1_STATE_E0_WATCH:
      __InstrumentWatchMem(trace, false);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom1InstrumentWatchInstCount(TRACE trace) {
  Idiom1SchedStatus *s = idiom1_sched_status_;

  switch (s->state_) {
    case IDIOM1_STATE_E0_WATCH:
      __InstrumentWatchInstCount(trace);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom2InstrumentWatchMem(TRACE trace) {
  Idiom2SchedStatus *s = idiom2_sched_status_;

  if (ContainCandidates(trace)) {
    __InstrumentWatchMem(trace, true);
    return;
  }

  switch (s->state_) {
    case IDIOM2_STATE_E0_WATCH:
    case IDIOM2_STATE_E0_E1_WATCH:
    case IDIOM2_STATE_E1_WATCH:
      __InstrumentWatchMem(trace, false);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom2InstrumentWatchInstCount(TRACE trace) {
  Idiom2SchedStatus *s = idiom2_sched_status_;

  switch (s->state_) {
    case IDIOM2_STATE_E0_WATCH:
    case IDIOM2_STATE_E0_E1_WATCH:
    case IDIOM2_STATE_E1_WATCH:
      __InstrumentWatchInstCount(trace);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom3InstrumentWatchMem(TRACE trace) {
  Idiom3SchedStatus *s = idiom3_sched_status_;

  if (ContainCandidates(trace)) {
    __InstrumentWatchMem(trace, true);
    return;
  }

  switch (s->state_) {
    case IDIOM3_STATE_E0_WATCH:
    case IDIOM3_STATE_E0_E1_WATCH:
    case IDIOM3_STATE_E1_WATCH:
    case IDIOM3_STATE_E0_WATCH_E3:
    case IDIOM3_STATE_E1_WATCH_E3:
    case IDIOM3_STATE_E1_WATCH_E2:
    case IDIOM3_STATE_E1_WATCH_E2_WATCH:
      __InstrumentWatchMem(trace, false);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom3InstrumentWatchInstCount(TRACE trace) {
  Idiom3SchedStatus *s = idiom3_sched_status_;

  switch (s->state_) {
    case IDIOM3_STATE_E0_WATCH:
    case IDIOM3_STATE_E0_E1_WATCH:
    case IDIOM3_STATE_E1_WATCH:
    case IDIOM3_STATE_E0_WATCH_E3:
    case IDIOM3_STATE_E1_WATCH_E3:
    case IDIOM3_STATE_E1_WATCH_E2:
    case IDIOM3_STATE_E1_WATCH_E2_WATCH:
      __InstrumentWatchInstCount(trace);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom4InstrumentWatchMem(TRACE trace) {
  Idiom4SchedStatus *s = idiom4_sched_status_;

  if (ContainCandidates(trace)) {
    __InstrumentWatchMem(trace, true);
    return;
  }

  switch (s->state_) {
    case IDIOM4_STATE_E0_WATCH:
    case IDIOM4_STATE_E0_E1_WATCH:
    case IDIOM4_STATE_E1_WATCH:
    case IDIOM4_STATE_E0_WATCH_E3:
    case IDIOM4_STATE_E1_WATCH_E3:
    case IDIOM4_STATE_E1_WATCH_E2:
    case IDIOM4_STATE_E1_WATCH_E2_WATCH:
      __InstrumentWatchMem(trace, false);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom4InstrumentWatchInstCount(TRACE trace) {
  Idiom4SchedStatus *s = idiom4_sched_status_;

  switch (s->state_) {
    case IDIOM4_STATE_E0_WATCH:
    case IDIOM4_STATE_E0_E1_WATCH:
    case IDIOM4_STATE_E1_WATCH:
    case IDIOM4_STATE_E0_WATCH_E3:
    case IDIOM4_STATE_E1_WATCH_E3:
    case IDIOM4_STATE_E1_WATCH_E2:
    case IDIOM4_STATE_E1_WATCH_E2_WATCH:
      __InstrumentWatchInstCount(trace);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom5InstrumentWatchMem(TRACE trace) {
  Idiom5SchedStatus *s = idiom5_sched_status_;

  if (ContainCandidates(trace)) {
    __InstrumentWatchMem(trace, true);
    return;
  }

  switch (s->state_) {
    case IDIOM5_STATE_E0_WATCH:
    case IDIOM5_STATE_E2_WATCH:
    case IDIOM5_STATE_E0_E2_WATCH:
    case IDIOM5_STATE_E0_WATCH_E3:
    case IDIOM5_STATE_E2_WATCH_E1:
    case IDIOM5_STATE_E0_E2_WATCH_E3:
    case IDIOM5_STATE_E0_E2_WATCH_E1:
    case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
    case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
      __InstrumentWatchMem(trace, false);
      break;
    default:
      break;
  }
}

void SchedulerCommon::Idiom5InstrumentWatchInstCount(TRACE trace) {
  Idiom5SchedStatus *s = idiom5_sched_status_;

  switch (s->state_) {
    case IDIOM5_STATE_E0_WATCH:
    case IDIOM5_STATE_E2_WATCH:
    case IDIOM5_STATE_E0_E2_WATCH:
    case IDIOM5_STATE_E0_WATCH_E3:
    case IDIOM5_STATE_E2_WATCH_E1:
    case IDIOM5_STATE_E0_E2_WATCH_E3:
    case IDIOM5_STATE_E0_E2_WATCH_E1:
    case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
    case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
      __InstrumentWatchInstCount(trace);
      break;
    default:
      break;
  }
}

void SchedulerCommon::__InstrumentWatchInstCount(TRACE trace) {
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    BBL_InsertCall(bbl, IPOINT_BEFORE,
                   (AFUNPTR)__WatchInstCount,
                   IARG_UINT32, BBL_NumIns(bbl),
                   IARG_END);
  }
}

void SchedulerCommon::__InstrumentWatchMem(TRACE trace, bool cand) {
  IMG img = GetImgByTrace(trace);
  // do not instrument memory accesses in pthread library
  if (IMG_Valid(img) && IMG_Name(img).find("libpthread") != std::string::npos)
    return;

  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      // ignore stack accesses
      if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
        continue;

      if (IsCandidate(trace, ins))
        continue;

      if (INS_IsMemoryRead(ins)) {
        Inst *inst = FindInst(INS_Address(ins));
        INS_InsertCall(ins, IPOINT_BEFORE,
                       (AFUNPTR)__WatchMemRead,
                       IARG_PTR, inst,
                       IARG_MEMORYREAD_EA,
                       IARG_MEMORYREAD_SIZE,
                       IARG_BOOL, cand,
                       IARG_END);
      }

      if (INS_IsMemoryWrite(ins)) {
        Inst *inst = FindInst(INS_Address(ins));
        INS_InsertCall(ins, IPOINT_BEFORE,
                       (AFUNPTR)__WatchMemWrite,
                       IARG_PTR, inst,
                       IARG_MEMORYWRITE_EA,
                       IARG_MEMORYWRITE_SIZE,
                       IARG_BOOL, cand,
                       IARG_END);
      }

      if (INS_HasMemoryRead2(ins)) {
        Inst *inst = FindInst(INS_Address(ins));
        INS_InsertCall(ins, IPOINT_BEFORE,
                       (AFUNPTR)__WatchMemRead,
                       IARG_PTR, inst,
                       IARG_MEMORYREAD2_EA,
                       IARG_MEMORYREAD_SIZE,
                       IARG_BOOL, cand,
                       IARG_END);
      }
    } // end of for ins
  } // end of for bbl
}

bool SchedulerCommon::ContainCandidates(TRACE trace) {
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    iRootEvent *e = curr_iroot_->GetEvent(i);
    if (e->IsMem()) {
      Inst *inst = e->inst();
      DEBUG_ASSERT(inst);
      Image *image = inst->image();
      DEBUG_ASSERT(image);
      IMG img = GetImgByTrace(trace);

      // no need to proceed if image does not match
      if (IMG_Valid(img)) {
        if (image->name().compare(IMG_Name(img)) != 0)
          continue;
      } else {
        if (image->name().compare(PSEUDO_IMAGE_NAME) != 0)
          continue;
      }

      ADDRINT img_low_addr = IMG_Valid(img) ? IMG_LowAddress(img) : 0;
      for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
          ADDRINT offset = INS_Address(ins) - img_low_addr;
          if (offset == inst->offset()) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

bool SchedulerCommon::IsCandidate(TRACE trace, INS ins) {
  int size = iRoot::GetNumEvents(curr_iroot_->idiom());
  for (int i = 0; i < size; i++) {
    iRootEvent *e = curr_iroot_->GetEvent(i);
    Inst *inst = e->inst();
    DEBUG_ASSERT(inst);
    Image *image = inst->image();
    DEBUG_ASSERT(image);
    IMG img = GetImgByTrace(trace);

    // no need to proceed if image does not match
    if (IMG_Valid(img)) {
      if (image->name().compare(IMG_Name(img)) != 0)
        continue;
    } else {
      if (image->name().compare(PSEUDO_IMAGE_NAME) != 0)
        continue;
    }

    ADDRINT img_low_addr = IMG_Valid(img) ? IMG_LowAddress(img) : 0;
    ADDRINT offset = INS_Address(ins) - img_low_addr;
    if (offset == inst->offset())
      return true;
  }
  return false;
}

void SchedulerCommon::FlushWatch() {
  DEBUG_ASSERT(curr_iroot_);
  if (curr_iroot_->HasMem()) {
    DEBUG_FMT_PRINT_SAFE("flush code cache\n");
    CODECACHE_FlushCache();
    //PIN_RemoveInstrumentation();
  }
}

void SchedulerCommon::ActivelyExposed() {
  DEBUG_ASSERT(curr_iroot_);
  if (!test_success_) {
    TestSuccess();
    history_->UpdateResult(true);
    test_success_ = true;
  }
}

Inst *SchedulerCommon::FindInst(ADDRINT pc) {
  Image *image = NULL;
  ADDRINT offset = 0;

  IMG img = IMG_FindByAddress(pc);
  if (!IMG_Valid(img)) {
    image = sinfo_->FindImage(PSEUDO_IMAGE_NAME);
    offset = pc;
  } else {
    image = sinfo_->FindImage(IMG_Name(img));
    offset = pc - IMG_LowAddress(img);
  }
  DEBUG_ASSERT(image);
  Inst *inst = image->Find(offset);
  return inst;
}

void SchedulerCommon::CalculatePriorities() {
  if (knob_->ValueBool("strict")) {
    int lowest = knob_->ValueInt("lowest_realtime_priority");
    int highest = knob_->ValueInt("highest_realtime_priority");
    min_priority_ = lowest;
    max_priority_ = highest;
    normal_priority_ = (lowest + highest) / 2;
    lower_priority_ = lowest + 1;
    higher_priority_ = highest - 1;
    // fill new thread priorities, increasing priority
    for (int i = 0; i < higher_priority_ - lower_priority_ - 1; i++) {
      new_thread_priorities_.push_back(lower_priority_ + 1 + i);
    }
  } else {
    int lowest = knob_->ValueInt("lowest_nice_value"); // highest priority
    int highest = knob_->ValueInt("highest_nice_value"); // lowest priority
    min_priority_ = highest;
    max_priority_ = lowest;
    normal_priority_ = 0;
    lower_priority_ = highest - 1;
    higher_priority_ = lowest + 1;
    // fill new thread priorities, increasing priority
    for (int i = 0; i < lower_priority_ - higher_priority_ - 1; i++) {
      new_thread_priorities_.push_back(lower_priority_ - 1 - i);
    }
  }
}

int SchedulerCommon::MainThreadPriority() {
  // assign a random priority to the main thread
  // int priority = NormalPriority(); // TODO: choose which?
  // int priority = HigherPriority();
  int priority = NextNewThreadPriority();
  return priority;
}

int SchedulerCommon::NextNewThreadPriority() {
  int priority;
  if (knob_->ValueBool("ordered_new_thread_prio")) {
    if (UseDecreasingPriorities()) {
      // decreasing priorities
      int cursor = ATOMIC_FETCH_AND_SUB(&new_thread_priorities_cursor_, 1);
      priority = new_thread_priorities_[cursor % new_thread_priorities_.size()];
    } else {
      // increasing priorities
      int cursor = ATOMIC_FETCH_AND_ADD(&new_thread_priorities_cursor_, 1);
      priority = new_thread_priorities_[cursor % new_thread_priorities_.size()];
    }
  } else {
    // assign a random priority to the thread
    priority = NormalPriority();
  }
  return priority;
}

void SchedulerCommon::InitNewThreadPriority() {
  // set new thread priorities cursor
  if (knob_->ValueBool("ordered_new_thread_prio")) {
    if (UseDecreasingPriorities()) {
      // decreasing priorities
      DEBUG_FMT_PRINT_SAFE("decreasing priorities\n");
      new_thread_priorities_cursor_ = new_thread_priorities_.size() - 1;
    } else {
      // increasing priorities
      DEBUG_FMT_PRINT_SAFE("increasing priorities\n");
      new_thread_priorities_cursor_ = 0;
    }
  }
}

bool SchedulerCommon::RandomChoice(double true_rate) {
#ifdef _USING_SYS_RAND_GEN
  double val = rand() / (RAND_MAX + 1.0);
  if (val < true_rate)
    return true;
  else
    return false;
#else
  double val = random_number();
  if (val < true_rate)
    return true;
  else
    return false;
#endif
}

int SchedulerCommon::GetPriority(thread_id_t target) {
  LockMisc();
  int priority = priority_map_[target];
  UnlockMisc();
  return priority;
}

void SchedulerCommon::SetPriority(int priority) {
  // set the priority of the current thread
  DEBUG_FMT_PRINT_SAFE("[T%lx] Set self priority=%d\n",
                       PIN_ThreadUid(), priority);

  if (knob_->ValueBool("strict")) {
    SetStrictPriority(priority);
  } else {
    SetRelaxPriority(priority);
  }
}

void SchedulerCommon::SetPriority(thread_id_t target, int priority) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] Set priority(T%lx)=%d\n",
                       PIN_ThreadUid(), target, priority);
  LockMisc();
  priority_map_[target] = priority;
  UnlockMisc();

  if (knob_->ValueBool("strict")) {
    SetStrictPriority(target, priority);
  } else {
    SetRelaxPriority(target, priority);
  }
}

void SchedulerCommon::SetPriorityNormal(thread_id_t target) {
  LockMisc();
  int priority = ori_priority_map_[target];
  UnlockMisc();
  SetPriority(target, priority);
}

void SchedulerCommon::SetPriorityLow(thread_id_t target) {
  SetPriority(target, LowerPriority());
}

void SchedulerCommon::SetPriorityHigh(thread_id_t target) {
  SetPriority(target, HigherPriority());
}

void SchedulerCommon::SetPriorityMin(thread_id_t target) {
  SetPriority(target, MinPriority());
}

void SchedulerCommon::SetPriorityMax(thread_id_t target) {
  SetPriority(target, MaxPriority());
}

void SchedulerCommon::SetStrictPriority(int priority) {
  struct sched_param param;
  param.sched_priority = priority;
  if (sched_setscheduler(0, SCHED_FIFO, &param)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetStrictPriority failed\n");
  }
}

void SchedulerCommon::SetRelaxPriority(int priority) {
  if (setpriority(PRIO_PROCESS, 0, priority)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetRelaxPriority failed\n");
  }
}

void SchedulerCommon::SetStrictPriority(thread_id_t target, int priority) {
  // get os pid
  LockMisc();
  OS_THREAD_ID os_target = INVALID_OS_THREAD_ID;
  if (thd_id_os_tid_map_.find(target) != thd_id_os_tid_map_.end())
    os_target = thd_id_os_tid_map_[target];
  UnlockMisc();

  if (os_target == INVALID_OS_THREAD_ID)
    return;

  struct sched_param param;
  param.sched_priority = priority;
  if (sched_setscheduler(os_target, SCHED_FIFO, &param)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetStrictPriority failed\n");
  }
}

void SchedulerCommon::SetRelaxPriority(thread_id_t target, int priority) {
  // get os pid
  LockMisc();
  OS_THREAD_ID os_target = INVALID_OS_THREAD_ID;
  if (thd_id_os_tid_map_.find(target) != thd_id_os_tid_map_.end())
    os_target = thd_id_os_tid_map_[target];
  UnlockMisc();

  if (os_target == INVALID_OS_THREAD_ID)
    return;

  if (setpriority(PRIO_PROCESS, os_target, priority)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetRelaxPriority failed\n");
  }
}

void SchedulerCommon::SetAffinity() {
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

void SchedulerCommon::HandleBeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1BeforeiRootMemRead(idx, addr, size);
      break;
    case IDIOM_2:
      Idiom2BeforeiRootMemRead(idx, addr, size);
      break;
    case IDIOM_3:
      Idiom3BeforeiRootMemRead(idx, addr, size);
      break;
    case IDIOM_4:
      Idiom4BeforeiRootMemRead(idx, addr, size);
      break;
    case IDIOM_5:
      Idiom5BeforeiRootMemRead(idx, addr, size);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleBeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1BeforeiRootMemWrite(idx, addr, size);
      break;
    case IDIOM_2:
      Idiom2BeforeiRootMemWrite(idx, addr, size);
      break;
    case IDIOM_3:
      Idiom3BeforeiRootMemWrite(idx, addr, size);
      break;
    case IDIOM_4:
      Idiom4BeforeiRootMemWrite(idx, addr, size);
      break;
    case IDIOM_5:
      Idiom5BeforeiRootMemWrite(idx, addr, size);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleAfteriRootMem(UINT32 idx) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1AfteriRootMem(idx);
      break;
    case IDIOM_2:
      Idiom2AfteriRootMem(idx);
      break;
    case IDIOM_3:
      Idiom3AfteriRootMem(idx);
      break;
    case IDIOM_4:
      Idiom4AfteriRootMem(idx);
      break;
    case IDIOM_5:
      Idiom5AfteriRootMem(idx);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleBeforeiRootMutexLock(UINT32 idx, address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1BeforeiRootMutexLock(idx, addr);
      break;
    case IDIOM_2:
      Idiom2BeforeiRootMutexLock(idx, addr);
      break;
    case IDIOM_3:
      Idiom3BeforeiRootMutexLock(idx, addr);
      break;
    case IDIOM_4:
      Idiom4BeforeiRootMutexLock(idx, addr);
      break;
    case IDIOM_5:
      Idiom5BeforeiRootMutexLock(idx, addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleAfteriRootMutexLock(UINT32 idx, address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1AfteriRootMutexLock(idx, addr);
      break;
    case IDIOM_2:
      Idiom2AfteriRootMutexLock(idx, addr);
      break;
    case IDIOM_3:
      Idiom3AfteriRootMutexLock(idx, addr);
      break;
    case IDIOM_4:
      Idiom4AfteriRootMutexLock(idx, addr);
      break;
    case IDIOM_5:
      Idiom5AfteriRootMutexLock(idx, addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleBeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1BeforeiRootMutexUnlock(idx, addr);
      break;
    case IDIOM_2:
      Idiom2BeforeiRootMutexUnlock(idx, addr);
      break;
    case IDIOM_3:
      Idiom3BeforeiRootMutexUnlock(idx, addr);
      break;
    case IDIOM_4:
      Idiom4BeforeiRootMutexUnlock(idx, addr);
      break;
    case IDIOM_5:
      Idiom5BeforeiRootMutexUnlock(idx, addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleAfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1AfteriRootMutexUnlock(idx, addr);
      break;
    case IDIOM_2:
      Idiom2AfteriRootMutexUnlock(idx, addr);
      break;
    case IDIOM_3:
      Idiom3AfteriRootMutexUnlock(idx, addr);
      break;
    case IDIOM_4:
      Idiom4AfteriRootMutexUnlock(idx, addr);
      break;
    case IDIOM_5:
      Idiom5AfteriRootMutexUnlock(idx, addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleWatchMutexLock(address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1WatchMutexLock(addr);
      break;
    case IDIOM_2:
      Idiom2WatchMutexLock(addr);
      break;
    case IDIOM_3:
      Idiom3WatchMutexLock(addr);
      break;
    case IDIOM_4:
      Idiom4WatchMutexLock(addr);
      break;
    case IDIOM_5:
      Idiom5WatchMutexLock(addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleWatchMutexUnlock(address_t addr) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1WatchMutexUnlock(addr);
      break;
    case IDIOM_2:
      Idiom2WatchMutexUnlock(addr);
      break;
    case IDIOM_3:
      Idiom3WatchMutexUnlock(addr);
      break;
    case IDIOM_4:
      Idiom4WatchMutexUnlock(addr);
      break;
    case IDIOM_5:
      Idiom5WatchMutexUnlock(addr);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleWatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1WatchMemRead(inst, addr, size, cand);
      break;
    case IDIOM_2:
      Idiom2WatchMemRead(inst, addr, size, cand);
      break;
    case IDIOM_3:
      Idiom3WatchMemRead(inst, addr, size, cand);
      break;
    case IDIOM_4:
      Idiom4WatchMemRead(inst, addr, size, cand);
      break;
    case IDIOM_5:
      Idiom5WatchMemRead(inst, addr, size, cand);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleWatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1WatchMemWrite(inst, addr, size, cand);
      break;
    case IDIOM_2:
      Idiom2WatchMemWrite(inst, addr, size, cand);
      break;
    case IDIOM_3:
      Idiom3WatchMemWrite(inst, addr, size, cand);
      break;
    case IDIOM_4:
      Idiom4WatchMemWrite(inst, addr, size, cand);
      break;
    case IDIOM_5:
      Idiom5WatchMemWrite(inst, addr, size, cand);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleWatchInstCount(timestamp_t c) {
  if (!start_schedule_)
    return;

  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Abort("invalid watch inst count\n");
      break;
    case IDIOM_2:
      Idiom2WatchInstCount(c);
      break;
    case IDIOM_3:
      Idiom3WatchInstCount(c);
      break;
    case IDIOM_4:
      Idiom4WatchInstCount(c);
      break;
    case IDIOM_5:
      Idiom5WatchInstCount(c);
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::HandleSchedYield() {
  switch (curr_iroot_->idiom()) {
    case IDIOM_1:
      Idiom1SchedYield();
      break;
    case IDIOM_2:
      // TODO:
      break;
    case IDIOM_3:
      // TODO:
      break;
    case IDIOM_4:
      // TODO:
      break;
    case IDIOM_5:
      // TODO:
      break;
    default:
      Abort("invalid idiom\n");
      break;
  }
}

void SchedulerCommon::Idiom1BeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_WRITE)
    return;

  switch (idx) {
    case 0:
      Idiom1BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom1BeforeEvent1(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1BeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_READ)
    return;

  switch (idx) {
    case 0:
      Idiom1BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom1BeforeEvent1(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1AfteriRootMem(UINT32 idx) {
  switch (idx) {
    case 0:
      Idiom1AfterEvent0();
      break;
    case 1:
      Idiom1AfterEvent1();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1BeforeiRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom1BeforeEvent1(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1AfteriRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom1AfterEvent1();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1BeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom1BeforeEvent0(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1AfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom1AfterEvent0();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom1BeforeEvent0(address_t addr, size_t size) {
  Idiom1SchedStatus *s = idiom1_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event0", 1);

  // pankit fix : increment the count for curr_thd
  if (s->num_mem_acc.find(curr_thd_id) == s->num_mem_acc.end()) {

      s->num_mem_acc[curr_thd_id] = 1;

  } else {

      s->num_mem_acc[curr_thd_id] = s->num_mem_acc.find(curr_thd_id)->second + 1;
  }

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 0\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM1_STATE_INIT:
        s->thd_id_[0] = curr_thd_id;
        s->addr_[0] = addr;
        s->size_[0] = size;
        Idiom1SetState(IDIOM1_STATE_E0);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM1_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          Idiom1SetState(IDIOM1_STATE_E0_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
          // force to execute event 0 first
          // after that, set thd[0] to low priority
        } else {
          // random choice (has to use random choice to avoid livelock)
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM1_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          thread_id_t target = s->thd_id_[1];
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom1SetState(IDIOM1_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom1SetState(IDIOM1_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM1_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed any more
            if (Idiom1CheckGiveup(0)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E0);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E0);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom1CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom1ClearDelaySet(&copy);
                Idiom1SetState(IDIOM1_STATE_E0);
                UnlockSchedStatus();
                Idiom1WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E0);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom1BeforeEvent1(address_t addr, size_t size) {
  Idiom1SchedStatus *s = idiom1_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event1", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 1\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM1_STATE_INIT:
        s->thd_id_[1] = curr_thd_id;
        s->addr_[1] = addr;
        s->size_[1] = size;
        Idiom1SetState(IDIOM1_STATE_E1);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM1_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          thread_id_t target = s->thd_id_[0];
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom1SetState(IDIOM1_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityHigh(target);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom1SetState(IDIOM1_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM1_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom1CheckGiveup(1)) {
            // has to go back to initial state
            Idiom1SetState(IDIOM1_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM1_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed any more
            if (Idiom1CheckGiveup(1)) {
              DelaySet copy;
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E1);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E1);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom1ClearDelaySet(&copy);
            Idiom1SetState(IDIOM1_STATE_E0_E1);
            UnlockSchedStatus();
            Idiom1WakeDelaySet(&copy);
            SetPriorityNormal(target);
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_E1);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom1AfterEvent0() {
  Idiom1SchedStatus *s = idiom1_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] post event 0\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM1_STATE_E0_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        Idiom1SetState(IDIOM1_STATE_E0_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM1_STATE_E0_E1:
      if (curr_thd_id == s->thd_id_[0]) {
        thread_id_t target = s->thd_id_[1];
        UnlockSchedStatus();
        // force to execute event 1
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom1AfterEvent1() {
  Idiom1SchedStatus *s = idiom1_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] post event 1\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM1_STATE_E0_E1:
      if (curr_thd_id == s->thd_id_[1]) {
        ActivelyExposed();
        DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                             curr_thd_id, curr_iroot_->id());
        Idiom1SetState(IDIOM1_STATE_DONE);
        UnlockSchedStatus();
        //SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom1WatchAccess(address_t addr, size_t size) {
  Idiom1SchedStatus *s = idiom1_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("watch_access", 1);

  while (true) {
    // control variables
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM1_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed any more

            // pankit fix : increment the count for the curr_thd
            if (s->num_mem_acc.find(curr_thd_id) == s->num_mem_acc.end()) {

                s->num_mem_acc[curr_thd_id] = 1;

            } else {

                s->num_mem_acc[curr_thd_id] = s->num_mem_acc.find(curr_thd_id)->second + 1;
            }

            if (Idiom1CheckGiveup(2)) {
              DEBUG_FMT_PRINT_SAFE("[T%lx] watch access give up\n", curr_thd_id);
              DelaySet copy;
              Idiom1ClearDelaySet(&copy);
              Idiom1SetState(IDIOM1_STATE_INIT);
              UnlockSchedStatus();
              Idiom1WakeDelaySet(&copy);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // pankit fix : increment the count for the curr_thd
            if (s->num_mem_acc.find(curr_thd_id) == s->num_mem_acc.end()) {

                s->num_mem_acc[curr_thd_id] = 1;

            } else {

                s->num_mem_acc[curr_thd_id] = s->num_mem_acc.find(curr_thd_id)->second + 1;
            }

            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom1CheckGiveup(2)) {
                DEBUG_FMT_PRINT_SAFE("[T%lx] watch access give up\n", curr_thd_id);
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom1ClearDelaySet(&copy);
                Idiom1SetState(IDIOM1_STATE_INIT);
                UnlockSchedStatus();
                Idiom1WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
             
              // pankit fix : if the count is satisfied then only give up, else increment go ahead with high priority
            } else  {
                
                if (curr_iroot_->getCountPairBool() &&  s->num_mem_acc.find(curr_thd_id) != s->num_mem_acc.end() && 
                            s->num_mem_acc.find(curr_thd_id)->second != curr_iroot_->getDstCount()) {
                    UnlockSchedStatus();
                    SetPriorityHigh(curr_thd_id);
                } else {
                    s->delay_set_.insert(curr_thd_id);
                    UnlockSchedStatus();
                    SetPriorityLow(curr_thd_id);
                    restart = true;
                }
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom1WatchMutexLock(address_t addr) {
  Idiom1WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom1WatchMutexUnlock(address_t addr) {
  Idiom1WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom1WatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  Idiom1WatchAccess(addr, size);
  if (!cand)
    Idiom1CheckFlush();
}

void SchedulerCommon::Idiom1WatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  Idiom1WatchAccess(addr, size);
  if (!cand)
    Idiom1CheckFlush();
}

void SchedulerCommon::Idiom1SchedYield() {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  DEBUG_FMT_PRINT_SAFE("[T%lx] sched yield\n", curr_thd_id);
  SetPriorityMin(curr_thd_id);
}

void SchedulerCommon::Idiom1CheckFlush() {
  Idiom1SchedStatus *s = idiom1_sched_status_;

  // define a static local variable
  static int token = 0;

  switch (s->state_) {
    case IDIOM1_STATE_INIT:
    case IDIOM1_STATE_E0:
    case IDIOM1_STATE_E1:
    case IDIOM1_STATE_DONE:
      if (token-- <= 0) {
        FlushWatch();
        token = 10;
      }
      break;
    default:
      break;
  }
}

bool SchedulerCommon::Idiom1CheckGiveup(int idx) {
  // return true means actual give up
  // return false means one more chance
  if (YieldWithDelay()) {
    Idiom1SchedStatus *s = idiom1_sched_status_;
    thread_id_t curr_thd_id = PIN_ThreadUid();
    DEBUG_FMT_PRINT_SAFE("[T%lx] Check giveup\n", curr_thd_id);
    static unsigned long last_state[] = {IDIOM1_STATE_INVALID,
                                         IDIOM1_STATE_INVALID,
                                         IDIOM1_STATE_INVALID};
    static thread_id_t last_thd[] = {INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID};
    static int time_delayed_each[] = {0, 0, 0};
    static int time_delayed_total = 0;
    if (time_delayed_each[idx] <= knob_->ValueInt("yield_delay_min_each") ||
        time_delayed_total <= knob_->ValueInt("yield_delay_max_total")) {
      if (s->state_ != last_state[idx] || curr_thd_id != last_thd[idx]) {
        DEBUG_FMT_PRINT_SAFE("[T%lx] time delay\n", curr_thd_id);
        int time_unit = knob_->ValueInt("yield_delay_unit");
        last_state[idx] = s->state_;
        last_thd[idx] = curr_thd_id;
        time_delayed_each[idx] += time_unit;
        time_delayed_total += time_unit;
        UnlockSchedStatus();
        DEBUG_STAT_INC("delay", 1);
        usleep(1000 * time_unit);
        LockSchedStatus();
        return false;
      } else {
        return true;
      }
    } else {
      return true;
    }
  } else {
    return true;
  }
}

void SchedulerCommon::Idiom1SetState(unsigned long s) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set state: %s\n", PIN_ThreadUid(),
                       Idiom1SchedStatus::StateToString(s).c_str());
  idiom1_sched_status_->state_ = s;
}

void SchedulerCommon::Idiom1ClearDelaySet(DelaySet *copy) {
  *copy = idiom1_sched_status_->delay_set_;
  idiom1_sched_status_->delay_set_.clear();
}

void SchedulerCommon::Idiom1WakeDelaySet(DelaySet *copy) {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  SetPriorityHigh(curr_thd_id);
  for (DelaySet::iterator it = copy->begin(); it != copy->end(); ++it) {
    if (*it != curr_thd_id)
      SetPriorityNormal(*it);
  }
}

void SchedulerCommon::Idiom2BeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_WRITE)
    return;

  switch (idx) {
    case 0:
      Idiom2BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom2BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom2BeforeEvent2(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom2BeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_READ)
    return;

  switch (idx) {
    case 0:
      Idiom2BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom2BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom2BeforeEvent2(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom2AfteriRootMem(UINT32 idx) {
  switch (idx) {
    case 0:
      Idiom2AfterEvent0();
      break;
    case 1:
      Idiom2AfterEvent1();
      break;
    case 2:
      Idiom2AfterEvent2();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom2BeforeiRootMutexLock(UINT32 idx, address_t addr) {
  Abort("invalid event\n");
}

void SchedulerCommon::Idiom2AfteriRootMutexLock(UINT32 idx, address_t addr) {
  Abort("invalid event\n");
}

void SchedulerCommon::Idiom2BeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  Abort("invalid event\n");
}

void SchedulerCommon::Idiom2AfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  Abort("invalid event\n");
}

void SchedulerCommon::Idiom2BeforeEvent0(address_t addr, size_t size) {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event0", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 0\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM2_STATE_INIT:
        s->thd_id_[0] = curr_thd_id;
        s->addr_[0] = addr;
        s->size_[0] = size;
        Idiom2SetState(IDIOM2_STATE_E0);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM2_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          // has to execute event 0, go to watch mode
          Idiom2SetState(IDIOM2_STATE_E0_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom2SetState(IDIOM2_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first, thd_[1] remains low priority
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom2SetState(IDIOM2_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay because event 2 will not be hit
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_E0);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM2_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 2 will not be hit
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0_E1);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            // force to execute event 0
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              // TODO: here, one problem is that there is no way to tell
              // thd_[1] to ignore the event 1 (need a flag?)
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // random choice
            //   1) delay this thread (set to min prio)
            //   2) treat this event as event 0
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            if (RandomChoice(0.5)) {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityMin(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              // force to execute event 0 (curr_thd)
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM2_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 2 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // cannot be delayed anymore
            if (Idiom2CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_E0);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target1);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E0);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom2BeforeEvent1(address_t addr, size_t size) {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event1", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 1\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM2_STATE_INIT:
        s->thd_id_[1] = curr_thd_id;
        s->addr_[1] = addr;
        s->size_[1] = size;
        Idiom2SetState(IDIOM2_STATE_E1);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM2_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          thread_id_t target = s->thd_id_[0];
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom2SetState(IDIOM2_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityMax(curr_thd_id);
          SetPriorityHigh(target);
          SetPriorityLow(curr_thd_id);
          restart = true;
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom2SetState(IDIOM2_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom2CheckGiveup(1)) {
            // has to go back to initial state
            Idiom2SetState(IDIOM2_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay because event 2 will not be hit
            DelaySet copy;
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E1);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
            // the dependency e0->e1 is satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0_E1_WATCH);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            // prioritize the thread that executed event 0, delay curr thd
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM2_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 2 will not be hit
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E1);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DelaySet copy;
          Idiom2ClearDelaySet(&copy);
          Idiom2SetState(IDIOM2_STATE_E1_WATCH_X);
          UnlockSchedStatus();
          Idiom2WakeDelaySet(&copy);
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // random choice
            //   1) delay this thread (set to min prob)
            //   2) treat this event as event 1 (set target to min prob)
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            if (RandomChoice(0.5)) {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityMin(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityMin(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM2_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 2 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E1);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // cannot be delayed any more
            if (Idiom2CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                s->thd_id_[1] = curr_thd_id;
                s->addr_[1] = addr;
                s->size_[1] = size;
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_E1);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target1);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_E1);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom2BeforeEvent2(address_t addr, size_t size) {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event2", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 2\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM2_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed any more
            if (Idiom2CheckGiveup(2)) {
              DelaySet copy;
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_INIT);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_INIT);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // idiom-2 iroot is satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0_E1_E2);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // idiom-2 iroot is satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E0_E1_E2);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(target);
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // cannot be delayed any more
            if (Idiom2CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_INIT);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_INIT);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom2AfterEvent0() {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 0\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM2_STATE_E0_E1:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = 0;
        Idiom2SetState(IDIOM2_STATE_E0_E1_WATCH);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
        FlushWatch();
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM2_STATE_E0_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = 0;
        Idiom2SetState(IDIOM2_STATE_E0_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom2AfterEvent1() {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 1\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM2_STATE_E1_WATCH_X:
      if (curr_thd_id == s->thd_id_[1]) {
        Idiom2SetState(IDIOM2_STATE_E1_WATCH);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM2_STATE_E0_E1_E2:
      if (curr_thd_id == s->thd_id_[1]) {
        thread_id_t target = s->thd_id_[2];
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom2AfterEvent2() {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 2\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM2_STATE_E0_E1_E2:
      if (curr_thd_id == s->thd_id_[2]) {
        ActivelyExposed();
        DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                             curr_thd_id, curr_iroot_->id());
        Idiom2SetState(IDIOM2_STATE_DONE);
        UnlockSchedStatus();
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom2WatchAccess(address_t addr, size_t size) {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("watch_access", 1);

  while (true) {
    // control variables
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM2_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_INIT);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom2CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_INIT);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay
            DelaySet copy;
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_E1);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM2_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom2ClearDelaySet(&copy);
            Idiom2SetState(IDIOM2_STATE_INIT);
            UnlockSchedStatus();
            Idiom2WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // cannot be delayed any more
            if (Idiom2CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom2ClearDelaySet(&copy);
              Idiom2SetState(IDIOM2_STATE_INIT);
              UnlockSchedStatus();
              Idiom2WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot by delayed any more
              if (Idiom2CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom2ClearDelaySet(&copy);
                Idiom2SetState(IDIOM2_STATE_INIT);
                UnlockSchedStatus();
                Idiom2WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom2WatchInstCount(timestamp_t c) {
  Idiom2SchedStatus *s = idiom2_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM2_STATE_E0_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ += c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom2ClearDelaySet(&copy);
          Idiom2SetState(IDIOM2_STATE_INIT);
          UnlockSchedStatus();
          Idiom2WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM2_STATE_E0_E1_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = s->window_ + c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom2ClearDelaySet(&copy);
          Idiom2SetState(IDIOM2_STATE_E1);
          UnlockSchedStatus();
          Idiom2WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM2_STATE_E1_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = s->window_ + c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[1];
          Idiom2ClearDelaySet(&copy);
          Idiom2SetState(IDIOM2_STATE_INIT);
          UnlockSchedStatus();
          Idiom2WakeDelaySet(&copy);
          SetPriorityNormal(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom2WatchMutexLock(address_t addr) {
  Idiom2WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom2WatchMutexUnlock(address_t addr) {
  Idiom2WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom2WatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  Idiom2WatchAccess(addr, size);
  if (!cand)
    Idiom2CheckFlush();
}

void SchedulerCommon::Idiom2WatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  Idiom2WatchAccess(addr, size);
  if (!cand)
    Idiom2CheckFlush();
}


void SchedulerCommon::Idiom2CheckFlush() {
  Idiom2SchedStatus *s = idiom2_sched_status_;

  // define a static local variable
  static int token = 0;

  switch (s->state_) {
    case IDIOM2_STATE_INIT:
    case IDIOM2_STATE_E0:
    case IDIOM2_STATE_E1:
    case IDIOM2_STATE_DONE:
      if (token-- <= 0) {
        FlushWatch();
        token = 10;
      }
      break;
    default:
      break;
  }
}

bool SchedulerCommon::Idiom2CheckGiveup(int idx) {
  // return true means actual give up
  // return false means one more chance
  if (YieldWithDelay()) {
    Idiom2SchedStatus *s = idiom2_sched_status_;
    thread_id_t curr_thd_id = PIN_ThreadUid();
    DEBUG_FMT_PRINT_SAFE("[T%lx] Check giveup\n", curr_thd_id);
    static unsigned long last_state[] = {IDIOM2_STATE_INVALID,
                                         IDIOM2_STATE_INVALID,
                                         IDIOM2_STATE_INVALID,
                                         IDIOM2_STATE_INVALID};
    static thread_id_t last_thd[] = {INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID};
    static int time_delayed_each[] = {0, 0, 0, 0};
    static int time_delayed_total = 0;
    if (time_delayed_each[idx] <= knob_->ValueInt("yield_delay_min_each") ||
        time_delayed_total <= knob_->ValueInt("yield_delay_max_total")) {
      if (s->state_ != last_state[idx] || curr_thd_id != last_thd[idx]) {
        DEBUG_FMT_PRINT_SAFE("[T%lx] time delay\n", curr_thd_id);
        int time_unit = knob_->ValueInt("yield_delay_unit");
        last_state[idx] = s->state_;
        last_thd[idx] = curr_thd_id;
        time_delayed_each[idx] += time_unit;
        time_delayed_total += time_unit;
        UnlockSchedStatus();
        usleep(1000 * time_unit);
        LockSchedStatus();
        return false;
      } else {
        return true;
      }
    } else {
      return true;
    }
  } else {
    return true;
  }
}

void SchedulerCommon::Idiom2SetState(unsigned long s) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set state: %s\n", PIN_ThreadUid(),
                       Idiom2SchedStatus::StateToString(s).c_str());
  idiom2_sched_status_->state_ = s;
}

void SchedulerCommon::Idiom2ClearDelaySet(DelaySet *copy) {
  *copy = idiom2_sched_status_->delay_set_;
  idiom2_sched_status_->delay_set_.clear();
}

void SchedulerCommon::Idiom2WakeDelaySet(DelaySet *copy) {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  SetPriorityHigh(curr_thd_id);
  for (DelaySet::iterator it = copy->begin(); it != copy->end(); ++it) {
    if (*it != curr_thd_id)
      SetPriorityNormal(*it);
  }
}

void SchedulerCommon::Idiom3BeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_WRITE)
    return;

  switch (idx) {
    case 0:
      Idiom3BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom3BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom3BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom3BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3BeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_READ)
    return;

  switch (idx) {
    case 0:
      Idiom3BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom3BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom3BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom3BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3AfteriRootMem(UINT32 idx) {
  switch (idx) {
    case 0:
      Idiom3AfterEvent0();
      break;
    case 1:
      Idiom3AfterEvent1();
      break;
    case 2:
      Idiom3AfterEvent2();
      break;
    case 3:
      Idiom3AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3BeforeiRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom3BeforeEvent1(addr, WORD_SIZE);
      break;
    case 3:
      Idiom3BeforeEvent3(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3AfteriRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom3AfterEvent1();
      break;
    case 3:
      Idiom3AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3BeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom3BeforeEvent0(addr, WORD_SIZE);
      break;
    case 2:
      Idiom3BeforeEvent2(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3AfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom3AfterEvent0();
      break;
    case 2:
      Idiom3AfterEvent2();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom3BeforeEvent0(address_t addr, size_t size) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event0", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 0\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM3_STATE_INIT:
        s->thd_id_[0] = curr_thd_id;
        s->addr_[0] = addr;
        s->size_[0] = size;
        Idiom3SetState(IDIOM3_STATE_E0);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM3_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          // has to execute event 0, go to watch mode
          Idiom3SetState(IDIOM3_STATE_E0_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          thread_id_t target = s->thd_id_[1];
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom3SetState(IDIOM3_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay since event 3 will not be hit
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E0);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM3_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX should not reach here
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be reached
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
            OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              SetPriorityHigh(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E0);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E0);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom3BeforeEvent1(address_t addr, size_t size) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event1", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 1\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM3_STATE_INIT:
        s->thd_id_[1] = curr_thd_id;
        s->addr_[1] = addr;
        s->size_[1] = size;
        Idiom3SetState(IDIOM3_STATE_E1);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM3_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          thread_id_t target = s->thd_id_[0];
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom3SetState(IDIOM3_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityMax(curr_thd_id);
          SetPriorityHigh(target);
          SetPriorityNormal(curr_thd_id);
          restart = true;
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom3CheckGiveup(1)) {
            // has to go back to initial state
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay because event 3 will not be hit
            DelaySet copy;
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
            // the dependency e0->e1 is satisfied
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E0_E1_WATCH);
            UnlockSchedStatus();
            // force to execute thd[1] first
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM3_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id == s->thd_id_[1]);
        DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
        // XXX confirm here since e1 might be blocked
        // since it might be acquiring a lock
        Idiom3SetState(IDIOM3_STATE_E1_WATCH_X);
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay sine event 3 will not be reached
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
            OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
          // should execute this e1 and goto E1_WATCH_E3
          DelaySet copy;
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom3ClearDelaySet(&copy);
          Idiom3SetState(IDIOM3_STATE_E1_WATCH_E3_X);
          UnlockSchedStatus();
          Idiom3WakeDelaySet(&copy);
          SetPriorityHigh(curr_thd_id);
          // force to execute this event 1 first
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              SetPriorityHigh(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E1);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom3BeforeEvent2(address_t addr, size_t size) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event2", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 2\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM3_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay since event 3 will not be hit
            DelaySet copy;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_INIT);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX should not reach here
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // goto state E1_WATCH_E2
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom3SetState(IDIOM3_STATE_E1_WATCH_E2);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
            OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_INIT);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
            // iroot conditions are satisfied
            thread_id_t target = s->thd_id_[3];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom3SetState(IDIOM3_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            // force to execute this event 2
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          thread_id_t target = s->thd_id_[0];
          Idiom3SetState(IDIOM3_STATE_E1_WATCH_E2_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom3BeforeEvent3(address_t addr, size_t size) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event3", 1);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 3\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM3_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // goto state E0_WATCH_E3
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom3SetState(IDIOM3_STATE_E0_WATCH_E3);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_INIT);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX inaccuracy here
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // goto state E1_WATCH_E3
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom3SetState(IDIOM3_STATE_E1_WATCH_E3);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom3CheckGiveup(3)) {
            DelaySet copy;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_INIT);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          thread_id_t target = s->thd_id_[1];
          Idiom3SetState(IDIOM3_STATE_INIT);
          UnlockSchedStatus();
          SetPriorityMax(curr_thd_id);
          SetPriorityNormal(target);
          SetPriorityNormal(curr_thd_id);
          restart = true;
        } else if (curr_thd_id == s->thd_id_[1]) {
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // iroot conditions are satisfied
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom3SetState(IDIOM3_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // iroot conditions are satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityHigh(curr_thd_id);
            // force to execute current event 3
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom3AfterEvent0() {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 0\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM3_STATE_E0_E1:
      if (curr_thd_id == s->thd_id_[0]) {
        thread_id_t target = s->thd_id_[1];
        s->window_ = 0;
        Idiom3SetState(IDIOM3_STATE_E0_E1_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E0_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = 0;
        Idiom3SetState(IDIOM3_STATE_E0_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom3AfterEvent1() {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 1\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM3_STATE_E1_WATCH_X:
      if (curr_thd_id == s->thd_id_[1]) {
        thread_id_t target = s->thd_id_[0];
        Idiom3SetState(IDIOM3_STATE_E1_WATCH);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
        SetPriorityNormal(target);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E1_WATCH_E3_X:
      if (curr_thd_id == s->thd_id_[1]) {
        Idiom3SetState(IDIOM3_STATE_E1_WATCH_E3);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom3AfterEvent2() {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 2\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM3_STATE_E1_WATCH_E2_WATCH_X:
      if (curr_thd_id == s->thd_id_[2]) {
        thread_id_t target = s->thd_id_[0];
        Idiom3SetState(IDIOM3_STATE_E1_WATCH_E2_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[2]) {
        thread_id_t target = s->thd_id_[3];
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom3AfterEvent3() {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 3\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM3_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[3]) {
        ActivelyExposed();
        DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                             curr_thd_id, curr_iroot_->id());
        Idiom3SetState(IDIOM3_STATE_DONE);
        UnlockSchedStatus();
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom3WatchAccess(address_t addr, size_t size) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("watch_access", 1);

  while (true) {
    // control variables
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM3_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need delay since event 3 will not be hit
            DelaySet copy;
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              if (Idiom3CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_INIT);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM3_STATE_E0_E1_WATCH:
        // DEBUG-ASSERT(0); XXX inaccuracy here
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
            OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_INIT);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        UnlockSchedStatus();
        break;
      case IDIOM3_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom3ClearDelaySet(&copy);
            Idiom3SetState(IDIOM3_STATE_INIT);
            UnlockSchedStatus();
            Idiom3WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom3CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom3ClearDelaySet(&copy);
              Idiom3SetState(IDIOM3_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom3WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1]) ||
              OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom3CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom3ClearDelaySet(&copy);
                Idiom3SetState(IDIOM3_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom3WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom3WatchInstCount(timestamp_t c) {
  Idiom3SchedStatus *s = idiom3_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM3_STATE_E0_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ += c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom3ClearDelaySet(&copy);
          Idiom3SetState(IDIOM3_STATE_INIT);
          UnlockSchedStatus();
          Idiom3WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E0_E1_WATCH:
      // DEBUG_ASSERT(0); XXX inaccuracy here
      UnlockSchedStatus();
      break;
    case IDIOM3_STATE_E1_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ += c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          thread_id_t target = s->thd_id_[1];
          Idiom3SetState(IDIOM3_STATE_INIT);
          UnlockSchedStatus();
          SetPriorityMax(curr_thd_id);
          SetPriorityNormal(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E0_WATCH_E3:
      UnlockSchedStatus();
      break;
    case IDIOM3_STATE_E1_WATCH_E3:
      UnlockSchedStatus();
      break;
    case IDIOM3_STATE_E1_WATCH_E2:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ += c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          thread_id_t target = s->thd_id_[1];
          Idiom3SetState(IDIOM3_STATE_INIT);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM3_STATE_E1_WATCH_E2_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ += c;
        if (s->window_ >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[1];
          Idiom3ClearDelaySet(&copy);
          Idiom3SetState(IDIOM3_STATE_INIT);
          UnlockSchedStatus();
          Idiom3WakeDelaySet(&copy);
          SetPriorityNormal(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom3WatchMutexLock(address_t addr) {
  Idiom3WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom3WatchMutexUnlock(address_t addr) {
  Idiom3WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom3WatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  Idiom3WatchAccess(addr, size);
  if (!cand)
    Idiom3CheckFlush();
}

void SchedulerCommon::Idiom3WatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  Idiom3WatchAccess(addr, size);
  if (!cand)
    Idiom3CheckFlush();
}

void SchedulerCommon::Idiom3CheckFlush() {
  Idiom3SchedStatus *s = idiom3_sched_status_;

  // define a static local variable
  static int token = 0;

  switch (s->state_) {
    case IDIOM3_STATE_INIT:
    case IDIOM3_STATE_E0:
    case IDIOM3_STATE_E1:
    case IDIOM3_STATE_DONE:
      if (token-- <= 0) {
        FlushWatch();
        token = 10;
      }
      break;
    default:
      break;
  }
}

bool SchedulerCommon::Idiom3CheckGiveup(int idx) {
  // return true means actual give up
  // return false means one more chance
  if (YieldWithDelay()) {
    Idiom3SchedStatus *s = idiom3_sched_status_;
    thread_id_t curr_thd_id = PIN_ThreadUid();
    DEBUG_FMT_PRINT_SAFE("[T%lx] Check giveup\n", curr_thd_id);
    static unsigned long last_state[] = {IDIOM3_STATE_INVALID,
                                         IDIOM3_STATE_INVALID,
                                         IDIOM3_STATE_INVALID,
                                         IDIOM3_STATE_INVALID,
                                         IDIOM3_STATE_INVALID};
    static thread_id_t last_thd[] = {INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID};
    static int time_delayed_each[] = {0, 0, 0, 0, 0};
    static int time_delayed_total = 0;
    if (time_delayed_each[idx] <= knob_->ValueInt("yield_delay_min_each") ||
        time_delayed_total <= knob_->ValueInt("yield_delay_max_total")) {
      if (s->state_ != last_state[idx] || curr_thd_id != last_thd[idx]) {
        DEBUG_FMT_PRINT_SAFE("[T%lx] time delay\n", curr_thd_id);
        int time_unit = knob_->ValueInt("yield_delay_unit");
        last_state[idx] = s->state_;
        last_thd[idx] = curr_thd_id;
        time_delayed_each[idx] += time_unit;
        time_delayed_total += time_unit;
        UnlockSchedStatus();
        usleep(1000 * time_unit);
        LockSchedStatus();
        return false;
      } else {
        return true;
      }
    } else {
      return true;
    }
  } else {
    return true;
  }
}

void SchedulerCommon::Idiom3SetState(unsigned long s) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set state: %s\n", PIN_ThreadUid(),
                       Idiom3SchedStatus::StateToString(s).c_str());
  idiom3_sched_status_->state_ = s;
}

void SchedulerCommon::Idiom3ClearDelaySet(DelaySet *copy) {
  *copy = idiom3_sched_status_->delay_set_;
  idiom3_sched_status_->delay_set_.clear();
}

void SchedulerCommon::Idiom3WakeDelaySet(DelaySet *copy) {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  SetPriorityMax(curr_thd_id);
  for (DelaySet::iterator it = copy->begin(); it != copy->end(); ++it) {
    if (*it != curr_thd_id)
      SetPriorityNormal(*it);
  }
}

void SchedulerCommon::Idiom4BeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_WRITE)
    return;

  switch (idx) {
    case 0:
      Idiom4BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom4BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom4BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom4BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4BeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_READ)
    return;

  switch (idx) {
    case 0:
      Idiom4BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom4BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom4BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom4BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4AfteriRootMem(UINT32 idx) {
  switch (idx) {
    case 0:
      Idiom4AfterEvent0();
      break;
    case 1:
      Idiom4AfterEvent1();
      break;
    case 2:
      Idiom4AfterEvent2();
      break;
    case 3:
      Idiom4AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4BeforeiRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom4BeforeEvent1(addr, WORD_SIZE);
      break;
    case 3:
      Idiom4BeforeEvent3(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4AfteriRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 1:
      Idiom4AfterEvent1();
      break;
    case 3:
      Idiom4AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4BeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom4BeforeEvent0(addr, WORD_SIZE);
      break;
    case 2:
      Idiom4BeforeEvent2(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4AfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom4AfterEvent0();
      break;
    case 2:
      Idiom4AfterEvent2();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom4BeforeEvent0(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event0", 1);

  DEBUG_FMT_PRINT_SAFE("[T%lx] before event 0, addr=0x%lx, size=%lx\n",
                       curr_thd_id, addr, size);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 0\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_INIT:
        s->thd_id_[0] = curr_thd_id;
        s->addr_[0] = addr;
        s->size_[0] = size;
        Idiom4SetState(IDIOM4_STATE_E0);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM4_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          // has to execute event 0, go to watch mode
          Idiom4SetState(IDIOM4_STATE_E0_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        // check overlap
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          thread_id_t target = s->thd_id_[1];
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom4SetState(IDIOM4_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay since event 3 will not be hit
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E0);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX should not reach here
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be reached
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
      case IDIOM4_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              SetPriorityHigh(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E0);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E0);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  }
}

void SchedulerCommon::Idiom4BeforeEvent1(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event1", 1);

  DEBUG_FMT_PRINT_SAFE("[T%lx] before event 1, addr=0x%lx, size=%lx\n",
                       curr_thd_id, addr, size);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 1\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_INIT:
        s->thd_id_[1] = curr_thd_id;
        s->addr_[1] = addr;
        s->size_[1] = size;
        Idiom4SetState(IDIOM4_STATE_E1);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM4_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        // check conflict
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          thread_id_t target = s->thd_id_[0];
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom4SetState(IDIOM4_STATE_E0_E1);
          UnlockSchedStatus();
          // force to execute event 0 first
          SetPriorityMax(curr_thd_id);
          SetPriorityHigh(target);
          SetPriorityNormal(curr_thd_id);
          restart = true;
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom4CheckGiveup(1)) {
            // has to go back to initial state
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay because event 3 will not be hit
            DelaySet copy;
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
            // the dependency e0->e1 is satisfied
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E0_E1_WATCH);
            UnlockSchedStatus();
            // force to execute thd[1] first
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id == s->thd_id_[1]);
        DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
        // XXX confirm there since e1 might be blocked
        // since it might be acquiring a lock
        Idiom4SetState(IDIOM4_STATE_E1_WATCH_X);
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay sine event 3 will not be reached
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          // should execute this e1 and goto E1_WATCH_E3
          DelaySet copy;
          s->thd_id_[1] = curr_thd_id;
          s->addr_[1] = addr;
          s->size_[1] = size;
          Idiom4ClearDelaySet(&copy);
          Idiom4SetState(IDIOM4_STATE_E1_WATCH_E3_X);
          UnlockSchedStatus();
          Idiom4WakeDelaySet(&copy);
          SetPriorityHigh(curr_thd_id);
          // force to execute this event 1 first
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              SetPriorityHigh(curr_thd_id);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            thread_id_t target0 = s->thd_id_[0];
            thread_id_t target1 = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target0);
            SetPriorityNormal(target1);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target1 = s->thd_id_[1];
              s->thd_id_[1] = curr_thd_id;
              s->addr_[1] = addr;
              s->size_[1] = size;
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target1);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom4BeforeEvent2(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event2", 1);

  DEBUG_FMT_PRINT_SAFE("[T%lx] before event 2, addr=0x%lx, size=%lx\n",
                       curr_thd_id, addr, size);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 2\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay since event 3 will not be hit
            DelaySet copy;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_INIT);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX should not reach here
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // ignore this event since it does not form an idiom-4 iroot
            UnlockSchedStatus();
          } else {
            // check recorded addr set
            if (Idiom4Recorded(addr, size)) {
              // ignore this event since it will not form an idiom-4 iroot
              UnlockSchedStatus();
            } else {
              // goto state E1_WATCH_E2
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom4SetState(IDIOM4_STATE_E1_WATCH_E2);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityHigh(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_INIT);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
            // iroot conditions are satisfied
            thread_id_t target = s->thd_id_[3];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom4SetState(IDIOM4_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            // force to execute this event 2
          } else if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
                     OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // has to ignore this event2
            UnlockSchedStatus();
          } else {
            if (Idiom4Recorded(addr, size)) {
              // has to ignore this event 2
              UnlockSchedStatus();
            } else {
              // random choice
              //  1) goto state E1_WATCH_E2
              //  2) ignore this event2
              if (RandomChoice(0.5)) {
                thread_id_t target = s->thd_id_[3];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom4SetState(IDIOM4_STATE_E1_WATCH_E2);
                UnlockSchedStatus();
                SetPriorityMax(curr_thd_id);
                SetPriorityHigh(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
              }
            }
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // goto state E1_WATCH
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          thread_id_t target = s->thd_id_[0];
          Idiom4SetState(IDIOM4_STATE_E1_WATCH_E2_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // cannot be delayed any more
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom4BeforeEvent3(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event3", 1);

  DEBUG_FMT_PRINT_SAFE("[T%lx] before event 3, addr=0x%lx, size=%lx\n",
                       curr_thd_id, addr, size);

  while (true) {
    // control variables
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 3\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // go back to initial state
            DelaySet copy;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            if (Idiom4Recorded(addr, size)) {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            } else {
              // goto state E0_WATCH_E3
              s->thd_id_[3] = curr_thd_id;
              s->addr_[3] = addr;
              s->size_[3] = size;
              Idiom4SetState(IDIOM4_STATE_E0_WATCH_E3);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_INIT);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX inaccuracy here
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // go back to initial state
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            if (Idiom4Recorded(addr, size)) {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            } else {
              // goto state E1_WATCH_E3
              thread_id_t target = s->thd_id_[1];
              s->thd_id_[3] = curr_thd_id;
              s->addr_[3] = addr;
              s->size_[3] = size;
              Idiom4SetState(IDIOM4_STATE_E1_WATCH_E3);
              UnlockSchedStatus();
              SetPriorityMax(curr_thd_id);
              SetPriorityHigh(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom4CheckGiveup(3)) {
            DelaySet copy;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E0_WATCH);
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_INIT);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          thread_id_t target = s->thd_id_[1];
          Idiom4SetState(IDIOM4_STATE_E1_WATCH);
          Idiom4RecordAccess(addr, size);
          UnlockSchedStatus();
          SetPriorityMax(curr_thd_id);
          SetPriorityHigh(target);
          SetPriorityNormal(curr_thd_id);
        } else if (curr_thd_id == s->thd_id_[1]) {
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // iroot conditions are satisfied
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom4SetState(IDIOM4_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
          } else if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
                     OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // go to initial state
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            if (Idiom4Recorded(addr, size)) {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            } else {
              // random choice
              //  1) goto state E1_WATCH_E3
              //  2) ignore this event 3
              if (RandomChoice(0.5)) {
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[3] = curr_thd_id;
                s->addr_[3] = addr;
                s->size_[3] = size;
                Idiom4SetState(IDIOM4_STATE_E1_WATCH_E3);
                UnlockSchedStatus();
                SetPriorityMax(curr_thd_id);
                SetPriorityHigh(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                Idiom4RecordAccess(addr, size);
                UnlockSchedStatus();
              }
            }
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // iroot conditions are satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_E0_E1_E2_E3);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityHigh(curr_thd_id);
            // force to execute current event 3
          } else if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
                     OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // go to initial state
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            if (Idiom4Recorded(addr, size)) {
              Idiom4RecordAccess(addr, size);
              UnlockSchedStatus();
            } else {
              // random choice
              //  1) goto state E1_WATCH_E3
              //  2) ignore this event 3
              if (RandomChoice(0.5)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[3] = curr_thd_id;
                s->addr_[3] = addr;
                s->size_[3] = size;
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH_E3);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityHigh(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                Idiom4RecordAccess(addr, size);
                UnlockSchedStatus();
              }
            }
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom4AfterEvent0() {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 0\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM4_STATE_E0_E1:
      if (curr_thd_id == s->thd_id_[0]) {
        thread_id_t target = s->thd_id_[1];
        s->window_ = 0;
        Idiom4ClearRecordedAccess();
        Idiom4SetState(IDIOM4_STATE_E0_E1_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM4_STATE_E0_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_ = 0;
        Idiom4ClearRecordedAccess();
        Idiom4SetState(IDIOM4_STATE_E0_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom4AfterEvent1() {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 1\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM4_STATE_E1_WATCH_X:
      if (curr_thd_id == s->thd_id_[1]) {
        thread_id_t target = s->thd_id_[0];
        Idiom4SetState(IDIOM4_STATE_E1_WATCH);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
        SetPriorityNormal(target);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM4_STATE_E1_WATCH_E3_X:
      if (curr_thd_id == s->thd_id_[1]) {
        Idiom4SetState(IDIOM4_STATE_E1_WATCH_E3);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom4AfterEvent2() {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 2\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM4_STATE_E1_WATCH_E2_WATCH_X:
      if (curr_thd_id == s->thd_id_[2]) {
        thread_id_t target = s->thd_id_[0];
        Idiom4SetState(IDIOM4_STATE_E1_WATCH_E2_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM4_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[2]) {
        thread_id_t target = s->thd_id_[3];
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom4AfterEvent3() {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 3\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM4_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[3]) {
        ActivelyExposed();
        DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                             curr_thd_id, curr_iroot_->id());
        Idiom4SetState(IDIOM4_STATE_DONE);
        UnlockSchedStatus();
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  }
}

void SchedulerCommon::Idiom4WatchAccess(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("watch_access", 1);

  while (true) {
    // control variables
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need delay since event 3 will not be hit
            DelaySet copy;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              if (Idiom4CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_INIT);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX inaccuracy here
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority());
          DEBUG_ASSERT(GetPriority(curr_thd_id) != HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          UnlockSchedStatus();
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_INIT);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be hit
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // goto state E1_WATCH
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0]) ||
              OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
            // no need to delay since event 3 will not be reached
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_E1_WATCH);
            Idiom4RecordAccess(s->addr_[2], s->size_[2]); // XXX this is a hack
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityHigh(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            Idiom4RecordAccess(addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed any more
            if (Idiom4CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom4ClearDelaySet(&copy);
              Idiom4SetState(IDIOM4_STATE_E1_WATCH);
              UnlockSchedStatus();
              Idiom4WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityHigh(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom4CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target1 = s->thd_id_[1];
                Idiom4ClearDelaySet(&copy);
                Idiom4SetState(IDIOM4_STATE_E1_WATCH);
                UnlockSchedStatus();
                Idiom4WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityHigh(target1);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom4WatchInstCount(timestamp_t c) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  while (true) {
    // control variable
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM4_STATE_E0_WATCH:
        if (curr_thd_id == s->thd_id_[0]) {
          s->window_ += c;
          if (s->window_ >= vw_) {
            // window expired
            DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
            DelaySet copy;
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E0_E1_WATCH:
        // DEBUG_ASSERT(0); XXX inaccuracy here
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH:
        if (curr_thd_id == s->thd_id_[0]) {
          s->window_ += c;
          if (s->window_ >= vw_) {
            // window expired
            DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityMax(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E0_WATCH_E3:
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH_E3:
        UnlockSchedStatus();
        break;
      case IDIOM4_STATE_E1_WATCH_E2:
        if (curr_thd_id == s->thd_id_[0]) {
          s->window_ += c;
          if (s->window_ >= vw_) {
            // window expired
            DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
            thread_id_t target = s->thd_id_[1];
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        if (curr_thd_id == s->thd_id_[0]) {
          s->window_ += c;
          if (s->window_ >= vw_) {
            // window expired
            DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
            DelaySet copy;
            thread_id_t target = s->thd_id_[1];
            Idiom4ClearDelaySet(&copy);
            Idiom4SetState(IDIOM4_STATE_INIT);
            UnlockSchedStatus();
            Idiom4WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityNormal(curr_thd_id);
          } else {
            UnlockSchedStatus();
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom4WatchMutexLock(address_t addr) {
  Idiom4WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom4WatchMutexUnlock(address_t addr) {
  Idiom4WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom4WatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  Idiom4WatchAccess(addr, size);
  if (!cand)
    Idiom4CheckFlush();
}

void SchedulerCommon::Idiom4WatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  Idiom4WatchAccess(addr, size);
  if (!cand)
    Idiom4CheckFlush();
}

void SchedulerCommon::Idiom4CheckFlush() {
  Idiom4SchedStatus *s = idiom4_sched_status_;

  // define a static local variable
  static int token = 0;

  switch (s->state_) {
    case IDIOM4_STATE_INIT:
    case IDIOM4_STATE_E0:
    case IDIOM4_STATE_E1:
    case IDIOM4_STATE_DONE:
      if (token-- <= 0) {
        FlushWatch();
        token = 10;
      }
      break;
    default:
      break;
  }
}

bool SchedulerCommon::Idiom4CheckGiveup(int idx) {
  // return true means actual give up
  // return false means one more chance
  if (YieldWithDelay()) {
    Idiom4SchedStatus *s = idiom4_sched_status_;
    thread_id_t curr_thd_id = PIN_ThreadUid();
    DEBUG_FMT_PRINT_SAFE("[T%lx] Check giveup\n", curr_thd_id);
    static unsigned long last_state[] = {IDIOM4_STATE_INVALID,
                                         IDIOM4_STATE_INVALID,
                                         IDIOM4_STATE_INVALID,
                                         IDIOM4_STATE_INVALID,
                                         IDIOM4_STATE_INVALID};
    static thread_id_t last_thd[] = {INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID};
    static int time_delayed_each[] = {0, 0, 0, 0, 0};
    static int time_delayed_total = 0;
    if (time_delayed_each[idx] <= knob_->ValueInt("yield_delay_min_each") ||
        time_delayed_total <= knob_->ValueInt("yield_delay_max_total")) {
      if (s->state_ != last_state[idx] || curr_thd_id != last_thd[idx]) {
        DEBUG_FMT_PRINT_SAFE("[T%lx] time delay\n", curr_thd_id);
        int time_unit = knob_->ValueInt("yield_delay_unit");
        last_state[idx] = s->state_;
        last_thd[idx] = curr_thd_id;
        time_delayed_each[idx] += time_unit;
        time_delayed_total += time_unit;
        UnlockSchedStatus();
        usleep(1000 * time_unit);
        LockSchedStatus();
        return false;
      } else {
        return true;
      }
    } else {
      return true;
    }
  } else {
    return true;
  }
}

void SchedulerCommon::Idiom4SetState(unsigned long s) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set state: %s\n", PIN_ThreadUid(),
                       Idiom4SchedStatus::StateToString(s).c_str());
  idiom4_sched_status_->state_ = s;
}

void SchedulerCommon::Idiom4ClearDelaySet(DelaySet *copy) {
  *copy = idiom4_sched_status_->delay_set_;
  idiom4_sched_status_->delay_set_.clear();
}

void SchedulerCommon::Idiom4WakeDelaySet(DelaySet *copy) {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  SetPriorityMax(curr_thd_id);
  for (DelaySet::iterator it = copy->begin(); it != copy->end(); ++it) {
    if (*it != curr_thd_id)
      SetPriorityNormal(*it);
  }
}

void SchedulerCommon::Idiom4RecordAccess(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  s->recorded_addr_set_.insert(addr);
}

void SchedulerCommon::Idiom4ClearRecordedAccess() {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  s->recorded_addr_set_.clear();
}

bool SchedulerCommon::Idiom4Recorded(address_t addr, size_t size) {
  Idiom4SchedStatus *s = idiom4_sched_status_;
  if (s->recorded_addr_set_.find(addr) != s->recorded_addr_set_.end())
    return true;
  else
    return false;
}

void SchedulerCommon::Idiom5BeforeiRootMemRead(UINT32 idx, address_t addr,
                                         size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_WRITE)
    return;

  switch (idx) {
    case 0:
      Idiom5BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom5BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom5BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom5BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5BeforeiRootMemWrite(UINT32 idx, address_t addr,
                                          size_t size) {
  if (curr_iroot_->GetEvent(idx)->type() == IROOT_EVENT_MEM_READ)
    return;

  switch (idx) {
    case 0:
      Idiom5BeforeEvent0(addr, size);
      break;
    case 1:
      Idiom5BeforeEvent1(addr, size);
      break;
    case 2:
      Idiom5BeforeEvent2(addr, size);
      break;
    case 3:
      Idiom5BeforeEvent3(addr, size);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5AfteriRootMem(UINT32 idx) {
  switch (idx) {
    case 0:
      Idiom5AfterEvent0();
      break;
    case 1:
      Idiom5AfterEvent1();
      break;
    case 2:
      Idiom5AfterEvent2();
      break;
    case 3:
      Idiom5AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5BeforeiRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom5BeforeEvent0(addr, WORD_SIZE);
      break;
    case 1:
      Idiom5BeforeEvent1(addr, WORD_SIZE);
      break;
    case 2:
      Idiom5BeforeEvent2(addr, WORD_SIZE);
      break;
    case 3:
      Idiom5BeforeEvent3(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5AfteriRootMutexLock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom5AfterEvent0();
      break;
    case 1:
      Idiom5AfterEvent1();
      break;
    case 2:
      Idiom5AfterEvent2();
      break;
    case 3:
      Idiom5AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5BeforeiRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom5BeforeEvent0(addr, WORD_SIZE);
      break;
    case 1:
      Idiom5BeforeEvent1(addr, WORD_SIZE);
      break;
    case 2:
      Idiom5BeforeEvent2(addr, WORD_SIZE);
      break;
    case 3:
      Idiom5BeforeEvent3(addr, WORD_SIZE);
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5AfteriRootMutexUnlock(UINT32 idx, address_t addr) {
  switch (idx) {
    case 0:
      Idiom5AfterEvent0();
      break;
    case 1:
      Idiom5AfterEvent1();
      break;
    case 2:
      Idiom5AfterEvent2();
      break;
    case 3:
      Idiom5AfterEvent3();
      break;
    default:
      Abort("idx invalid\n");
  }
}

void SchedulerCommon::Idiom5BeforeEvent0(address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event0", 1);

  while (true) {
    // control variable
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 0\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM5_STATE_INIT:
        s->thd_id_[0] = curr_thd_id;
        s->addr_[0] = addr;
        s->size_[0] = size;
        Idiom5SetState(IDIOM5_STATE_E0);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM5_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          // has to execute event 0, go to watch mode
          Idiom5SetState(IDIOM5_STATE_E0_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        // check NON overlap
        if (!OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
          thread_id_t target = s->thd_id_[2];
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom5SetState(IDIOM5_STATE_E0_E2);
          UnlockSchedStatus();
          // execute event 0 first, then event 2
          SetPriorityHigh(curr_thd_id);
          SetPriorityNormal(target);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom5SetState(IDIOM5_STATE_E0);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (!Idiom5Recorded(2, addr, size)) {
            // goto state E0_E2_WATCH_X
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_X);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
            // force to execute event 0
          } else {
            // check overlap
            if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
              if (GetPriority(curr_thd_id) == LowerPriority()) {
                if (Idiom5CheckGiveup(0)) {
                  DelaySet copy;
                  thread_id_t target = s->thd_id_[2];
                  s->thd_id_[0] = curr_thd_id;
                  s->addr_[0] = addr;
                  s->size_[0] = size;
                  Idiom5ClearDelaySet(&copy);
                  Idiom5SetState(IDIOM5_STATE_E0);
                  UnlockSchedStatus();
                  Idiom5WakeDelaySet(&copy);
                  SetPriorityNormal(target);
                  SetPriorityLow(curr_thd_id);
                  restart = true;
                } else {
                  UnlockSchedStatus();
                  restart = true;
                }
              } else {
                s->delay_set_.insert(curr_thd_id);
                UnlockSchedStatus();
                SetPriorityLow(curr_thd_id);
                restart = true;
              }
            } else {
              // random choice
              if (RandomChoice(0.5)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
              }
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[3], s->size_[3]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[0] = curr_thd_id;
            s->addr_[0] = addr;
            s->size_[0] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[1], s->size_[1], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          DelaySet copy;
          s->thd_id_[0] = curr_thd_id;
          s->addr_[0] = addr;
          s->size_[0] = size;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E1_X);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          // force to execute event 0
        } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed any more
            if (Idiom5CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          // random choice
          if (Idiom5Recorded(2, addr, size)) {
            // restart with low prob
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_X);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay, goto state E0_WATCH
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH_E1);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(2, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(0)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(0)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[0] = curr_thd_id;
                s->addr_[0] = addr;
                s->size_[0] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[0] = curr_thd_id;
              s->addr_[0] = addr;
              s->size_[0] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom5BeforeEvent1(address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event1", 1);

  while (true) {
    // control variable
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 1\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM5_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_INIT);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (!Idiom5Recorded(2, addr, size)) {
            // TODO: check assertion could fail
            DEBUG_ASSERT(!OVERLAP(addr, size, s->addr_[2], s->size_[2]));
            DelaySet copy;
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH_E1);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
              // no need to delay
              DelaySet copy;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[3], s->size_[3]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM5_STATE_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[1], s->size_[1], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom5CheckGiveup(1)) {
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // goto state E0_E2_WATCH_E1
            // set priority of thd[0] to high
            DelaySet copy;
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E1);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // goto state E0_WATCH
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH_E1);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[1]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          DEBUG_ASSERT(OVERLAP(addr, size, s->addr_[1], s->size_[1]));
          DelaySet copy;
          thread_id_t target = s->thd_id_[0];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E1_WATCH_X);
          Idiom5RecordAccess(2, addr, size);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(target);
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // done, goto state E0_E1_E2_E3
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E1_E2_E3);
            ActivelyExposed();
            DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                                 curr_thd_id, curr_iroot_->id());
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityHigh(target);
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(1)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // iroot satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[1] = curr_thd_id;
            s->addr_[1] = addr;
            s->size_[1] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E1_E2_E3);
            ActivelyExposed();
            DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                                 curr_thd_id, curr_iroot_->id());
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(1)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            UnlockSchedStatus();
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom5BeforeEvent2(address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event2", 1);

  while (true) {
    // control variable
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 2\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM5_STATE_INIT:
        s->thd_id_[2] = curr_thd_id;
        s->addr_[2] = addr;
        s->size_[2] = size;
        Idiom5SetState(IDIOM5_STATE_E2);
        UnlockSchedStatus();
        SetPriorityLow(curr_thd_id);
        restart = true;
        break;
      case IDIOM5_STATE_E0:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        // check NON overlap
        if (!OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          thread_id_t target = s->thd_id_[0];
          s->thd_id_[2] = curr_thd_id;
          s->addr_[2] = addr;
          s->size_[2] = size;
          Idiom5SetState(IDIOM5_STATE_E0_E2);
          UnlockSchedStatus();
          // execute event 0 first, then event 2
          SetPriorityMax(curr_thd_id);
          SetPriorityHigh(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5SetState(IDIOM5_STATE_E2);
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          // has to execute event 2, go to watch mode
          Idiom5SetState(IDIOM5_STATE_E2_WATCH_X);
          UnlockSchedStatus();
          SetPriorityHigh(curr_thd_id);
        } else {
          // random choice
          if (RandomChoice(0.5)) {
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            UnlockSchedStatus();
            SetPriorityHigh(curr_thd_id);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (!Idiom5Recorded(0, addr, size)) {
            // goto state E0_E2_WATCH_X
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_X);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
            // force to execute event 2
          } else {
            // check overlap
            if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
              if (GetPriority(curr_thd_id) == LowerPriority()) {
                // cannot be delayed any more
                if (Idiom5CheckGiveup(2)) {
                  DelaySet copy;
                  thread_id_t target = s->thd_id_[0];
                  s->thd_id_[2] = curr_thd_id;
                  s->addr_[2] = addr;
                  s->size_[2] = size;
                  Idiom5ClearDelaySet(&copy);
                  Idiom5SetState(IDIOM5_STATE_E2);
                  UnlockSchedStatus();
                  Idiom5WakeDelaySet(&copy);
                  SetPriorityNormal(target);
                  SetPriorityLow(curr_thd_id);
                  restart = true;
                } else {
                  UnlockSchedStatus();
                  restart = true;
                }
              } else {
                s->delay_set_.insert(curr_thd_id);
                UnlockSchedStatus();
                SetPriorityLow(curr_thd_id);
                restart = true;
              }
            } else {
              // random choice
              if (RandomChoice(0.5)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
              }
            }
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[3], s->size_[3]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else if (OVERLAP(addr, size, s->addr_[3], s->size_[3])) {
          DelaySet copy;
          s->thd_id_[2] = curr_thd_id;
          s->addr_[2] = addr;
          s->size_[2] = size;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E3_X);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          // force to execute event 2
        } else {
          // random choice
          if (Idiom5Recorded(0, addr, size)) {
            // restart with low prob
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_X);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[1], s->size_[1], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (OVERLAP(addr, size, s->addr_[1], s->size_[1])) {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot not be delayed
            if (Idiom5CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          // random choice (restart with low prob)
          if (RandomChoice(0.2)) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[2] = curr_thd_id;
            s->addr_[2] = addr;
            s->size_[2] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay, goto state E2_WATCH
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[1]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH_E3);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              }
            } else {
              Idiom5RecordAccess(2, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(2)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              DelaySet copy;
              thread_id_t target0 = s->thd_id_[0];
              thread_id_t target2 = s->thd_id_[2];
              s->thd_id_[2] = curr_thd_id;
              s->addr_[2] = addr;
              s->size_[2] = size;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target0);
              SetPriorityNormal(target2);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
            }
          } else {
            // random choice (restart with low prob)
            if (RandomChoice(0.2)) {
              if (Idiom5Recorded(0, addr, size)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                s->thd_id_[2] = curr_thd_id;
                s->addr_[2] = addr;
                s->size_[2] = size;
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityLow(curr_thd_id);
                restart = true;
              } else {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart= true;
              }
            } else {
              UnlockSchedStatus();
            }
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom5BeforeEvent3(address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("event3", 1);

  while (true) {
    // control variable
    bool restart = false;
    DEBUG_FMT_PRINT_SAFE("[T%lx] pre event 3\n", curr_thd_id);

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM5_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (!Idiom5Recorded(0, addr, size)) {
            DEBUG_ASSERT(!OVERLAP(addr, size, s->addr_[0], s->size_[0]));
            DelaySet copy;
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH_E3);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
              // no need to delay
              DelaySet copy;
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              Idiom5RecordAccess(0, addr, size);
              UnlockSchedStatus();
            }
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_INIT);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[3], s->size_[3]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (Idiom5CheckGiveup(3)) {
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
          } else {
            UnlockSchedStatus();
            restart = true;
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[1], s->size_[1], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // goto state E2_WATCH
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // goto state E0_E2_WATCH_E3
            // set priority of thd[2] to high
            DelaySet copy;
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E3);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // done, goto state E0_E1_E2_E3
            // force to execute event 1 first
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E1_E2_E3);
            ActivelyExposed();
            DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                                 curr_thd_id, curr_iroot_->id());
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityHigh(target);
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          DEBUG_ASSERT(OVERLAP(addr, size, s->addr_[3], s->size_[3]));
          DelaySet copy;
          thread_id_t target = s->thd_id_[2];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E3_WATCH_X);
          Idiom5RecordAccess(0, addr, size);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(target);
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH_E3);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // iroot satisfied
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            s->thd_id_[3] = curr_thd_id;
            s->addr_[3] = addr;
            s->size_[3] = size;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_E1_E2_E3);
            ActivelyExposed();
            DEBUG_FMT_PRINT_SAFE("[T%lx] iRoot %u exposed.\n",
                                 curr_thd_id, curr_iroot_->id());
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(target);
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed any more
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(3)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(3)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            UnlockSchedStatus();
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom5AfterEvent0() {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 0\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM5_STATE_E0_E2:
      if (curr_thd_id == s->thd_id_[0]) {
        thread_id_t target = s->thd_id_[2];
        s->window_[0] = 0;
        Idiom5ClearRecordedAccess(0);
        Idiom5RecordAccess(0, s->addr_[0], s->size_[0]);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityHigh(curr_thd_id);
        SetPriorityMax(target);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_[0] = 0;
        Idiom5ClearRecordedAccess(0);
        Idiom5RecordAccess(0, s->addr_[0], s->size_[0]);
        Idiom5SetState(IDIOM5_STATE_E0_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_X:
      if (curr_thd_id == s->thd_id_[0]) {
        thread_id_t target = s->thd_id_[2];
        s->window_[0] = 0;
        Idiom5ClearRecordedAccess(0);
        Idiom5RecordAccess(0, s->addr_[0], s->size_[0]);
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E1_X:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_[0] = 0;
        Idiom5ClearRecordedAccess(0);
        Idiom5RecordAccess(0, s->addr_[0], s->size_[0]);
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E1);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom5AfterEvent1() {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 1\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH_X:
      if (curr_thd_id == s->thd_id_[1]) {
        thread_id_t target = s->thd_id_[0];
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E1_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[1]) {
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        UnlockSchedStatus();
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom5AfterEvent2() {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 2\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM5_STATE_E0_E2:
      if (curr_thd_id == s->thd_id_[2]) {
        s->window_[1] = 0;
        Idiom5ClearRecordedAccess(2);
        Idiom5RecordAccess(2, s->addr_[2], s->size_[2]);
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E2_WATCH_X:
      if (curr_thd_id == s->thd_id_[2]) {
        s->window_[1] = 0;
        Idiom5ClearRecordedAccess(2);
        Idiom5RecordAccess(2, s->addr_[2], s->size_[2]);
        Idiom5SetState(IDIOM5_STATE_E2_WATCH);
        UnlockSchedStatus();
        FlushWatch();
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_X:
      if (curr_thd_id == s->thd_id_[2]) {
        thread_id_t target = s->thd_id_[0];
        s->window_[1] = 0;
        Idiom5ClearRecordedAccess(2);
        Idiom5RecordAccess(2, s->addr_[2], s->size_[2]);
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E3_X:
      if (curr_thd_id == s->thd_id_[2]) {
        s->window_[1] = 0;
        Idiom5ClearRecordedAccess(2);
        Idiom5RecordAccess(2, s->addr_[2], s->size_[2]);
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E3);
        UnlockSchedStatus();
        SetPriorityHigh(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom5AfterEvent3() {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_FMT_PRINT_SAFE("[T%lx] after event 3\n", curr_thd_id);

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH_X:
      if (curr_thd_id == s->thd_id_[3]) {
        thread_id_t target = s->thd_id_[2];
        Idiom5SetState(IDIOM5_STATE_E0_E2_WATCH_E3_WATCH);
        UnlockSchedStatus();
        SetPriorityMax(curr_thd_id);
        SetPriorityHigh(target);
        SetPriorityLow(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E1_E2_E3:
      if (curr_thd_id == s->thd_id_[3]) {
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        UnlockSchedStatus();
        SetPriorityNormal(curr_thd_id);
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom5WatchAccess(address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  DEBUG_STAT_INC("watch_access", 1);

  while (true) {
    // control variables
    bool restart = false;

    LockSchedStatus();
    switch (s->state_) {
      case IDIOM5_STATE_E0_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_INIT);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[0];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // no need to delay
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_INIT);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[3], s->size_[3]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM5_STATE_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[1], s->size_[1], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
          if (GetPriority(curr_thd_id) == LowerPriority()) {
            // cannot be delayed
            if (Idiom5CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityLow(curr_thd_id);
            restart = true;
          }
        } else {
          UnlockSchedStatus();
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] != s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E2_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityLow(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_E0_WATCH);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityLow(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[2]);
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH_E1);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        DEBUG_ASSERT(curr_thd_id != s->thd_id_[0]);
        if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (RandomChoice(0.5)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E0_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityLow(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_E2_WATCH);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DEBUG_ASSERT(GetPriority(curr_thd_id) != LowerPriority() &&
                         GetPriority(curr_thd_id) != MinPriority());
            s->delay_set_.insert(curr_thd_id);
            UnlockSchedStatus();
            SetPriorityMin(curr_thd_id);
            restart = true;
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[1] == s->thd_id_[2]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[2];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E2_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[0];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            UnlockSchedStatus();
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        DEBUG_ASSERT(s->thd_id_[0] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[2] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[3] != INVALID_THD_ID);
        DEBUG_ASSERT(s->thd_id_[0] == s->thd_id_[3]);
        DEBUG_ASSERT(!OVERLAP(s->addr_[0], s->size_[0], s->addr_[2], s->size_[2]));
        if (curr_thd_id == s->thd_id_[0]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            // cannot be delayed
            if (Idiom5CheckGiveup(4)) {
              DelaySet copy;
              thread_id_t target = s->thd_id_[2];
              Idiom5ClearDelaySet(&copy);
              Idiom5SetState(IDIOM5_STATE_INIT);
              UnlockSchedStatus();
              Idiom5WakeDelaySet(&copy);
              SetPriorityNormal(target);
              SetPriorityNormal(curr_thd_id);
              restart = true;
            } else {
              UnlockSchedStatus();
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          } else {
            Idiom5RecordAccess(0, addr, size);
            UnlockSchedStatus();
          }
        } else if (curr_thd_id == s->thd_id_[2]) {
          DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            DelaySet copy;
            thread_id_t target = s->thd_id_[0];
            Idiom5ClearDelaySet(&copy);
            Idiom5SetState(IDIOM5_STATE_E0_WATCH);
            UnlockSchedStatus();
            Idiom5WakeDelaySet(&copy);
            SetPriorityLow(target);
            SetPriorityNormal(curr_thd_id);
            restart = true;
          } else {
            Idiom5RecordAccess(2, addr, size);
            UnlockSchedStatus();
          }
        } else {
          if (OVERLAP(addr, size, s->addr_[0], s->size_[0])) {
            if (GetPriority(curr_thd_id) == LowerPriority()) {
              // cannot be delayed
              if (Idiom5CheckGiveup(4)) {
                DelaySet copy;
                thread_id_t target0 = s->thd_id_[0];
                thread_id_t target2 = s->thd_id_[2];
                Idiom5ClearDelaySet(&copy);
                Idiom5SetState(IDIOM5_STATE_INIT);
                UnlockSchedStatus();
                Idiom5WakeDelaySet(&copy);
                SetPriorityNormal(target0);
                SetPriorityNormal(target2);
                SetPriorityNormal(curr_thd_id);
                restart = true;
              } else {
                UnlockSchedStatus();
                restart = true;
              }
            } else {
              s->delay_set_.insert(curr_thd_id);
              UnlockSchedStatus();
              SetPriorityLow(curr_thd_id);
              restart = true;
            }
          } else if (OVERLAP(addr, size, s->addr_[2], s->size_[2])) {
            UnlockSchedStatus();
          } else {
            UnlockSchedStatus();
          }
        }
        break;
      default:
        UnlockSchedStatus();
        break;
    } // end of switch

    if (!restart)
      break;
  } // end of while
}

void SchedulerCommon::Idiom5WatchInstCount(timestamp_t c) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  thread_id_t curr_thd_id = PIN_ThreadUid();

  LockSchedStatus();
  switch (s->state_) {
    case IDIOM5_STATE_E0_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
        s->window_[0] += c;
        if (s->window_[0] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_INIT);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E2_WATCH:
      if (curr_thd_id == s->thd_id_[2]) {
        DEBUG_ASSERT(GetPriority(curr_thd_id) == LowerPriority());
        s->window_[1] += c;
        if (s->window_[1] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_INIT);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_WATCH_E3:
      UnlockSchedStatus();
      break;
    case IDIOM5_STATE_E2_WATCH_E1:
      UnlockSchedStatus();
      break;
    case IDIOM5_STATE_E0_E2_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
        s->window_[0] += c;
        if (s->window_[0] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[2];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E2_WATCH);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityLow(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else if (curr_thd_id == s->thd_id_[2]) {
        DEBUG_ASSERT(GetPriority(curr_thd_id) == HigherPriority());
        s->window_[1] += c;
        if (s->window_[1] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[0];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_WATCH);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityLow(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E1:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_[0] += c;
        if (s->window_[0] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E2_WATCH_E1);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E3:
      if (curr_thd_id == s->thd_id_[2]) {
        s->window_[1] += c;
        if (s->window_[1] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_WATCH_E3);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
      if (curr_thd_id == s->thd_id_[0]) {
        s->window_[0] += c;
        if (s->window_[0] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[2];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E2_WATCH);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityLow(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
      if (curr_thd_id == s->thd_id_[2]) {
        s->window_[1] += c;
        if (s->window_[1] >= vw_) {
          // window expired
          DEBUG_FMT_PRINT_SAFE("[T%lx] window expired\n", curr_thd_id);
          DelaySet copy;
          thread_id_t target = s->thd_id_[0];
          Idiom5ClearDelaySet(&copy);
          Idiom5SetState(IDIOM5_STATE_E0_WATCH);
          UnlockSchedStatus();
          Idiom5WakeDelaySet(&copy);
          SetPriorityLow(target);
          SetPriorityNormal(curr_thd_id);
        } else {
          UnlockSchedStatus();
        }
      } else {
        UnlockSchedStatus();
      }
      break;
    default:
      UnlockSchedStatus();
      break;
  } // end of switch
}

void SchedulerCommon::Idiom5WatchMutexLock(address_t addr) {
  Idiom5WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom5WatchMutexUnlock(address_t addr) {
  Idiom5WatchAccess(addr, WORD_SIZE);
}

void SchedulerCommon::Idiom5WatchMemRead(Inst *inst, address_t addr, size_t size,
                                   bool cand) {
  Idiom5WatchAccess(addr, size);
  if (!cand)
    Idiom5CheckFlush();
}

void SchedulerCommon::Idiom5WatchMemWrite(Inst *inst, address_t addr, size_t size,
                                    bool cand) {
  Idiom5WatchAccess(addr, size);
  if (!cand)
    Idiom5CheckFlush();
}

void SchedulerCommon::Idiom5CheckFlush() {
  Idiom5SchedStatus *s = idiom5_sched_status_;

  // define a static local variable
  static int token = 0;

  switch (s->state_) {
    case IDIOM5_STATE_INIT:
    case IDIOM5_STATE_E0:
    case IDIOM5_STATE_E2:
    case IDIOM5_STATE_E0_E1_E2_E3:
    case IDIOM5_STATE_DONE:
      if (token-- <= 0) {
        FlushWatch();
        token = 10;
      }
      break;
    default:
      break;
  }
}

bool SchedulerCommon::Idiom5CheckGiveup(int idx) {
  // return true means actual give up
  // return false means one more chance
  if (YieldWithDelay()) {
    Idiom5SchedStatus *s = idiom5_sched_status_;
    thread_id_t curr_thd_id = PIN_ThreadUid();
    DEBUG_FMT_PRINT_SAFE("[T%lx] Check giveup\n", curr_thd_id);
    static unsigned long last_state[] = {IDIOM5_STATE_INVALID,
                                         IDIOM5_STATE_INVALID,
                                         IDIOM5_STATE_INVALID,
                                         IDIOM5_STATE_INVALID,
                                         IDIOM5_STATE_INVALID};
    static thread_id_t last_thd[] = {INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID,
                                     INVALID_THD_ID};
    static int time_delayed_each[] = {0, 0, 0, 0, 0};
    static int time_delayed_total = 0;
    if (time_delayed_each[idx] <= knob_->ValueInt("yield_delay_min_each") ||
        time_delayed_total <= knob_->ValueInt("yield_delay_max_total")) {
      if (s->state_ != last_state[idx] || curr_thd_id != last_thd[idx]) {
        DEBUG_FMT_PRINT_SAFE("[T%lx] time delay\n", curr_thd_id);
        int time_unit = knob_->ValueInt("yield_delay_unit");
        last_state[idx] = s->state_;
        last_thd[idx] = curr_thd_id;
        time_delayed_each[idx] += time_unit;
        time_delayed_total += time_unit;
        UnlockSchedStatus();
        usleep(1000 * time_unit);
        LockSchedStatus();
        return false;
      } else {
        return true;
      }
    } else {
      return true;
    }
  } else {
    return true;
  }
}

void SchedulerCommon::Idiom5SetState(unsigned long s) {
  DEBUG_FMT_PRINT_SAFE("[T%lx] set state: %s\n", PIN_ThreadUid(),
                       Idiom5SchedStatus::StateToString(s).c_str());
  idiom5_sched_status_->state_ = s;
}

void SchedulerCommon::Idiom5ClearDelaySet(DelaySet *copy) {
  *copy = idiom5_sched_status_->delay_set_;
  idiom5_sched_status_->delay_set_.clear();
}

void SchedulerCommon::Idiom5WakeDelaySet(DelaySet *copy) {
  thread_id_t curr_thd_id = PIN_ThreadUid();
  SetPriorityHigh(curr_thd_id);
  for (DelaySet::iterator it = copy->begin(); it != copy->end(); ++it) {
    if (*it != curr_thd_id)
      SetPriorityNormal(*it);
  }
}

void SchedulerCommon::Idiom5RecordAccess(int idx, address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  if (idx == 0) {
    s->recorded_addr_set0_.insert(addr);
  } else if (idx == 2) {
    s->recorded_addr_set2_.insert(addr);
  } else {
    Abort("Idiom5RecordAccess: invalid idx\n");
  }
}

void SchedulerCommon::Idiom5ClearRecordedAccess(int idx) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  if (idx == 0) {
    s->recorded_addr_set0_.clear();
  } else if (idx == 2) {
    s->recorded_addr_set2_.clear();
  } else {
    Abort("Idiom5Recorded: invalid idx\n");
  }
}

bool SchedulerCommon::Idiom5Recorded(int idx, address_t addr, size_t size) {
  Idiom5SchedStatus *s = idiom5_sched_status_;
  if (idx == 0) {
    return s->recorded_addr_set0_.find(addr) != s->recorded_addr_set0_.end();
  } else if (idx == 2) {
    return s->recorded_addr_set2_.find(addr) != s->recorded_addr_set2_.end();
  } else {
    Abort("Idiom5Recorded: invalid idx\n");
    return false;
  }
}

void SchedulerCommon::__BeforeiRootMemRead(UINT32 idx, ADDRINT addr, UINT32 size) {
  ((SchedulerCommon *)ctrl_)->HandleBeforeiRootMemRead(idx, addr, size);
}

void SchedulerCommon::__BeforeiRootMemWrite(UINT32 idx, ADDRINT addr, UINT32 size) {
  ((SchedulerCommon *)ctrl_)->HandleBeforeiRootMemWrite(idx, addr, size);
}

void SchedulerCommon::__AfteriRootMem(UINT32 idx) {
  ((SchedulerCommon *)ctrl_)->HandleAfteriRootMem(idx);
}

void SchedulerCommon::__WatchMemRead(Inst *inst, ADDRINT addr, UINT32 size,
                               BOOL cand) {
  ((SchedulerCommon *)ctrl_)->HandleWatchMemRead(inst, addr, size, cand);
}

void SchedulerCommon::__WatchMemWrite(Inst *inst, ADDRINT addr, UINT32 size,
                                BOOL cand) {
  ((SchedulerCommon *)ctrl_)->HandleWatchMemWrite(inst, addr, size, cand);
}

void SchedulerCommon::__WatchInstCount(UINT32 c) {
  ((SchedulerCommon *)ctrl_)->HandleWatchInstCount(c);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexLock, SchedulerCommon) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  // check iRoot
  CheckiRootBeforeMutexLock(inst, (address_t)wrapper->arg0());

  wrapper->CallOriginal();

  // check iRoot
  CheckiRootAfterMutexLock(inst, (address_t)wrapper->arg0());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexUnlock, SchedulerCommon) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  // check iRoot
  CheckiRootBeforeMutexUnlock(inst, (address_t)wrapper->arg0());

  wrapper->CallOriginal();

  // check iRoot
  CheckiRootAfterMutexUnlock(inst, (address_t)wrapper->arg0());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

} // namespace idiom


