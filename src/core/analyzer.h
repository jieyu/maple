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

// File: core/analyzer.h - Analyze (profile) program behavior.

#ifndef CORE_ANALYZER_H_
#define CORE_ANALYZER_H_

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/knob.h"
#include "core/descriptor.h"

// Forward declarations.
class CallStackInfo;

// An analyzer is used to profile program behaviors like an observer. It has no
// control over the execution of the program.
class Analyzer {
 public:
  Analyzer() : callstack_info_(NULL) {
    knob_ = Knob::Get();
  }

  virtual ~Analyzer() {}

  virtual void Register() {}
  virtual bool Enabled() { return false; }
  virtual void ProgramStart() {}
  virtual void ProgramExit() {}
  virtual void ImageLoad(Image *image, address_t low_addr,
                         address_t high_addr, address_t data_start,
                         size_t data_size, address_t bss_start,
                         size_t bss_size) {}
  virtual void ImageUnload(Image *image, address_t low_addr,
                           address_t high_addr, address_t data_start,
                           size_t data_size, address_t bss_start,
                           size_t bss_size) {}
  virtual void SyscallEntry(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            int syscall_num) {}
  virtual void SyscallExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           int syscall_num) {}
  virtual void SignalReceived(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              int signal_num) {}
  virtual void ThreadStart(thread_id_t curr_thd_id,
                           thread_id_t parent_thd_id) {}
  virtual void ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {}
  virtual void Main(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {}
  virtual void ThreadMain(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {}
  virtual void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size) {}
  virtual void AfterMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t addr, size_t size) {}
  virtual void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size) {}
  virtual void AfterMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size) {}
  virtual void BeforeAtomicInst(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                std::string type, address_t addr) {}
  virtual void AfterAtomicInst(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               std::string type, address_t addr) {}
  virtual void BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t target) {}
  virtual void AfterCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                         Inst *inst, address_t target, address_t ret) {}
  virtual void BeforeReturn(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t target) {}
  virtual void AfterReturn(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, address_t target) {}
  virtual void BeforePthreadCreate(thread_id_t curr_thd_id,
                                   timestamp_t curr_thd_clk, Inst *inst) {}
  virtual void AfterPthreadCreate(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  thread_id_t child_thd_id) {}
  virtual void BeforePthreadJoin(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 thread_id_t child_thd_id) {}
  virtual void AfterPthreadJoin(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk,Inst *inst,
                                thread_id_t child_thd_id) {}
  virtual void BeforePthreadMutexTryLock(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t addr) {}
  virtual void AfterPthreadMutexTryLock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr, int ret_val) {}
  virtual void BeforePthreadMutexLock(thread_id_t curr_thd_id,
                                      timestamp_t curr_thd_clk, Inst *inst,
                                      address_t addr) {}
  virtual void AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t addr) {}
  virtual void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {}
  virtual void AfterPthreadMutexUnlock(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr) {}
  virtual void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr) {}
  virtual void AfterPthreadCondSignal(thread_id_t curr_thd_id,
                                      timestamp_t curr_thd_clk, Inst *inst,
                                      address_t addr) {}
  virtual void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t addr) {}
  virtual void AfterPthreadCondBroadcast(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t addr) {}
  virtual void BeforePthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr,
                                     address_t mutex_addr) {}
  virtual void AfterPthreadCondWait(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk, Inst *inst,
                                    address_t cond_addr,
                                    address_t mutex_addr) {}
  virtual void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr) {}
  virtual void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t cond_addr,
                                         address_t mutex_addr) {}
  virtual void BeforePthreadBarrierInit(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr, unsigned int count) {}
  virtual void AfterPthreadBarrierInit(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr, unsigned int count) {}
  virtual void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {}
  virtual void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr) {}
  virtual void BeforeMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t size) {}
  virtual void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {}
  virtual void BeforeCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t nmemb, size_t size) {}
  virtual void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t nmemb, size_t size,
                           address_t addr) {}
  virtual void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size) {}
  virtual void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t ori_addr, size_t size,
                            address_t new_addr) {}
  virtual void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t addr) {}
  virtual void AfterFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                         Inst *inst, address_t addr) {}
  virtual void BeforeValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t size) {}
  virtual void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {}

  Descriptor *desc() { return &desc_; }
  void set_callstack_info(CallStackInfo *info) { callstack_info_ = info; }

 protected:
  Descriptor desc_;
  Knob *knob_;
  CallStackInfo *callstack_info_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Analyzer);
};

#endif

