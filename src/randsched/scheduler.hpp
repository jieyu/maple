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

// File: randsched/scheduler.hpp - Define the Random scheduler controller.

#ifndef RANDSCHED_SCHEDULER_HPP_
#define RANDSCHED_SCHEDULER_HPP_

#include <vector>

#include "core/basictypes.h"
#include "core/atomic.h"
#include "core/execution_control.hpp"
#include "randsched/history.h"

namespace randsched {

// Random scheduler controller.
class Scheduler : public ExecutionControl {
 public:
  Scheduler();
  virtual ~Scheduler();

 protected:
  virtual Mutex *CreateMutex() { return new PinMutex; }
  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandlePostInstrumentTrace(TRACE trace);
  virtual void HandleProgramExit();
  virtual void HandleThreadStart();
  virtual void HandleThreadExit();
  void HandleChange(UINT32 c);

  bool NeedChange(unsigned long k);
  int RandomPriority();
  void RandomDelay();
  void Randomize();
  void SetPriority(int priority);
  void SetStrictPriority(int priority);
  void SetRelaxPriority(int priority);
  void SetAffinity();

  History *history_;
  bool delay_; // delay mode or not
  bool float_; // whether # of change points depends on execution length
  std::vector<int> prio_vec_;
  std::vector<unsigned long> chg_pts_vec_;
  int chg_pts_cursor_;
  int delay_unit_; // in microseconds
  unsigned long total_inst_count_;
  unsigned long total_num_threads_;
  int curr_num_threads_;
  volatile bool start_sched_; // start scheduling when at least two
                              // threads are created

 private:
  static void __Change(UINT32 c);

  DISALLOW_COPY_CONSTRUCTORS(Scheduler);
};

} // namespace randsched

#endif


