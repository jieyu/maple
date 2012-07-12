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

// File: tracer/recorder.h - Define the analyzer for recording traces.

#ifndef TRACER_RECORDER_H_
#define TRACER_RECORDER_H_

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "tracer/log.h"

namespace tracer {

// Analyzer for recording traces.
class RecorderAnalyzer : public Analyzer {
 public:
  RecorderAnalyzer();
  ~RecorderAnalyzer() {}

  void Register();
  bool Enabled();
  void Setup(Mutex *lock);

  void ProgramStart() {
    trace_log_->OpenForWrite();
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_PROGRAM_START);
  }

  void ProgramExit() {
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_PROGRAM_EXIT);
    trace_log_->CloseForWrite();
  }

  void ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                 address_t data_start, size_t data_size, address_t bss_start,
                 size_t bss_size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_IMAGE_LOAD);
    entry.add_arg(image->id());
    entry.add_arg(low_addr);
    entry.add_arg(high_addr);
    entry.add_arg(data_start);
    entry.add_arg(data_size);
    entry.add_arg(bss_start);
    entry.add_arg(bss_size);
  }

  void ImageUnload(Image *image, address_t low_addr, address_t high_addr,
                   address_t data_start, size_t data_size, address_t bss_start,
                   size_t bss_size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_IMAGE_UNLOAD);
    entry.add_arg(image->id());
    entry.add_arg(low_addr);
    entry.add_arg(high_addr);
    entry.add_arg(data_start);
    entry.add_arg(data_size);
    entry.add_arg(bss_start);
    entry.add_arg(bss_size);
  }

  void SyscallEntry(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    int syscall_num) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_SYSCALL_ENTRY);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.add_arg(syscall_num);
  }

  void SyscallExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   int syscall_num) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_SYSCALL_EXIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.add_arg(syscall_num);
  }

  void SignalReceived(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      int signal_num) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_SIGNAL_RECEIVED);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.add_arg(signal_num);
  }

  void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_THREAD_START);
    entry.set_thd_id(curr_thd_id);
    entry.add_arg(parent_thd_id);
  }

  void ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_THREAD_EXIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
  }

  void Main(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_MAIN);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
  }

  void ThreadMain(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_THREAD_MAIN);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
  }

  void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_MEM_READ);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(size);
  }

  void AfterMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t addr, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_MEM_READ);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(size);
  }

  void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_MEM_WRITE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(size);
  }

  void AfterMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_MEM_WRITE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(size);
  }

  void BeforeAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, std::string type, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_ATOMIC_INST);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_str_arg(type);
  }

  void AfterAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                       Inst *inst, std::string type, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_ATOMIC_INST);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_str_arg(type);
  }

  void BeforePthreadCreate(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_CREATE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
  }

  void AfterPthreadCreate(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, thread_id_t child_thd_id) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_CREATE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(child_thd_id);
  }

  void BeforePthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                         Inst *inst, thread_id_t child_thd_id) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_JOIN);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(child_thd_id);
  }

  void AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, thread_id_t child_thd_id) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_JOIN);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(child_thd_id);
  }

  void BeforePthreadMutexTryLock(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_MUTEX_TRYLOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadMutexTryLock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr, int ret_val) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_MUTEX_TRYLOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(ret_val);
  }

  void BeforePthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_MUTEX_LOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_MUTEX_LOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_MUTEX_UNLOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadMutexUnlock(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_MUTEX_UNLOCK);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_COND_SIGNAL);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadCondSignal(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_COND_SIGNAL);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_COND_BROADCAST);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadCondBroadcast(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_COND_BROADCAST);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforePthreadCondWait(thread_id_t curr_thd_id,
                             timestamp_t curr_thd_clk, Inst *inst,
                             address_t cond_addr, address_t mutex_addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_COND_WAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(cond_addr);
    entry.add_arg(mutex_addr);
  }

  void AfterPthreadCondWait(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t cond_addr,
                            address_t mutex_addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_COND_WAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(cond_addr);
    entry.add_arg(mutex_addr);
  }

  void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  address_t cond_addr, address_t mutex_addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_COND_TIMEDWAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(cond_addr);
    entry.add_arg(mutex_addr);
  }

  void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t cond_addr, address_t mutex_addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_COND_TIMEDWAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(cond_addr);
    entry.add_arg(mutex_addr);
  }

  void BeforePthreadBarrierInit(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr, unsigned int count) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_BARRIER_INIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(count);
  }

  void AfterPthreadBarrierInit(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr, unsigned int count) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_BARRIER_INIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
    entry.add_arg(count);
  }

  void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_PTHREAD_BARRIER_WAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_PTHREAD_BARRIER_WAIT);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforeMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_MALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(size);
  }

  void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_MALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(size);
    entry.add_arg(addr);
  }

  void BeforeCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t nmemb, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_CALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(nmemb);
    entry.add_arg(size);
  }

  void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t nmemb, size_t size, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_CALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(nmemb);
    entry.add_arg(size);
    entry.add_arg(addr);
  }

  void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t ori_addr, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_REALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(ori_addr);
    entry.add_arg(size);
  }

  void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t ori_addr, size_t size,
                    address_t new_addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_REALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(ori_addr);
    entry.add_arg(size);
    entry.add_arg(new_addr);
  }

  void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                  Inst *inst, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_FREE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void AfterFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                 Inst *inst, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_FREE);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(addr);
  }

  void BeforeValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t size) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_BEFORE_VALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(size);
  }

  void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr) {
    ScopedLock locker(internal_lock_);
    LogEntry entry = trace_log_->NewEntry();
    entry.set_type(LOG_ENTRY_AFTER_VALLOC);
    entry.set_thd_id(curr_thd_id);
    entry.set_thd_clk(curr_thd_clk);
    entry.set_inst_id(inst->id());
    entry.add_arg(size);
    entry.add_arg(addr);
  }

 private:
  Mutex *internal_lock_;
  TraceLog *trace_log_;

  DISALLOW_COPY_CONSTRUCTORS(RecorderAnalyzer);
};

} // namespace tracer

#endif

