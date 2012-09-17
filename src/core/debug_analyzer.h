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

// File: core/debug_analyzer.h - The analyzer for debug purpose.

#ifndef CORE_DEBUG_ANALYZER_H_
#define CORE_DEBUG_ANALYZER_H_

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/logging.h"
#include "core/knob.h"

// Debug analyzer. The main function is to print every event.
class DebugAnalyzer : public Analyzer {
 public:
  DebugAnalyzer() {}
  ~DebugAnalyzer() {}

  void Register();
  bool Enabled();
  void Setup();

  void ProgramStart() {
    INFO_FMT_PRINT_SAFE("Program Start\n");
  }

  void ProgramExit() {
    INFO_FMT_PRINT_SAFE("Program Exit\n");
  }

  void ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                 address_t data_start, size_t data_size, address_t bss_start,
                 size_t bss_size) {
    INFO_FMT_PRINT_SAFE("Image Load, name='%s', low=0x%lx, high=0x%lx, "
                        "data_start=0x%lx, data_size=%lu, "
                        "bss_start=0x%lx, bss_size=%lu\n",
                        image->name().c_str(), low_addr, high_addr,
                        data_start, data_size, bss_start, bss_size);
  }

  void ImageUnload(Image *image, address_t low_addr, address_t high_addr,
                   address_t data_start, size_t data_size, address_t bss_start,
                   size_t bss_size) {
    INFO_FMT_PRINT_SAFE("Image Unload, name='%s', low=0x%lx, high=0x%lx, "
                        "data_start=0x%lx, data_size=%lu, "
                        "bss_start=0x%lx, bss_size=%lu\n",
                        image->name().c_str(), low_addr, high_addr,
                        data_start, data_size, bss_start, bss_size);
  }

  void SyscallEntry(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    int syscall_num) {
    INFO_FMT_PRINT_SAFE("[T%lx] Syscall enter num = %d\n",
                        curr_thd_id, syscall_num);
  }

  void SyscallExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   int syscall_num) {
    INFO_FMT_PRINT_SAFE("[T%lx] Syscall exit num = %d\n",
                        curr_thd_id, syscall_num);
  }

  void SignalReceived(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      int signal_num) {
    INFO_FMT_PRINT_SAFE("[T%lx] Signal received, signo = %d\n",
                        curr_thd_id, signal_num);
  }

  void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id) {
    INFO_FMT_PRINT_SAFE("[T%lx] Thread Start, parent=%lx\n",
                        curr_thd_id, parent_thd_id);
  }

  void ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    INFO_FMT_PRINT_SAFE("[T%lx] Thread Exit\n", curr_thd_id);
  }

  void Main(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    INFO_FMT_PRINT_SAFE("[T%lx] Main Func\n", curr_thd_id);
  }

  void ThreadMain(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
    INFO_FMT_PRINT_SAFE("[T%lx] Thread Main Func\n", curr_thd_id);
  }

  void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Read, inst='%s', addr=0x%lx, size=%lu, clk=%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr, size, curr_thd_clk);
  }

  void AfterMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t addr, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Read, inst='%s', addr=0x%lx, size=%lu\n",
        curr_thd_id, inst->ToString().c_str(), addr, size);
  }

  void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Write, inst='%s', addr=0x%lx, size=%lu, clk=%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr, size, curr_thd_clk);
  }

  void AfterMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Write, inst='%s', addr=0x%lx, size=%lu\n",
        curr_thd_id, inst->ToString().c_str(), addr, size);
  }

  void BeforeAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, std::string type, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Atomic Inst, inst='%s', type='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), type.c_str(), addr);
  }

  void AfterAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                       Inst *inst, std::string type, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Atomic Inst, inst='%s', type='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), type.c_str(), addr);
  }

  void BeforeCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                  Inst *inst, address_t target) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Call, inst='%s', target=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), target);
  }

  void AfterCall(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                 Inst *inst, address_t target, address_t ret) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Call, inst='%s', target=0x%lx, ret=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), target, ret);
  }

  void BeforeReturn(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t target) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Return, inst='%s', target=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), target);
  }

  void AfterReturn(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, address_t target) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Return, inst='%s', target=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), target);
  }

  void BeforePthreadCreate(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst) {
    INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadCreate, inst='%s'\n",
                        curr_thd_id, inst->ToString().c_str());
  }

  void AfterPthreadCreate(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, thread_id_t child_thd_id) {
    INFO_FMT_PRINT_SAFE("[T%lx] After PthreadCreate, inst='%s', child=%lx\n",
                        curr_thd_id, inst->ToString().c_str(), child_thd_id);
  }

  void BeforePthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                         Inst *inst, thread_id_t child_thd_id) {
    INFO_FMT_PRINT_SAFE("[T%lx] Before PthreadJoin, inst='%s', child=%lx\n",
                        curr_thd_id, inst->ToString().c_str(), child_thd_id);
  }

  void AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, thread_id_t child_thd_id) {
    INFO_FMT_PRINT_SAFE("[T%lx] After PthreadJoin, inst='%s', child=%lx\n",
                        curr_thd_id, inst->ToString().c_str(), child_thd_id);
  }

  void BeforePthreadMutexTryLock(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadMutexTryLock, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterPthreadMutexTryLock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr, int ret_val) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadMutexTryLock, inst='%s', addr=0x%lx, ret_val=%d\n",
        curr_thd_id, inst->ToString().c_str(), addr, ret_val);
  }

  void BeforePthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadMutexLock, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterPthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadMutexLock, inst='%s', addr=0x%lx, clk=%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr, curr_thd_clk);
  }

  void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadMutexUnlock, inst='%s', addr=0x%lx, clk=%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr, curr_thd_clk);
  }

  void AfterPthreadMutexUnlock(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadMutexUnlock, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondSignal, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterPthreadCondSignal(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondSignal, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondBroadcast, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterPthreadCondBroadcast(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondBroadcast, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void BeforePthreadCondWait(thread_id_t curr_thd_id,
                             timestamp_t curr_thd_clk, Inst *inst,
                             address_t cond_addr, address_t mutex_addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondWait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  }

  void AfterPthreadCondWait(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t cond_addr,
                            address_t mutex_addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondWait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  }

  void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  address_t cond_addr, address_t mutex_addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadCondTimedwait, inst='%s',"
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  }

  void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t cond_addr, address_t mutex_addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadCondTimedwait, inst='%s', "
        "cond_addr=0x%lx, mutex_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), cond_addr, mutex_addr);
  }

  void BeforePthreadBarrierInit(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr, unsigned int count) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadBarrierInit, inst='%s', addr=0x%lx, count=%u\n",
        curr_thd_id, inst->ToString().c_str(), addr, count);
  }

  void AfterPthreadBarrierInit(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr, unsigned int count) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadBarrierInit, inst='%s', addr=0x%lx\n, count=%u\n",
        curr_thd_id, inst->ToString().c_str(), addr, count);
  }

  void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before PthreadBarrierWait, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After PthreadBarrierWait, inst='%s', addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void BeforeMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t size) {
    INFO_FMT_PRINT_SAFE(
      "[T%lx] Before Malloc, inst='%s', size=%lu\n",
      curr_thd_id, inst->ToString().c_str(), size);
  }

  void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Malloc, inst='%s', size=%lu, addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), size, addr);
  }

  void BeforeCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t nmemb, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Calloc, inst='%s', nmemb=%lu, size=%lu\n",
        curr_thd_id, inst->ToString().c_str(), nmemb, size);
  }

  void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t nmemb, size_t size, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Calloc, inst='%s', nmemb=%lu, size=%lu, addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), nmemb, size, addr);
  }

  void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t ori_addr, size_t size) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] Before Realloc, inst='%s', ori_addr=0x%lx, size=%lu\n",
        curr_thd_id, inst->ToString().c_str(), ori_addr, size);
  }

  void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t ori_addr, size_t size,
                    address_t new_addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Realloc, inst='%s', ori_addr=0x%lx, "
        "size=%lu, new_addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), ori_addr, size, new_addr);
  }

  void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                  Inst *inst, address_t addr) {
    INFO_FMT_PRINT_SAFE("[T%lx] Before Free, inst='%s', addr=0x%lx\n",
                        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void AfterFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                 Inst *inst, address_t addr) {
    INFO_FMT_PRINT_SAFE("[T%lx] After Free, inst='%s', addr=0x%lx\n",
                        curr_thd_id, inst->ToString().c_str(), addr);
  }

  void BeforeValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, size_t size) {
    INFO_FMT_PRINT_SAFE("[T%lx] Before Valloc, inst='%s', size=%lu\n",
                        curr_thd_id, inst->ToString().c_str(), size);
  }

  void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr) {
    INFO_FMT_PRINT_SAFE(
        "[T%lx] After Valloc, inst='%s', size=%lu, addr=0x%lx\n",
        curr_thd_id, inst->ToString().c_str(), size, addr);
  }

 private:
  DISALLOW_COPY_CONSTRUCTORS(DebugAnalyzer);
};

#endif

