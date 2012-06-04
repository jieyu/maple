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

// File: pct/scheduler.hpp - Define the PCT scheduler controller.

#ifndef PCT_SCHEDULER_HPP_
#define PCT_SCHEDULER_HPP_

#include <vector>

#include "core/basictypes.h"
#include "core/atomic.h"
#include "core/execution_control.hpp"
#include "pct/history.h"

namespace pct {

// PCT scheduler controller.
class Scheduler : public ExecutionControl {
 public:
  Scheduler();
  virtual ~Scheduler();

 protected:
  virtual Mutex *CreateMutex() { return new PinMutex; }
  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandlePostInstrumentTrace(TRACE trace);
  virtual void HandleSyscallEntry(THREADID tid, CONTEXT *ctxt,
                                  SYSCALL_STANDARD std);
  virtual void HandleProgramExit();
  virtual void HandleThreadStart();
  virtual void HandleThreadExit();
  void HandlePriorityChange(UINT32 c);

  bool NeedPriorityChange(unsigned long k);
  int NextNewThreadPriority();
  int NextChangePriority();
  void Randomize();
  void SetPriority(int priority);
  void SetStrictPriority(int priority);
  void SetRelaxPriority(int priority);
  void SetAffinity();

  History *history_;
  int depth_;
  bool count_mem_;
  std::vector<unsigned long> priority_change_points_;
  std::vector<int> new_thread_priorities_;
  std::vector<int> change_priorities_;
  int change_points_cursor_;
  int change_priorities_cursor_;
  int new_thread_priorities_cursor_;
  unsigned long total_inst_count_;
  unsigned long total_num_threads_;
  int curr_num_threads_;
  volatile bool start_inst_count_; // start counting inst when at least
                                   // 2 threads are started

 private:
  static void __PriorityChange(UINT32 c);
  static void __Main();
  static void __ThreadMain();

  DISALLOW_COPY_CONSTRUCTORS(Scheduler);
};

} // namespace pct

#endif


