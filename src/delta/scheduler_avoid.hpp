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

// File: delta/scheduler.hpp - Define the reorder scheduler controller.

#ifndef DELTA_SCHEDULER_AVOID_HPP_
#define DELTA_SCHEDULER_AVOID_HPP_

#include "core/basictypes.h"
#include "core/execution_control.hpp"
#include "idiom/iroot.h"
#include "ilist.h"
#include "observer.h"
#include "slist.h"

namespace delta {

class Scheduler : public ExecutionControl {
	
 public: 
  Scheduler();
  virtual ~Scheduler() {}

 private:
  Mutex *CreateMutex() { return new SemMutex; }
  void HandlePreSetup();
  void HandlePostSetup();
  bool HandleIgnoreMemAccess(IMG img);
  void HandlePreInstrumentTrace(TRACE trace);
  void HandleProgramExit();
  void HandleThreadStart();
  void HandleThreadExit();
  void HandleMain(THREADID tid, CONTEXT *ctxt);
  void HandleThreadMain(THREADID tid, CONTEXT *ctxt);
  void HandlePriorityBeforeChange(THREADID tid, Inst *inst);
  void HandlePriorityAfterChange(THREADID tid, Inst *inst);
  
  int NextNewThreadPriority();

  int NormalPriority() { return normal_priority_; }
  int HighesetPriority() {return highest_priority_;}
  int HigherPriority() { return higher_priority_; }
  int LowerPriority() { return lower_priority_; }
  int LowestPriority() {return lowest_priority_; }
  void Randomize();
  void SetPriority(int priority);
  void SetAffinity();

  static void __PriorityBeforeChange(THREADID tid, Inst *inst);
  static void __PriorityAfterChange(THREADID tid, Inst *inst);

  int depth_;
  bool count_mem_;
  std::vector<int> new_thread_priorities_;
  int new_thread_priorities_cursor_;
  volatile bool start_schedule_;  //start scheduling when 2 threads are started

  std::vector< inst_id_type > suspect_;
  idiom::iRoot *curr_iroot_;
  uint32 fence_inst;
  
  int highest_priority_;
  int higher_priority_;
  int normal_priority_;
  int lower_priority_;
  int lowest_priority_;
  Observer *observer_;
  idiom::iRootDB *iroot_db_;
  iList *ilist_;  
  SuspectList *slist_;
  
  DISALLOW_COPY_CONSTRUCTORS(Scheduler);
};

} //namespace

#endif
