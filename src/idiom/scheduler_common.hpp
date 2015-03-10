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

// File: idiom/scheduler_common.hpp - Define the common active scheduler
// controller.

#ifndef IDIOM_SCHEDULER_COMMON_HPP_
#define IDIOM_SCHEDULER_COMMON_HPP_

#include <cstring>
#include <set>
#include <tr1/unordered_set>
#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "idiom/iroot.h"
#include "idiom/history.h"

namespace idiom {

class SchedulerCommon;

// a set of thread that have been delayed
typedef std::set<thread_id_t> DelaySet;

#define IDIOM1_STATE_INVALID          0
#define IDIOM1_STATE_INIT             1
#define IDIOM1_STATE_E0               2
#define IDIOM1_STATE_E1               3
#define IDIOM1_STATE_E0_E1            4
#define IDIOM1_STATE_E0_WATCH         5
#define IDIOM1_STATE_E0_WATCH_X       6
#define IDIOM1_STATE_DONE             7

class Idiom1SchedStatus {
 public:
  Idiom1SchedStatus();
  ~Idiom1SchedStatus() {}

  static std::string StateToString(long s) {
    switch (s) {
      case IDIOM1_STATE_INIT:
        return "INIT";
      case IDIOM1_STATE_E0:
        return "E0";
      case IDIOM1_STATE_E1:
        return "E1";
      case IDIOM1_STATE_E0_E1:
        return "E0_E1";
      case IDIOM1_STATE_E0_WATCH:
        return "E0_WATCH";
      case IDIOM1_STATE_E0_WATCH_X:
        return "E0_WATCH_X";
      case IDIOM1_STATE_DONE:
        return "DONE";
      default:
        return "INVALID";
    }
  }

 private:
  unsigned long state_;
  thread_id_t thd_id_[2];
  address_t addr_[2];
  size_t size_[2];
  DelaySet delay_set_;

  // pankit
  map <thread_id_t, long> num_mem_acc;

  friend class SchedulerCommon;

  DISALLOW_COPY_CONSTRUCTORS(Idiom1SchedStatus);
};

#define IDIOM2_STATE_INVALID           0
#define IDIOM2_STATE_INIT              1
#define IDIOM2_STATE_E0                2
#define IDIOM2_STATE_E1                3
#define IDIOM2_STATE_E0_E1             4
#define IDIOM2_STATE_E0_E1_WATCH       5
#define IDIOM2_STATE_E0_E1_WATCH_X     6
#define IDIOM2_STATE_E0_WATCH          7
#define IDIOM2_STATE_E0_WATCH_X        8
#define IDIOM2_STATE_E1_WATCH          9
#define IDIOM2_STATE_E1_WATCH_X        10
#define IDIOM2_STATE_E0_E1_E2          11
#define IDIOM2_STATE_DONE              12

class Idiom2SchedStatus {
 public:
  Idiom2SchedStatus();
  ~Idiom2SchedStatus() {}

  static std::string StateToString(unsigned long s) {
    switch (s) {
      case IDIOM2_STATE_INIT:
        return "INIT";
      case IDIOM2_STATE_E0:
        return "E0";
      case IDIOM2_STATE_E1:
        return "E1";
      case IDIOM2_STATE_E0_E1:
        return "E0_E1";
      case IDIOM2_STATE_E0_E1_WATCH:
        return "E0_E1_WATCH";
      case IDIOM2_STATE_E0_E1_WATCH_X:
        return "E0_E1_WATCH_X";
      case IDIOM2_STATE_E0_WATCH:
        return "E0_WATCH";
      case IDIOM2_STATE_E0_WATCH_X:
        return "E0_WATCH_X";
      case IDIOM2_STATE_E1_WATCH:
        return "E1_WATCH";
      case IDIOM2_STATE_E1_WATCH_X:
        return "E1_WATCH_X";
      case IDIOM2_STATE_E0_E1_E2:
        return "E0_E1_E2";
      case IDIOM2_STATE_DONE:
        return "DONE";
      default:
        return "INVALID";
    }
  }

 private:
  unsigned long state_;
  thread_id_t thd_id_[3];
  address_t addr_[3];
  size_t size_[3];
  timestamp_t window_;
  DelaySet delay_set_;

  friend class SchedulerCommon;

  DISALLOW_COPY_CONSTRUCTORS(Idiom2SchedStatus);
};

#define IDIOM3_STATE_INVALID              0
#define IDIOM3_STATE_INIT                 1
#define IDIOM3_STATE_E0                   2
#define IDIOM3_STATE_E1                   3
#define IDIOM3_STATE_E0_WATCH             4
#define IDIOM3_STATE_E0_WATCH_X           5
#define IDIOM3_STATE_E0_E1                6
#define IDIOM3_STATE_E0_E1_WATCH          7
#define IDIOM3_STATE_E1_WATCH             8
#define IDIOM3_STATE_E1_WATCH_X           9
#define IDIOM3_STATE_E0_WATCH_E3          10
#define IDIOM3_STATE_E1_WATCH_E3          11
#define IDIOM3_STATE_E1_WATCH_E3_X        12
#define IDIOM3_STATE_E1_WATCH_E2          13
#define IDIOM3_STATE_E1_WATCH_E2_WATCH    14
#define IDIOM3_STATE_E1_WATCH_E2_WATCH_X  15
#define IDIOM3_STATE_E0_E1_E2_E3          16
#define IDIOM3_STATE_DONE                 17

class Idiom3SchedStatus {
 public:
  Idiom3SchedStatus();
  ~Idiom3SchedStatus() {}

  static std::string StateToString(unsigned long s) {
    switch (s) {
      case IDIOM3_STATE_INIT:
        return "INIT";
      case IDIOM3_STATE_E0:
        return "E0";
      case IDIOM3_STATE_E1:
        return "E1";
      case IDIOM3_STATE_E0_WATCH:
        return "E0_WATCH";
      case IDIOM3_STATE_E0_WATCH_X:
        return "E0_WATCH_X";
      case IDIOM3_STATE_E0_E1:
        return "E0_E1";
      case IDIOM3_STATE_E0_E1_WATCH:
        return "E0_E1_WATCH";
      case IDIOM3_STATE_E1_WATCH:
        return "E1_WATCH";
      case IDIOM3_STATE_E1_WATCH_X:
        return "E1_WATCH_X";
      case IDIOM3_STATE_E0_WATCH_E3:
        return "E0_WATCH_E3";
      case IDIOM3_STATE_E1_WATCH_E3:
        return "E1_WATCH_E3";
      case IDIOM3_STATE_E1_WATCH_E3_X:
        return "E1_WATCH_E3_X";
      case IDIOM3_STATE_E1_WATCH_E2:
        return "E1_WATCH_E2";
      case IDIOM3_STATE_E1_WATCH_E2_WATCH:
        return "E1_WATCH_E2_WATCH";
      case IDIOM3_STATE_E1_WATCH_E2_WATCH_X:
        return "E1_WATCH_E2_WATCH_X";
      case IDIOM3_STATE_E0_E1_E2_E3:
        return "E0_E1_E2_E3";
      case IDIOM3_STATE_DONE:
        return "DONE";
      default:
        return "INVALID";
    }
  }

 private:
  unsigned long state_;
  thread_id_t thd_id_[4];
  address_t addr_[4];
  size_t size_[4];
  timestamp_t window_;
  DelaySet delay_set_;

  friend class SchedulerCommon;

  DISALLOW_COPY_CONSTRUCTORS(Idiom3SchedStatus);
};

#define IDIOM4_STATE_INVALID              0
#define IDIOM4_STATE_INIT                 1
#define IDIOM4_STATE_E0                   2
#define IDIOM4_STATE_E1                   3
#define IDIOM4_STATE_E0_WATCH             4
#define IDIOM4_STATE_E0_WATCH_X           5
#define IDIOM4_STATE_E0_E1                6
#define IDIOM4_STATE_E0_E1_WATCH          7
#define IDIOM4_STATE_E1_WATCH             8
#define IDIOM4_STATE_E1_WATCH_X           9
#define IDIOM4_STATE_E0_WATCH_E3          10
#define IDIOM4_STATE_E1_WATCH_E3          11
#define IDIOM4_STATE_E1_WATCH_E3_X        12
#define IDIOM4_STATE_E1_WATCH_E2          13
#define IDIOM4_STATE_E1_WATCH_E2_WATCH    14
#define IDIOM4_STATE_E1_WATCH_E2_WATCH_X  15
#define IDIOM4_STATE_E0_E1_E2_E3          16
#define IDIOM4_STATE_DONE                 17

class Idiom4SchedStatus {
 public:
  Idiom4SchedStatus();
  ~Idiom4SchedStatus() {}

  static std::string StateToString(unsigned long s) {
    switch (s) {
      case IDIOM4_STATE_INIT:
        return "INIT";
      case IDIOM4_STATE_E0:
        return "E0";
      case IDIOM4_STATE_E1:
        return "E1";
      case IDIOM4_STATE_E0_WATCH:
        return "E0_WATCH";
      case IDIOM4_STATE_E0_WATCH_X:
        return "E0_WATCH_X";
      case IDIOM4_STATE_E0_E1:
        return "E0_E1";
      case IDIOM4_STATE_E0_E1_WATCH:
        return "E0_E1_WATCH";
      case IDIOM4_STATE_E1_WATCH:
        return "E1_WATCH";
      case IDIOM4_STATE_E1_WATCH_X:
        return "E1_WATCH_X";
      case IDIOM4_STATE_E0_WATCH_E3:
        return "E0_WATCH_E3";
      case IDIOM4_STATE_E1_WATCH_E3:
        return "E1_WATCH_E3";
      case IDIOM4_STATE_E1_WATCH_E3_X:
        return "E1_WATCH_E3_X";
      case IDIOM4_STATE_E1_WATCH_E2:
        return "E1_WATCH_E2";
      case IDIOM4_STATE_E1_WATCH_E2_WATCH:
        return "E1_WATCH_E2_WATCH";
      case IDIOM4_STATE_E1_WATCH_E2_WATCH_X:
        return "E1_WATCH_E2_WATCH_X";
      case IDIOM4_STATE_E0_E1_E2_E3:
        return "E0_E1_E2_E3";
      case IDIOM4_STATE_DONE:
        return "DONE";
      default:
        return "INVALID";
    }
  }

 private:
  typedef std::tr1::unordered_set<address_t> RecordedAddrSet;

  unsigned long state_;
  thread_id_t thd_id_[4];
  address_t addr_[4];
  size_t size_[4];
  timestamp_t window_;
  DelaySet delay_set_;
  RecordedAddrSet recorded_addr_set_;

  friend class SchedulerCommon;

  DISALLOW_COPY_CONSTRUCTORS(Idiom4SchedStatus);
};

#define IDIOM5_STATE_INVALID                0
#define IDIOM5_STATE_INIT                   1
#define IDIOM5_STATE_E0                     2
#define IDIOM5_STATE_E2                     3
#define IDIOM5_STATE_E0_WATCH               4
#define IDIOM5_STATE_E0_WATCH_X             5
#define IDIOM5_STATE_E2_WATCH               6
#define IDIOM5_STATE_E2_WATCH_X             7
#define IDIOM5_STATE_E0_E2                  8
#define IDIOM5_STATE_E0_E2_WATCH            9
#define IDIOM5_STATE_E0_E2_WATCH_X          10
#define IDIOM5_STATE_E0_WATCH_E3            11
#define IDIOM5_STATE_E2_WATCH_E1            12
#define IDIOM5_STATE_E0_E2_WATCH_E3         13
#define IDIOM5_STATE_E0_E2_WATCH_E3_X       14
#define IDIOM5_STATE_E0_E2_WATCH_E1         15
#define IDIOM5_STATE_E0_E2_WATCH_E1_X       16
#define IDIOM5_STATE_E0_E2_WATCH_E3_WATCH   17
#define IDIOM5_STATE_E0_E2_WATCH_E3_WATCH_X 18
#define IDIOM5_STATE_E0_E2_WATCH_E1_WATCH   19
#define IDIOM5_STATE_E0_E2_WATCH_E1_WATCH_X 20
#define IDIOM5_STATE_E0_E1_E2_E3            21
#define IDIOM5_STATE_DONE                   22

class Idiom5SchedStatus {
 public:
  Idiom5SchedStatus();
  ~Idiom5SchedStatus();

  static std::string StateToString(unsigned long s) {
    switch (s) {
      case IDIOM5_STATE_INVALID:
        return "INVALID";
      case IDIOM5_STATE_INIT:
        return "INIT";
      case IDIOM5_STATE_E0:
        return "E0";
      case IDIOM5_STATE_E2:
        return "E2";
      case IDIOM5_STATE_E0_WATCH:
        return "E0_WATCH";
      case IDIOM5_STATE_E0_WATCH_X:
        return "E0_WATCH_X";
      case IDIOM5_STATE_E2_WATCH:
        return "E2_WATCH";
      case IDIOM5_STATE_E2_WATCH_X:
        return "E2_WATCH_X";
      case IDIOM5_STATE_E0_E2:
        return "E0_E2";
      case IDIOM5_STATE_E0_E2_WATCH:
        return "E0_E2_WATCH";
      case IDIOM5_STATE_E0_E2_WATCH_X:
        return "E0_E2_WATCH_X";
      case IDIOM5_STATE_E0_WATCH_E3:
        return "E0_WATCH_E3";
      case IDIOM5_STATE_E2_WATCH_E1:
        return "E2_WATCH_E1";
      case IDIOM5_STATE_E0_E2_WATCH_E3:
        return "E0_E2_WATCH_E3";
      case IDIOM5_STATE_E0_E2_WATCH_E3_X:
        return "E0_E2_WATCH_E3_X";
      case IDIOM5_STATE_E0_E2_WATCH_E1:
        return "E0_E2_WATCH_E1";
      case IDIOM5_STATE_E0_E2_WATCH_E1_X:
        return "E0_E2_WATCH_E1_X";
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH:
        return "E0_E2_WATCH_E3_WATCH";
      case IDIOM5_STATE_E0_E2_WATCH_E3_WATCH_X:
        return "E0_E2_WATCH_E3_WATCH_X";
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH:
        return "E0_E2_WATCH_E1_WATCH";
      case IDIOM5_STATE_E0_E2_WATCH_E1_WATCH_X:
        return "E0_E2_WATCH_E1_WATCH_X";
      case IDIOM5_STATE_E0_E1_E2_E3:
        return "E0_E1_E2_E3";
      case IDIOM5_STATE_DONE:
        return "DONE";
      default:
        return "INVALID";
    }
  }

 private:
  typedef std::tr1::unordered_set<address_t> RecordedAddrSet;

  unsigned long state_;
  thread_id_t thd_id_[4];
  address_t addr_[4];
  size_t size_[4];
  timestamp_t window_[2];
  DelaySet delay_set_;
  RecordedAddrSet recorded_addr_set0_;
  RecordedAddrSet recorded_addr_set2_;

  friend class SchedulerCommon;

  DISALLOW_COPY_CONSTRUCTORS(Idiom5SchedStatus);
};

// The controller for the idiom driven active scheduler.
class SchedulerCommon : public ExecutionControl {
 public:
  SchedulerCommon();
  virtual ~SchedulerCommon() {}

 protected:
  Mutex *CreateMutex() { return new PinMutex; }
  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandlePreInstrumentTrace(TRACE trace);
  virtual void HandleImageLoad(IMG img, Image *image);
  virtual void HandleSyscallEntry(THREADID tid, CONTEXT *ctxt,
                                  SYSCALL_STANDARD std);
  virtual void HandleProgramStart();
  virtual void HandleProgramExit();
  virtual void HandleThreadStart();
  virtual void HandleThreadExit();
  virtual void HandleMain(THREADID tid, CONTEXT *ctxt);
  virtual void HandleThreadMain(THREADID tid, CONTEXT *ctxt);

  // virtual functions to be overrided
  virtual void Choose();
  virtual void TestSuccess() {}
  virtual void TestFail() {}
  virtual bool UseDecreasingPriorities();
  virtual bool YieldWithDelay();

  // instrument iRoots
  void InstrumentMemiRootEvent(TRACE trace);
  void InstrumentMemiRootEvent(TRACE trace, UINT32 idx);
  void ReplacePthreadMutexWrappers(IMG img);
  void CheckiRootBeforeMutexLock(Inst *inst, address_t addr);
  void CheckiRootAfterMutexLock(Inst *inst, address_t addr);
  void CheckiRootBeforeMutexUnlock(Inst *inst, address_t addr);
  void CheckiRootAfterMutexUnlock(Inst *inst, address_t addr);

  // instrument to watch memeory accesses and maintain inst count
  void InstrumentWatchMem(TRACE trace);
  void InstrumentWatchInstCount(TRACE trace);
  void Idiom1InstrumentWatchMem(TRACE trace);
  void Idiom1InstrumentWatchInstCount(TRACE trace);
  void Idiom2InstrumentWatchMem(TRACE trace);
  void Idiom2InstrumentWatchInstCount(TRACE trace);
  void Idiom3InstrumentWatchMem(TRACE trace);
  void Idiom3InstrumentWatchInstCount(TRACE trace);
  void Idiom4InstrumentWatchMem(TRACE trace);
  void Idiom4InstrumentWatchInstCount(TRACE trace);
  void Idiom5InstrumentWatchMem(TRACE trace);
  void Idiom5InstrumentWatchInstCount(TRACE trace);
  void __InstrumentWatchInstCount(TRACE trace);
  void __InstrumentWatchMem(TRACE trace, bool cand);
  bool ContainCandidates(TRACE trace);
  bool IsCandidate(TRACE trace, INS ins);
  void FlushWatch();

  // utility functions
  void LockSchedStatus() { sched_status_lock_->Lock(); }
  void UnlockSchedStatus() { sched_status_lock_->Unlock(); }
  void LockMisc() { misc_lock_->Lock(); }
  void UnlockMisc() { misc_lock_->Unlock(); }
  void ActivelyExposed();
  Inst *FindInst(ADDRINT pc);
  void CalculatePriorities();
  int NormalPriority() { return normal_priority_; }
  int LowerPriority() { return lower_priority_; }
  int HigherPriority() { return higher_priority_; }
  int MinPriority() { return min_priority_; }
  int MaxPriority() { return max_priority_; }
  int MainThreadPriority();
  int NextNewThreadPriority();
  void InitNewThreadPriority();
  bool RandomChoice(double true_rate);
  int GetPriority(thread_id_t target);
  void SetPriority(int priority);
  void SetPriority(thread_id_t target, int priority);
  void SetPriorityNormal(thread_id_t target);
  void SetPriorityLow(thread_id_t target);
  void SetPriorityHigh(thread_id_t target);
  void SetPriorityMin(thread_id_t target);
  void SetPriorityMax(thread_id_t target);
  void SetStrictPriority(int priority);
  void SetRelaxPriority(int priority);
  void SetStrictPriority(thread_id_t target, int priority);
  void SetRelaxPriority(thread_id_t target, int priority);
  void SetAffinity();
  void RecordTargetiRoot(iRoot *iroot);
  void RecordRandomSeed(unsigned int seed);

  // iRoot handler
  void HandleBeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void HandleBeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void HandleAfteriRootMem(UINT32 idx);
  void HandleBeforeiRootMutexLock(UINT32 idx, address_t addr);
  void HandleAfteriRootMutexLock(UINT32 idx, address_t addr);
  void HandleBeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void HandleAfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void HandleWatchMutexLock(address_t addr);
  void HandleWatchMutexUnlock(address_t addr);
  void HandleWatchMemRead(Inst *inst, address_t addr, size_t size,
                          bool cand);
  void HandleWatchMemWrite(Inst *inst, address_t addr, size_t size,
                           bool cand);
  void HandleWatchInstCount(timestamp_t c);
  void HandleSchedYield();

  // idiom1 iRoot handler
  void Idiom1BeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void Idiom1BeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void Idiom1AfteriRootMem(UINT32 idx);
  void Idiom1BeforeiRootMutexLock(UINT32 idx, address_t addr);
  void Idiom1AfteriRootMutexLock(UINT32 idx, address_t addr);
  void Idiom1BeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom1AfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom1BeforeEvent0(address_t addr, size_t size);
  void Idiom1BeforeEvent1(address_t addr, size_t size);
  void Idiom1AfterEvent0();
  void Idiom1AfterEvent1();
  void Idiom1WatchAccess(address_t addr, size_t size);
  void Idiom1WatchMutexLock(address_t addr);
  void Idiom1WatchMutexUnlock(address_t addr);
  void Idiom1WatchMemRead(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom1WatchMemWrite(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom1SchedYield();
  void Idiom1CheckFlush();
  bool Idiom1CheckGiveup(int idx);
  void Idiom1SetState(unsigned long s);
  void Idiom1ClearDelaySet(DelaySet *copy);
  void Idiom1WakeDelaySet(DelaySet *copy);

  // idiom2 iRoot handler
  void Idiom2BeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void Idiom2BeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void Idiom2AfteriRootMem(UINT32 idx);
  void Idiom2BeforeiRootMutexLock(UINT32 idx, address_t addr);
  void Idiom2AfteriRootMutexLock(UINT32 idx, address_t addr);
  void Idiom2BeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom2AfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom2BeforeEvent0(address_t addr, size_t size);
  void Idiom2BeforeEvent1(address_t addr, size_t size);
  void Idiom2BeforeEvent2(address_t addr, size_t size);
  void Idiom2AfterEvent0();
  void Idiom2AfterEvent1();
  void Idiom2AfterEvent2();
  void Idiom2WatchAccess(address_t addr, size_t size);
  void Idiom2WatchInstCount(timestamp_t c);
  void Idiom2WatchMutexLock(address_t addr);
  void Idiom2WatchMutexUnlock(address_t addr);
  void Idiom2WatchMemRead(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom2WatchMemWrite(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom2CheckFlush();
  bool Idiom2CheckGiveup(int idx);
  void Idiom2SetState(unsigned long s);
  void Idiom2ClearDelaySet(DelaySet *copy);
  void Idiom2WakeDelaySet(DelaySet *copy);

  // idiom3 iRoot handler
  void Idiom3BeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void Idiom3BeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void Idiom3AfteriRootMem(UINT32 idx);
  void Idiom3BeforeiRootMutexLock(UINT32 idx, address_t addr);
  void Idiom3AfteriRootMutexLock(UINT32 idx, address_t addr);
  void Idiom3BeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom3AfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom3BeforeEvent0(address_t addr, size_t size);
  void Idiom3BeforeEvent1(address_t addr, size_t size);
  void Idiom3BeforeEvent2(address_t addr, size_t size);
  void Idiom3BeforeEvent3(address_t addr, size_t size);
  void Idiom3AfterEvent0();
  void Idiom3AfterEvent1();
  void Idiom3AfterEvent2();
  void Idiom3AfterEvent3();
  void Idiom3WatchAccess(address_t addr, size_t size);
  void Idiom3WatchInstCount(timestamp_t c);
  void Idiom3WatchMutexLock(address_t addr);
  void Idiom3WatchMutexUnlock(address_t addr);
  void Idiom3WatchMemRead(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom3WatchMemWrite(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom3CheckFlush();
  bool Idiom3CheckGiveup(int idx);
  void Idiom3SetState(unsigned long s);
  void Idiom3ClearDelaySet(DelaySet *copy);
  void Idiom3WakeDelaySet(DelaySet *copy);

  // idiom4 iRoot handler
  void Idiom4BeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void Idiom4BeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void Idiom4AfteriRootMem(UINT32 idx);
  void Idiom4BeforeiRootMutexLock(UINT32 idx, address_t addr);
  void Idiom4AfteriRootMutexLock(UINT32 idx, address_t addr);
  void Idiom4BeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom4AfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom4BeforeEvent0(address_t addr, size_t size);
  void Idiom4BeforeEvent1(address_t addr, size_t size);
  void Idiom4BeforeEvent2(address_t addr, size_t size);
  void Idiom4BeforeEvent3(address_t addr, size_t size);
  void Idiom4AfterEvent0();
  void Idiom4AfterEvent1();
  void Idiom4AfterEvent2();
  void Idiom4AfterEvent3();
  void Idiom4WatchAccess(address_t addr, size_t size);
  void Idiom4WatchInstCount(timestamp_t c);
  void Idiom4WatchMutexLock(address_t addr);
  void Idiom4WatchMutexUnlock(address_t addr);
  void Idiom4WatchMemRead(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom4WatchMemWrite(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom4CheckFlush();
  bool Idiom4CheckGiveup(int idx);
  void Idiom4SetState(unsigned long s);
  void Idiom4ClearDelaySet(DelaySet *copy);
  void Idiom4WakeDelaySet(DelaySet *copy);
  void Idiom4RecordAccess(address_t addr, size_t size);
  void Idiom4ClearRecordedAccess();
  bool Idiom4Recorded(address_t addr, size_t size);

  // idiom5 iRoot handler
  void Idiom5BeforeiRootMemRead(UINT32 idx, address_t addr, size_t size);
  void Idiom5BeforeiRootMemWrite(UINT32 idx, address_t addr, size_t size);
  void Idiom5AfteriRootMem(UINT32 idx);
  void Idiom5BeforeiRootMutexLock(UINT32 idx, address_t addr);
  void Idiom5AfteriRootMutexLock(UINT32 idx, address_t addr);
  void Idiom5BeforeiRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom5AfteriRootMutexUnlock(UINT32 idx, address_t addr);
  void Idiom5BeforeEvent0(address_t addr, size_t size);
  void Idiom5BeforeEvent1(address_t addr, size_t size);
  void Idiom5BeforeEvent2(address_t addr, size_t size);
  void Idiom5BeforeEvent3(address_t addr, size_t size);
  void Idiom5AfterEvent0();
  void Idiom5AfterEvent1();
  void Idiom5AfterEvent2();
  void Idiom5AfterEvent3();
  void Idiom5WatchAccess(address_t addr, size_t size);
  void Idiom5WatchInstCount(timestamp_t c);
  void Idiom5WatchMutexLock(address_t addr);
  void Idiom5WatchMutexUnlock(address_t addr);
  void Idiom5WatchMemRead(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom5WatchMemWrite(Inst *inst, address_t addr, size_t size, bool cand);
  void Idiom5CheckFlush();
  bool Idiom5CheckGiveup(int idx);
  void Idiom5SetState(unsigned long s);
  void Idiom5ClearDelaySet(DelaySet *copy);
  void Idiom5WakeDelaySet(DelaySet *copy);
  void Idiom5RecordAccess(int idx, address_t addr, size_t size);
  void Idiom5ClearRecordedAccess(int idx);
  bool Idiom5Recorded(int idx, address_t addr, size_t size);

  static void __BeforeiRootMemRead(UINT32 idx, ADDRINT addr, UINT32 size);
  static void __BeforeiRootMemWrite(UINT32 idx, ADDRINT addr, UINT32 size);
  static void __AfteriRootMem(UINT32 idx);
  static void __WatchMemRead(Inst *inst, ADDRINT addr, UINT32 size,
                             BOOL cand);
  static void __WatchMemWrite(Inst *inst, ADDRINT addr, UINT32 size,
                              BOOL cand);
  static void __WatchInstCount(UINT32 c);

  iRootDB *iroot_db_;
  TestHistory *history_;
  unsigned int random_seed_;
  int normal_priority_;
  int lower_priority_;
  int higher_priority_;
  int min_priority_;
  int max_priority_;
  std::vector<int> new_thread_priorities_;
  int new_thread_priorities_cursor_;
  address_t unit_size_;
  timestamp_t vw_;
  iRoot *curr_iroot_;
  Idiom1SchedStatus *idiom1_sched_status_;
  Idiom2SchedStatus *idiom2_sched_status_;
  Idiom3SchedStatus *idiom3_sched_status_;
  Idiom4SchedStatus *idiom4_sched_status_;
  Idiom5SchedStatus *idiom5_sched_status_;
  Mutex *sched_status_lock_;
  Mutex *misc_lock_;
  std::map<thread_id_t, int> priority_map_;
  std::map<thread_id_t, int> ori_priority_map_;
  std::map<thread_id_t, OS_THREAD_ID> thd_id_os_tid_map_;

  bool volatile start_schedule_; // start scheduling when 2 threads are started
  bool volatile test_success_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(SchedulerCommon);

  // Override wrappers.
  DECLARE_MEMBER_WRAPPER_HANDLER(PthreadMutexLock);
  DECLARE_MEMBER_WRAPPER_HANDLER(PthreadMutexUnlock);
};

} // namespace idiom

#endif


