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

// File: systematic/controller.cpp - The implementation of the execution
// controller for systematic concurrency testing.

#include "systematic/controller.hpp"

#include <sys/resource.h>
#include <cassert>
#include <cerrno>

namespace systematic {

Controller::CreationInfo::hash_val_t Controller::CreationInfo::Hash() {
  hash_val_t hash_val = 0;
  hash_val += (hash_val_t)creator_thd_id;
  hash_val += (hash_val_t)creator_inst->id();
  return hash_val;
}

bool Controller::CreationInfo::Match(CreationInfo *info) {
  return creator_thd_id == info->creator_thd_id &&
         creator_inst->id() == info->creator_inst->id();
}

Controller::Controller()
    : scheduler_(NULL),
      program_(NULL),
      execution_(NULL),
      race_db_(NULL),
      unit_size_(4),
      scheduler_thd_uid_(INVALID_PIN_THREAD_UID),
      program_exiting_(false),
      next_state_ready_(false),
      next_state_sem_(NULL) {
  // empty
}

Controller::~Controller() {
  // empty
}

void Controller::HandlePreSetup() {
  ExecutionControl::HandlePreSetup();

  knob_->RegisterBool("sched_app", "whether only schedule operations from the application", "1");
  knob_->RegisterBool("sched_race", "whether schedule racy memory operations (for racy programs)", "0");
  knob_->RegisterInt("cpu", "specify which cpu to run on", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("realtime_priority", "the realtime priority on which all the user thread should be run", "1");
  knob_->RegisterStr("program_in", "the input database for the modeled program", "program.db");
  knob_->RegisterStr("program_out", "the output database for the modeled program", "program.db");
  knob_->RegisterStr("race_in", "the input race database path", "race.db");
  knob_->RegisterStr("race_out", "the output race database path", "race.db");

  random_scheduler_ = new RandomScheduler(this);
  random_scheduler_->Register();
  chess_scheduler_ = new ChessScheduler(this);
  chess_scheduler_->Register();
}

void Controller::HandlePostSetup() {
  ExecutionControl::HandlePostSetup();

  // read settings and flags
  sched_app_ = knob_->ValueBool("sched_app");
  sched_race_ = knob_->ValueBool("sched_race");
  unit_size_ = knob_->ValueInt("unit_size");

  // init global states
  program_ = new Program;
  program_->Load(knob_->ValueStr("program_in"), sinfo_);
  execution_ = new Execution;
  if (sched_race_) {
    race_db_ = new race::RaceDB(CreateMutex());
    race_db_->Load(knob_->ValueStr("race_in"), sinfo_);
  }
  next_state_sem_ = CreateSemaphore(0);

  // init the scheduler
  if (random_scheduler_->Enabled())
    SetScheduler(random_scheduler_);
  if (chess_scheduler_->Enabled())
    SetScheduler(chess_scheduler_);

  // make sure that we use one scheduler
  if (!scheduler_)
    Abort("please choose a scheduler\n");

  // setup instrumentation
  desc_.SetHookPthreadFunc();
  desc_.SetHookMallocFunc();
}

void Controller::HandlePreInstrumentTrace(TRACE trace) {
  ExecutionControl::HandlePreInstrumentTrace(trace);

  if (sched_race_) {
    // instrument every racy memory operation
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
      for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
        // skip stack access
        if (INS_IsStackRead(ins) || INS_IsStackWrite(ins))
          continue;

        // only track memory access instructions
        if (INS_IsMemoryRead(ins) || INS_IsMemoryWrite(ins)) {
          // get instruction
          Inst *inst = GetInst(INS_Address(ins));
          UpdateInstOpcode(inst, ins);

          // check whether inst is racy
          if (race_db_->RacyInst(inst, false)) {
            // instrument inst (before)
            if (INS_IsMemoryRead(ins)) {
              INS_InsertCall(ins, IPOINT_BEFORE,
                             (AFUNPTR)__BeforeRaceRead,
                             IARG_THREAD_ID,
                             IARG_PTR, inst,
                             IARG_MEMORYREAD_EA,
                             IARG_MEMORYREAD_SIZE,
                             IARG_END);
            }

            if (INS_IsMemoryWrite(ins)) {
              INS_InsertCall(ins, IPOINT_BEFORE,
                             (AFUNPTR)__BeforeRaceWrite,
                             IARG_THREAD_ID,
                             IARG_PTR, inst,
                             IARG_MEMORYWRITE_EA,
                             IARG_MEMORYWRITE_SIZE,
                             IARG_END);
            }

            if (INS_HasMemoryRead2(ins)) {
              INS_InsertCall(ins, IPOINT_BEFORE,
                             (AFUNPTR)__BeforeRaceRead2,
                             IARG_THREAD_ID,
                             IARG_PTR, inst,
                             IARG_MEMORYREAD2_EA,
                             IARG_MEMORYREAD_SIZE,
                             IARG_END);
            }

            // instrument inst (after)
            if (INS_IsMemoryRead(ins)) {
              if (INS_HasFallThrough(ins)) {
                INS_InsertCall(ins, IPOINT_AFTER,
                               (AFUNPTR)__AfterRaceRead,
                               IARG_THREAD_ID,
                               IARG_PTR, inst,
                               IARG_END);
              }

              if (INS_IsBranchOrCall(ins)) {
                INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
                               (AFUNPTR)__AfterRaceRead,
                               IARG_THREAD_ID,
                               IARG_PTR, inst,
                               IARG_END);
              }
            }

            if (INS_IsMemoryWrite(ins)) {
              if (INS_HasFallThrough(ins)) {
                INS_InsertCall(ins, IPOINT_AFTER,
                             (AFUNPTR)__AfterRaceWrite,
                             IARG_THREAD_ID,
                             IARG_PTR, inst,
                             IARG_END);
              }

              if (INS_IsBranchOrCall(ins)) {
                INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
                             (AFUNPTR)__AfterRaceWrite,
                             IARG_THREAD_ID,
                             IARG_PTR, inst,
                             IARG_END);
              }
            }

            if (INS_HasMemoryRead2(ins)) {
              if (INS_HasFallThrough(ins)) {
                INS_InsertCall(ins, IPOINT_AFTER,
                               (AFUNPTR)__AfterRaceRead2,
                               IARG_THREAD_ID,
                               IARG_PTR, inst,
                               IARG_END);
              }

              if (INS_IsBranchOrCall(ins)) {
                INS_InsertCall(ins, IPOINT_TAKEN_BRANCH,
                               (AFUNPTR)__AfterRaceRead2,
                               IARG_THREAD_ID,
                               IARG_PTR, inst,
                               IARG_END);
              }
            }
          } // end of if racy
        } // end of if memory ins
      } // end of for each ins
    } // end of for each bbl
  } // end of if sched_race_
}

void Controller::HandleProgramStart() {
  ExecutionControl::HandleProgramStart();

  // create the scheduler thread (internal pintool thread)
  THREADID tid = PIN_SpawnInternalThread(__SchedulerThread,
                                         NULL, // no argument passed
                                         0, // use default stack size
                                         &scheduler_thd_uid_);
  if (tid == INVALID_THREADID)
    Abort("fail to create the scheduler thread\n");

  // register fini unlock function
  // this funciton is used to join the scheduler thread
  PIN_AddFiniUnlockedFunction(__SchedulerThreadReclaim, NULL);

  // invoke the callback in the scheduler
  scheduler_->ProgramStart();

  // set affinity and os sched policy (FIFO)
  SetAffinity();
  SetSchedPolicy();
}

void Controller::HandleProgramExit() {
  ExecutionControl::HandleProgramExit();

  // invoke the callback in the scheduler
  scheduler_->ProgramExit();

  // save status
  program_->Save(knob_->ValueStr("program_out"), sinfo_);
}

void Controller::HandleImageLoad(IMG img, Image *image) {
  address_t low_addr = IMG_LowAddress(img);
  address_t high_addr = IMG_HighAddress(img);
  address_t data_start = 0;
  size_t data_size = 0;
  address_t bss_start = 0;
  size_t bss_size = 0;

  for(SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    if (SEC_Name(sec) == ".data") {
      data_start = SEC_Address(sec);
      data_size = SEC_Size(sec);
    }
    if (SEC_Name(sec) == ".bss") {
      bss_start = SEC_Address(sec);
      bss_size = SEC_Size(sec);
    }
  }

  CALL_ANALYSIS_FUNC(ImageLoad, image, low_addr, high_addr, data_start,
                     data_size, bss_start, bss_size);

  // update region table
  LockKernel();
  if (data_start) {
    DEBUG_ASSERT(data_size);
    AllocSRegion(data_start, data_size, image);
  }
  if (bss_start) {
    DEBUG_ASSERT(bss_size);
    AllocSRegion(bss_start, bss_size, image);
  }
  UnlockKernel();
}

void Controller::HandleImageUnload(IMG img, Image *image) {
  address_t low_addr = IMG_LowAddress(img);
  address_t high_addr = IMG_HighAddress(img);
  address_t data_start = 0;
  size_t data_size = 0;
  address_t bss_start = 0;
  size_t bss_size = 0;

  for(SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    if (SEC_Name(sec) == ".data") {
      data_start = SEC_Address(sec);
      data_size = SEC_Size(sec);
    }
    if (SEC_Name(sec) == ".bss") {
      bss_start = SEC_Address(sec);
      bss_size = SEC_Size(sec);
    }
  }

  CALL_ANALYSIS_FUNC(ImageUnload, image, low_addr, high_addr, data_start,
                     data_size, bss_start, bss_size);

  // update region table
  LockKernel();
  if (data_start)
    FreeSRegion(data_start);
  if (bss_start)
    FreeSRegion(bss_start);
  UnlockKernel();
}

void Controller::HandleThreadStart() {
  thread_id_t self = Self();
  thread_id_t parent = GetParent();

  LockKernel();
  // get the unique thread identifier
  Thread *curr_thd = NULL;
  if (!main_thread_started_) {
    // this is the main thread
    curr_thd = program_->GetMainThread();
  } else {
    // this is the child thread, get the creator
    DEBUG_ASSERT(parent != INVALID_THD_ID);
    Thread *parent_thd = FindThread(parent);
    DEBUG_ASSERT(parent_thd);
    Thread::idx_t creator_idx = ++thread_creation_info_[parent];
    curr_thd = program_->GetThread(parent_thd, creator_idx);
  }
  // initialize per-thread data structures
  perm_sem_table_[self] = CreateSemaphore(0);
  thread_table_[self] = curr_thd;
  thread_reverse_table_[curr_thd] = self;
  action_table_[self] = NULL;
  enable_table_[self] = true;
  thread_creation_info_[self] = 0;
  active_table_[self] = true;
  if (sched_race_)
    race_active_table_[self] = false;
  UnlockKernel();

  ExecutionControl::HandleThreadStart();
}

void Controller::HandleThreadExit() {
  ExecutionControl::HandleThreadExit();

  thread_id_t self = Self();
  LockKernel();
  // notify the joiners
  JoinInfo *join_info = GetJoinInfo(self);
  join_info->exit = true;
  for (JoinInfo::WaitQueue::iterator it = join_info->wait_queue.begin();
       it != join_info->wait_queue.end(); ++it) {
    DEBUG_ASSERT(!enable_table_[*it]);
    enable_table_[*it] = true;
  }
  join_info->wait_queue.clear();

  // clean up self
  enable_table_[self] = false;
  active_table_[self] = false;
  if (sched_race_)
    race_active_table_[self] = false;
  // schedule on exit
  ScheduleOnExit(self);
  UnlockKernel();
}

void Controller::HandleSchedulerThread() {
  // this is the main entrance of the scheduler thread
  LockKernel();
  // wait for the initial state to be reached
  WaitForNextState();
  // create the initial state
  State *initial_state = CreateState();
  // call the main explore function in the scheduler
  scheduler_->Main(initial_state);
  // explore returns when program is about to exit
  // if the program is not exiting, wa are in a deadlock
  if (!program_exiting_) {
    printf("[CHESS] program deadlock\n");
  }
  UnlockKernel();
}

void Controller::HandleSchedulerThreadReclaim() {
  DEBUG_ASSERT(scheduler_thd_uid_ != INVALID_PIN_THREAD_UID);

  // wait until the scheduler thread finish
  bool success = false;
  success = PIN_WaitForThreadTermination(scheduler_thd_uid_,
                                         PIN_INFINITE_TIMEOUT,
                                         NULL);
  assert(success);
}

void Controller::HandleBeforeRaceRead(THREADID tid, Inst *inst,
                                      address_t addr, size_t size) {
  DEBUG_ASSERT(sched_race_);
  if (sched_app_ && inst->image()->IsCommonLib())
    return;

  thread_id_t self = Self();
  LockKernel();
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // schedule point
    Schedule(self, iaddr, OP_MEM_READ, inst);
    // activate racy region after the first memory op
    race_active_table_[self] = true;
  }
  UnlockKernel();
}

void Controller::HandleAfterRaceRead(THREADID tid, Inst *inst,
                                     address_t addr, size_t size) {
  DEBUG_ASSERT(sched_race_);
  if (sched_app_ && inst->image()->IsCommonLib())
    return;

  thread_id_t self = Self();
  LockKernel();
  race_active_table_[self] = false;
  UnlockKernel();
}

void Controller::HandleBeforeRaceWrite(THREADID tid, Inst *inst,
                                       address_t addr, size_t size) {
  DEBUG_ASSERT(sched_race_);
  if (sched_app_ && inst->image()->IsCommonLib())
    return;

  thread_id_t self = Self();
  LockKernel();
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // schedule point
    Schedule(self, iaddr, OP_MEM_WRITE, inst);
    // activate racy region after the first memory op
    race_active_table_[self] = true;
  }
  UnlockKernel();
}

void Controller::HandleAfterRaceWrite(THREADID tid, Inst *inst,
                                      address_t addr, size_t size) {
  DEBUG_ASSERT(sched_race_);
  if (sched_app_ && inst->image()->IsCommonLib())
    return;

  thread_id_t self = Self();
  LockKernel();
  race_active_table_[self] = false;
  UnlockKernel();
}

// main processing functions
int Controller::MutexTryLock(thread_id_t self,
                             address_t mutex_addr,
                             Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);
  int ret_val = 0;

  // schedule point
  Schedule(self, mutex_addr, OP_MUTEX_TRYLOCK, inst);

  // determine mutex status
  MutexInfo *mutex_info = GetMutexInfo(mutex_addr);
  if (mutex_info->holder != INVALID_THD_ID) {
    // mutex is still hold by other thread
    ret_val = EBUSY;
  } else {
    // mutex is open, grab the mutex
    mutex_info->holder = self;
    for (MutexInfo::ReadyMap::iterator it = mutex_info->ready_map.begin();
         it != mutex_info->ready_map.end(); ++it) {
      DEBUG_ASSERT(enable_table_[it->first]);
      enable_table_[it->first] = false;
      mutex_info->wait_queue.push_back(it->first);
    }
  }

  return ret_val;
}

void Controller::MutexLock(thread_id_t self,
                           address_t mutex_addr,
                           Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  // check mutex status
  MutexInfo *mutex_info = GetMutexInfo(mutex_addr);
  if (mutex_info->holder != INVALID_THD_ID) {
    DEBUG_ASSERT(mutex_info->holder != self);
    enable_table_[self] = false;
    mutex_info->wait_queue.push_back(self);
  }
  mutex_info->ready_map[self] = false;

  // schedule point
  Schedule(self, mutex_addr, OP_MUTEX_LOCK, inst);
  DEBUG_ASSERT(mutex_info->holder == INVALID_THD_ID);

  // grab the mutex
  mutex_info->holder = self;
  mutex_info->ready_map.erase(self);
  for (MutexInfo::ReadyMap::iterator it = mutex_info->ready_map.begin();
       it != mutex_info->ready_map.end(); ++it) {
    DEBUG_ASSERT(enable_table_[it->first]);
    enable_table_[it->first] = false;
    mutex_info->wait_queue.push_back(it->first);
  }
}

void Controller::MutexUnlock(thread_id_t self,
                             address_t mutex_addr,
                             Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  // schedule point
  Schedule(self, mutex_addr, OP_MUTEX_UNLOCK, inst);

  // release the mutex
  MutexInfo *mutex_info = GetMutexInfo(mutex_addr);
  DEBUG_ASSERT(mutex_info->holder == self);
  mutex_info->holder = INVALID_THD_ID;
  for (MutexInfo::WaitQueue::iterator it = mutex_info->wait_queue.begin();
       it != mutex_info->wait_queue.end(); ++it) {
    DEBUG_ASSERT(!enable_table_[*it]);
    enable_table_[*it] = true;
  }
  mutex_info->wait_queue.clear();
}

void Controller::CondSignal(thread_id_t self,
                            address_t cond_addr,
                            Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  // schedule point
  Schedule(self, cond_addr, OP_COND_SIGNAL, inst);

  // wake up
  CondInfo *cond_info = GetCondInfo(cond_addr);
  CondInfo::signal_id_t next_signal_id = ++cond_info->curr_signal_id;
  for (CondInfo::WaitMap::iterator mit = cond_info->wait_map.begin();
       mit != cond_info->wait_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;
    CondInfo::WaitInfo &wait_info = mit->second;
    if (!wait_info.broadcasted) {
      // set enabled if needed
      if (!wait_info.timed && wait_info.signal_set.empty()) {
        DEBUG_ASSERT(!enable_table_[thd_id]);
        enable_table_[thd_id] = true;
      }
      // update signal set
      wait_info.signal_set.insert(next_signal_id);
    }
  }
}

void Controller::CondBroadcast(thread_id_t self,
                               address_t cond_addr,
                               Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  // schedule point
  Schedule(self, cond_addr, OP_COND_BROADCAST, inst);

  // wake up
  CondInfo *cond_info = GetCondInfo(cond_addr);
  for (CondInfo::WaitMap::iterator mit = cond_info->wait_map.begin();
       mit != cond_info->wait_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;
    CondInfo::WaitInfo &wait_info = mit->second;
    if (!wait_info.broadcasted) {
      // set enabled if needed
      if (!wait_info.timed && wait_info.signal_set.empty()) {
        DEBUG_ASSERT(!enable_table_[thd_id]);
        enable_table_[thd_id] = true;
      }
      // clear signal set and set broadcasted to true
      wait_info.broadcasted = true;
      wait_info.signal_set.clear();
    }
  }
}

void Controller::CondWait(thread_id_t self,
                          address_t cond_addr,
                          Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  // wait on the cond variable
  CondInfo *cond_info = GetCondInfo(cond_addr);
  // initialize wait info
  CondInfo::WaitInfo &self_wait_info = cond_info->wait_map[self];
  self_wait_info.timed = false;
  self_wait_info.broadcasted = false;
  // disable self
  enable_table_[self] = false;

  // schedule point
  Schedule(self, cond_addr, OP_COND_WAIT, inst);

  // wake up
  self_wait_info = cond_info->wait_map[self];
  if (!self_wait_info.broadcasted) {
    // at least one signal should exist in the signal_set
    // otherwise, we should not reach here
    DEBUG_ASSERT(!self_wait_info.signal_set.empty());
    CondInfo::signal_id_t signal_id = *self_wait_info.signal_set.begin();
    // iterate against other wait info to put some waiters back
    // into sleep because a signal can only wake up one waiter
    for (CondInfo::WaitMap::iterator mit = cond_info->wait_map.begin();
         mit != cond_info->wait_map.end(); ++mit) {
      thread_id_t thd_id = mit->first;
      CondInfo::WaitInfo &wait_info = mit->second;
      // skip self
      if (thd_id == self)
        continue;
      // remove signal_id from its signal set if needed
      if (!wait_info.broadcasted) {
        CondInfo::SignalSet::iterator it
            = wait_info.signal_set.find(signal_id);
        if (it != wait_info.signal_set.end()) {
          wait_info.signal_set.erase(it);
          if (!wait_info.timed && wait_info.signal_set.empty()) {
            DEBUG_ASSERT(enable_table_[thd_id]);
            enable_table_[thd_id] = false;
          }
        }
      }
    } // end of for each waiter
  }
  cond_info->wait_map.erase(self);
}

int Controller::CondTimedwait(thread_id_t self,
                              address_t cond_addr,
                              Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);
  int ret_val = 0;

  // wait on the cond variable
  CondInfo *cond_info = GetCondInfo(cond_addr);
  // initialize wait info
  CondInfo::WaitInfo &self_wait_info = cond_info->wait_map[self];
  self_wait_info.timed = true;
  self_wait_info.broadcasted = false;
  // no need to disable self

  // schedule point
  Action *action = Schedule(self, cond_addr, OP_COND_TIMEDWAIT, inst);

  self_wait_info = cond_info->wait_map[self];
  if (!self_wait_info.broadcasted) {
    if (self_wait_info.signal_set.empty()) {
      // timeout happens
      ret_val = ETIMEDOUT;
      action->set_yield(true);
    } else {
      CondInfo::signal_id_t signal_id = *self_wait_info.signal_set.begin();
      // iterate against other wait info to put some waiters back
      // into sleep because a signal can only wake up one waiter
      for (CondInfo::WaitMap::iterator mit = cond_info->wait_map.begin();
           mit != cond_info->wait_map.end(); ++mit) {
        thread_id_t thd_id = mit->first;
        CondInfo::WaitInfo &wait_info = mit->second;
        // skip self
        if (thd_id == self)
          continue;
        // remove signal_id from its signal set if needed
        if (!wait_info.broadcasted) {
          CondInfo::SignalSet::iterator it
              = wait_info.signal_set.find(signal_id);
          if (it != wait_info.signal_set.end()) {
            wait_info.signal_set.erase(it);
            if (!wait_info.timed && wait_info.signal_set.empty()) {
              DEBUG_ASSERT(enable_table_[thd_id]);
              enable_table_[thd_id] = false;
            }
          }
        }
      } // end of for each waiter
    }
  }
  cond_info->wait_map.erase(self);

  return ret_val;
}

void Controller::BarrierWait(thread_id_t self,
                             address_t barrier_addr,
                             Inst *inst) {
  DEBUG_ASSERT(enable_table_[self]);

  BarrierInfo *barrier_info = GetBarrierInfo(barrier_addr);
  if (barrier_info->wait_queue.size() + 1 < barrier_info->count) {
    // wait for other threads to reach the barrier
    enable_table_[self] = false;
    barrier_info->wait_queue.push_back(self);
  } else {
    // all threads reach the barrier, wakeup
    for (BarrierInfo::WaitQueue::iterator it = barrier_info->wait_queue.begin();
         it != barrier_info->wait_queue.end(); ++it) {
      DEBUG_ASSERT(!enable_table_[*it]);
      enable_table_[*it] = true;
    }
    barrier_info->wait_queue.clear();
  }

  // schedule point
  Schedule(self, barrier_addr, OP_BARRIER_WAIT, inst);
}

Action *Controller::CreateAction(thread_id_t thd_id,
                                 address_t iaddr,
                                 Operation op,
                                 Inst *inst) {
  Thread *thd = FindThread(thd_id);
  DEBUG_ASSERT(thd);
  Object *obj = NULL;
  if (iaddr) {
    obj = GetObject(iaddr);
    DEBUG_ASSERT(obj);
  }
  Action *action = execution_->CreateAction(thd, obj, op, inst);
  return action;
}

State *Controller::CreateState() {
  State *state = execution_->CreateState();

  if (sched_race_) {
    // sched race mode
    for (std::map<thread_id_t, bool>::iterator it = race_active_table_.begin();
         it != race_active_table_.end(); ++it) {
      if (it->second) {
        Action *action = action_table_[it->first];
        DEBUG_ASSERT(action);
        DEBUG_ASSERT(enable_table_[it->first]);
        state->AddEnabled(action);
        return state;
      }
    }
  }

  // normal mode
  for (std::map<thread_id_t, Action *>::iterator it = action_table_.begin();
       it != action_table_.end(); ++it) {
    thread_id_t thd_id = it->first;
    Action *action = it->second;
    if (action && enable_table_[thd_id]) {
      state->AddEnabled(action);
    }
  }
  return state;
}

void Controller::WaitForNextState() {
  UnlockKernel();
  next_state_sem_->Wait();
  LockKernel();
  DEBUG_ASSERT(next_state_ready_);
  next_state_ready_ = false;
}

State *Controller::Execute(State *state, Action *action) {
  // grant permission
  thread_id_t target = thread_reverse_table_[action->thd()];
  SemPost(perm_sem_table_[target]);
  // wait for the next state
  WaitForNextState();
  // return the new state
  State *new_state = CreateState();
  return new_state;
}

Action *Controller::Schedule(thread_id_t self,
                             address_t iaddr,
                             Operation op,
                             Inst *inst) {
  // create the action
  Action *action = CreateAction(self, iaddr, op, inst);
  // register action
  action_table_[self] = action;
  // check flag next_state_ready
  if (!next_state_ready_) {
    // by setting this flag, other runnable threads will
    // go to wait status immediately if they call this func
    next_state_ready_ = true;
    // make sure that other runnable threads can be executed
    // as far as they can (assume the os sched policy is FIFO)
    // we use multiple yields here to reduce the noise
    UnlockKernel();
    for (int i = 0; i < 2; i++) Yield();
    LockKernel();
    // notify the scheduler thread to process the next state
    SemPost(next_state_sem_);
  }
  // wait for permission to proceed
  active_table_[self] = false;
  UnlockKernel();
  SemWait(perm_sem_table_[self]);
  LockKernel();
  DEBUG_ASSERT(enable_table_[self]);
  active_table_[self] = true;
  // unregister action
  action_table_[self] = NULL;
  // return the action
  return action;
}

void Controller::ScheduleOnExit(thread_id_t self) {
  for (std::map<thread_id_t, bool>::iterator it = active_table_.begin();
       it != active_table_.end(); ++it) {
    if (it->second)
      return;
  }
  // mark program as exiting if needed
  if (self == main_thd_id_)
    program_exiting_ = true;
  next_state_ready_ = true;
  SemPost(next_state_sem_);
}

// helper functions
void Controller::SetScheduler(Scheduler *scheduler) {
  if (scheduler_)
    Abort("please choose only one scheduler\n");
  scheduler->Setup();
  desc_.Merge(scheduler->desc());
  scheduler_ = scheduler;
}

void Controller::SemWait(Semaphore *sem) {
  if (sem->Wait()) {
    Abort("SemWait returns error\n");
  }
}

void Controller::SemPost(Semaphore *sem) {
  if (sem->Post()) {
    Abort("SemPost returns error\n");
  }
}

void Controller::SetAffinity() {
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

void Controller::SetSchedPolicy() {
  DEBUG_FMT_PRINT_SAFE("Setting os sched policy to FIFO\n");

  struct sched_param param;
  param.sched_priority = knob_->ValueInt("realtime_priority");
  if (sched_setscheduler(0, SCHED_FIFO, &param)) {
    fprintf(stderr, "errno = %d\n", errno);
    Abort("SetStrictPriority failed\n");
  }
}

void Controller::Yield() {
  sched_yield();
}

Controller::JoinInfo *Controller::GetJoinInfo(thread_id_t thd_id) {
  JoinInfo::Map::iterator it = join_info_table_.find(thd_id);
  JoinInfo *join_info = NULL;
  if (it == join_info_table_.end()) {
    join_info = new JoinInfo;
    join_info_table_[thd_id] = join_info;
  } else {
    join_info = it->second;
  }
  DEBUG_ASSERT(join_info);
  return join_info;
}

Controller::MutexInfo *Controller::GetMutexInfo(address_t iaddr) {
  Region *region = FindRegion(iaddr);
  DEBUG_ASSERT(region);
  MutexInfo::Map::iterator it = region->mutex_info_table.find(iaddr);
  MutexInfo *mutex_info = NULL;
  if (it == region->mutex_info_table.end()) {
    mutex_info = new MutexInfo;
    region->mutex_info_table[iaddr] = mutex_info;
  } else {
    mutex_info = it->second;
  }
  DEBUG_ASSERT(mutex_info);
  return mutex_info;
}

Controller::CondInfo *Controller::GetCondInfo(address_t iaddr) {
  Region *region = FindRegion(iaddr);
  DEBUG_ASSERT(region);
  CondInfo::Map::iterator it = region->cond_info_table.find(iaddr);
  CondInfo *cond_info = NULL;
  if (it == region->cond_info_table.end()) {
    cond_info = new CondInfo;
    region->cond_info_table[iaddr] = cond_info;
  } else {
    cond_info = it->second;
  }
  DEBUG_ASSERT(cond_info);
  return cond_info;
}

Controller::BarrierInfo *Controller::GetBarrierInfo(address_t iaddr) {
  Region *region = FindRegion(iaddr);
  DEBUG_ASSERT(region);
  BarrierInfo::Map::iterator it = region->barrier_info_table.find(iaddr);
  BarrierInfo *barrier_info = NULL;
  if (it == region->barrier_info_table.end()) {
    barrier_info = new BarrierInfo;
    region->barrier_info_table[iaddr] = barrier_info;
  } else {
    barrier_info = it->second;
  }
  DEBUG_ASSERT(barrier_info);
  return barrier_info;
}

Object *Controller::GetObject(address_t iaddr) {
  Region *region = FindRegion(iaddr);
  DEBUG_ASSERT(region);
  SRegion *sregion = dynamic_cast<SRegion *>(region);
  if (sregion) {
    Image *image = sregion->image;
    address_t offset = iaddr - sregion->addr;
    return program_->GetSObject(image, offset);
  }
  DRegion *dregion = dynamic_cast<DRegion *>(region);
  if (dregion) {
    Thread *creator = dregion->creator;
    Inst *creator_inst = dregion->creator_inst;
    Object::idx_t creator_idx = dregion->creator_idx;
    address_t offset = iaddr - dregion->addr;
    return program_->GetDObject(creator, creator_inst, creator_idx, offset);
  }
  // should not reach here
  Abort("GetObject return NULL\n");
  return NULL;
}

Thread *Controller::FindThread(thread_id_t thd_id) {
  std::map<thread_id_t, Thread *>::iterator it = thread_table_.find(thd_id);
  if (it == thread_table_.end())
    return NULL;
  else
    return it->second;
}

Controller::Region *Controller::FindRegion(address_t iaddr) {
  // iaddr is an aligned unit address
  if (region_table_.begin() == region_table_.end())
    return NULL;

  Region::Map::iterator it = region_table_.upper_bound(iaddr);
  if (it == region_table_.begin())
    return NULL;

  it--;
  Region *region = it->second;
  address_t region_start = region->addr;
  size_t region_size = region->size;
  if (iaddr >= region_start && iaddr < region_start + region_size)
    return region;
  else
    return NULL;
}

void Controller::AllocSRegion(address_t addr, size_t size, Image *image) {
  // create a static region
  SRegion *region = new SRegion;
  region->addr = addr;
  region->size = size;
  region->image = image;
  region_table_[addr] = region;
}

void Controller::AllocDRegion(address_t addr, size_t size, Inst *inst) {
  thread_id_t self = Self();
  // create a dynamic region
  Thread *creator = FindThread(self);
  DEBUG_ASSERT(creator);
  Object::idx_t creator_idx = GetCreatorIdx(self, inst);
  DRegion *region = new DRegion;
  region->addr = addr;
  region->size = size;
  region->creator = creator;
  region->creator_inst = inst;
  region->creator_idx = creator_idx;
  region_table_[addr] = region;
}

void Controller::FreeSRegion(address_t addr) {
  // delete a static region
  Region::Map::iterator it = region_table_.find(addr);
  if (it != region_table_.end()) {
    SRegion *region = dynamic_cast<SRegion *>(it->second);
    DEBUG_ASSERT(region);
    region_table_.erase(it);
    FreeMutexInfo(region);
    FreeCondInfo(region);
    delete region;
  }
}

void Controller::FreeDRegion(address_t addr) {
  // delete a dynamic region
  Region::Map::iterator it = region_table_.find(addr);
  if (it != region_table_.end()) {
    DRegion *region = dynamic_cast<DRegion *>(it->second);
    DEBUG_ASSERT(region);
    region_table_.erase(it);
    FreeMutexInfo(region);
    FreeCondInfo(region);
    delete region;
  }
}

void Controller::FreeMutexInfo(Region *region) {
  for (MutexInfo::Map::iterator it = region->mutex_info_table.begin();
       it != region->mutex_info_table.end(); ++it) {
    delete it->second;
  }
}

void Controller::FreeCondInfo(Region *region) {
  for (CondInfo::Map::iterator it = region->cond_info_table.begin();
       it != region->cond_info_table.end(); ++it) {
    delete it->second;
  }
}

Object::idx_t Controller::GetCreatorIdx(thread_id_t thd_id, Inst *inst) {
  CreationInfo info;
  info.creator_thd_id = thd_id;
  info.creator_inst = inst;
  CreationInfo::hash_val_t hash_val = info.Hash();
  CreationInfo::Vec &vec = creation_info_[hash_val];
  for (CreationInfo::Vec::iterator it = vec.begin(); it != vec.end(); ++it) {
    CreationInfo *tmp_info = *it;
    if (info.Match(tmp_info))
      return ++tmp_info->curr_creator_idx;
  }
  // create a new creation info
  CreationInfo *new_info = new CreationInfo;
  new_info->creator_thd_id = thd_id;
  new_info->creator_inst = inst;
  new_info->curr_creator_idx = 0;
  creation_info_[hash_val].push_back(new_info);
  return ++new_info->curr_creator_idx;
}

void Controller::__SchedulerThread(VOID *arg) {
  ((Controller *)ctrl_)->HandleSchedulerThread();
}

void Controller::__SchedulerThreadReclaim(INT32 code, VOID *v) {
  ((Controller *)ctrl_)->HandleSchedulerThreadReclaim();
}

void Controller::__BeforeRaceRead(THREADID tid, Inst *inst, ADDRINT addr,
                                  UINT32 size) {
  ((Controller *)ctrl_)->HandleBeforeRaceRead(tid, inst, addr, size);
  ((Controller *)ctrl_)->tls_race_read_addr_[tid] = addr;
  ((Controller *)ctrl_)->tls_race_read_size_[tid] = size;
}

void Controller::__AfterRaceRead(THREADID tid, Inst *inst) {
  address_t addr = ((Controller *)ctrl_)->tls_race_read_addr_[tid];
  address_t size = ((Controller *)ctrl_)->tls_race_read_size_[tid];
  ((Controller *)ctrl_)->HandleAfterRaceRead(tid, inst, addr, size);
}

void Controller::__BeforeRaceWrite(THREADID tid, Inst *inst, ADDRINT addr,
                                   UINT32 size) {
  ((Controller *)ctrl_)->HandleBeforeRaceWrite(tid, inst, addr, size);
  ((Controller *)ctrl_)->tls_race_write_addr_[tid] = addr;
  ((Controller *)ctrl_)->tls_race_write_size_[tid] = size;
}

void Controller::__AfterRaceWrite(THREADID tid, Inst *inst) {
  address_t addr = ((Controller *)ctrl_)->tls_race_write_addr_[tid];
  size_t size = ((Controller *)ctrl_)->tls_race_write_size_[tid];
  ((Controller *)ctrl_)->HandleAfterRaceWrite(tid, inst, addr, size);
}

void Controller::__BeforeRaceRead2(THREADID tid, Inst *inst, ADDRINT addr,
                                   UINT32 size) {
  ((Controller *)ctrl_)->HandleBeforeRaceRead(tid, inst, addr, size);
  ((Controller *)ctrl_)->tls_race_read2_addr_[tid] = addr;
  ((Controller *)ctrl_)->tls_race_read_size_[tid] = size;
}

void Controller::__AfterRaceRead2(THREADID tid, Inst *inst) {
  address_t addr = ((Controller *)ctrl_)->tls_race_read2_addr_[tid];
  address_t size = ((Controller *)ctrl_)->tls_race_read_size_[tid];
  ((Controller *)ctrl_)->HandleAfterRaceRead(tid, inst, addr, size);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCreate, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCreate,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst);

  LockKernel();
  DEBUG_ASSERT(enable_table_[self]);
  // schedule point
  Schedule(self, 0, OP_THREAD_CREATE, inst);
  UnlockKernel();

  wrapper->CallOriginal();

  // wait until the new child thread start
  thread_id_t child_thd_id = WaitForNewChild(wrapper);

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCreate,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      child_thd_id);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadJoin, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  // get child thd_id
  thread_id_t child = GetThdID(wrapper->arg0());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadJoin,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      child);

  LockKernel();
  DEBUG_ASSERT(enable_table_[self]);
  JoinInfo *join_info = GetJoinInfo(child);
  if (!join_info->exit) {
    enable_table_[self] = false;
    join_info->wait_queue.push_back(self);
  }
  // schedule point
  Schedule(self, 0, OP_THREAD_JOIN, inst);
  UnlockKernel();

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadJoin,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      child);
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexTryLock, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexTryLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t mutex_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
    int ret_val = MutexTryLock(self, mutex_addr, inst);
    UnlockKernel();
    wrapper->set_ret_val(ret_val);
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexTryLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->ret_val());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexLock, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t mutex_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
    MutexLock(self, mutex_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexLock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadMutexUnlock, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t mutex_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
    MutexUnlock(self, mutex_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadMutexUnlock,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondSignal, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondSignal,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t cond_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
    CondSignal(self, cond_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondSignal,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondBroadcast, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondBroadcast,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t cond_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
    CondBroadcast(self, cond_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondBroadcast,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondWait, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t cond_addr = (address_t)wrapper->arg0();
    address_t mutex_addr = (address_t)wrapper->arg1();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
    MutexUnlock(self, mutex_addr, inst);
    CondWait(self, cond_addr, inst);
    MutexLock(self, mutex_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadCondTimedwait, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadCondTimedwait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t cond_addr = (address_t)wrapper->arg0();
    address_t mutex_addr = (address_t)wrapper->arg1();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
    MutexUnlock(self, mutex_addr, inst);
    int ret_val = CondTimedwait(self, cond_addr, inst);
    MutexLock(self, mutex_addr, inst);
    UnlockKernel();
    // set return value
    wrapper->set_ret_val(ret_val);
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadCondTimedwait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      (address_t)wrapper->arg1());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadBarrierInit, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadBarrierInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());

  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    DEBUG_ASSERT(enable_table_[self]);
    address_t barrier_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(barrier_addr, unit_size_) == barrier_addr);
    // schedule point
    Schedule(self, barrier_addr, OP_BARRIER_INIT, inst);
    // set the barrier count
    BarrierInfo *barrier_info = GetBarrierInfo(barrier_addr);
    DEBUG_ASSERT(barrier_info->wait_queue.empty());
    barrier_info->count = wrapper->arg2();
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadBarrierInit,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg2());
}

IMPLEMENT_WRAPPER_HANDLER(PthreadBarrierWait, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      BeforePthreadBarrierWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());


  if (sched_app_ && inst->image()->IsCommonLib()) {
    wrapper->CallOriginal();
  } else {
    // simulated function
    LockKernel();
    address_t barrier_addr = (address_t)wrapper->arg0();
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(barrier_addr, unit_size_) == barrier_addr);
    BarrierWait(self, barrier_addr, inst);
    UnlockKernel();
  }

  CALL_ANALYSIS_FUNC2(PthreadFunc,
                      AfterPthreadBarrierWait,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(Sleep, Controller) {
  if (scheduler_->desc()->HookYieldFunc()) {
    // simulated function
    thread_id_t self = Self();
    Inst *inst = GetInst(wrapper->ret_addr());

    LockKernel();
    DEBUG_ASSERT(enable_table_[self]);
    // schedule point
    Action *action = Schedule(self, 0, OP_SLEEP, inst);
    action->set_yield(true);
    UnlockKernel();
  } else {
    wrapper->CallOriginal();
  }
}

IMPLEMENT_WRAPPER_HANDLER(Usleep, Controller) {
  if (scheduler_->desc()->HookYieldFunc()) {
    // simulated function
    thread_id_t self = Self();
    Inst *inst = GetInst(wrapper->ret_addr());

    LockKernel();
    DEBUG_ASSERT(enable_table_[self]);
    // schedule point
    Action *action = Schedule(self, 0, OP_USLEEP, inst);
    action->set_yield(true);
    UnlockKernel();
  } else {
    wrapper->CallOriginal();
  }
}

IMPLEMENT_WRAPPER_HANDLER(SchedYield, Controller) {
  if (scheduler_->desc()->HookYieldFunc()) {
    // simulated function
    thread_id_t self = Self();
    Inst *inst = GetInst(wrapper->ret_addr());

    LockKernel();
    DEBUG_ASSERT(enable_table_[self]);
    // schedule point
    Action *action = Schedule(self, 0, OP_SCHED_YIELD, inst);
    action->set_yield(true);
    UnlockKernel();
  } else {
    wrapper->CallOriginal();
  }
}

IMPLEMENT_WRAPPER_HANDLER(Malloc, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeMalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0());

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterMalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      (address_t)wrapper->ret_val());

  // alloc dynamic region
  LockKernel();
  AllocDRegion((address_t)wrapper->ret_val(), wrapper->arg0(), inst);
  UnlockKernel();
}

IMPLEMENT_WRAPPER_HANDLER(Calloc, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeCalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      wrapper->arg1());

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterCalloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      wrapper->arg1(),
                      (address_t)wrapper->ret_val());

  // alloc dynamic region
  LockKernel();
  size_t size = wrapper->arg0() * wrapper->arg1();
  AllocDRegion((address_t)wrapper->ret_val(), size, inst);
  UnlockKernel();
}

IMPLEMENT_WRAPPER_HANDLER(Realloc, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeRealloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg1());

  // free dynamic region
  LockKernel();
  FreeDRegion((address_t)wrapper->arg0());
  UnlockKernel();

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterRealloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0(),
                      wrapper->arg1(),
                      (address_t)wrapper->ret_val());

  // alloc dynamic region
  LockKernel();
  AllocDRegion((address_t)wrapper->ret_val(), wrapper->arg1(), inst);
  UnlockKernel();
}

IMPLEMENT_WRAPPER_HANDLER(Free, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeFree,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());

  // free dynamic region
  LockKernel();
  FreeDRegion((address_t)wrapper->arg0());
  UnlockKernel();

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterFree,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      (address_t)wrapper->arg0());
}

IMPLEMENT_WRAPPER_HANDLER(Valloc, Controller) {
  thread_id_t self = Self();
  Inst *inst = GetInst(wrapper->ret_addr());

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      BeforeValloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0());

  wrapper->CallOriginal();

  CALL_ANALYSIS_FUNC2(MallocFunc,
                      AfterValloc,
                      self,
                      GetThdClk(wrapper->tid()),
                      inst,
                      wrapper->arg0(),
                      (address_t)wrapper->ret_val());

  // alloc dynamic region
  LockKernel();
  AllocDRegion((address_t)wrapper->ret_val(), wrapper->arg0(), inst);
  UnlockKernel();
}

} // namespace systematic

