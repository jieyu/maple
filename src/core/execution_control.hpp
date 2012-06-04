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

// File: core/execution_control.hpp - The main controller definitions.

#ifndef CORE_EXECUTION_CONTROL_HPP_
#define CORE_EXECUTION_CONTROL_HPP_

#include <list>
#include <map>

#include "pin.H"

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/static_info.h"
#include "core/descriptor.h"
#include "core/analyzer.h"
#include "core/debug_analyzer.h"
#include "core/pin_sync.hpp"
#include "core/pin_knob.hpp"
#include "core/pthread_wrapper.hpp"
#include "core/malloc_wrapper.hpp"
#include "core/sched_wrapper.hpp"
#include "core/unistd_wrapper.hpp"

#define CALL_ANALYSIS_FUNC(func,...) \
    for (AnalyzerContainer::iterator it = analyzers_.begin(); \
         it != analyzers_.end(); ++it) { \
      (*it)->func(__VA_ARGS__); \
    }

#define CALL_ANALYSIS_FUNC2(type,func,...) \
    for (AnalyzerContainer::iterator it = analyzers_.begin(); \
         it != analyzers_.end(); ++it) { \
      if ((*it)->desc()->Hook##type()) \
        (*it)->func(__VA_ARGS__); \
    }

// The main controller for the dynamic program analysis.
class ExecutionControl {
 public:
  ExecutionControl();
  virtual ~ExecutionControl() {}

  void Initialize();
  void PreSetup();
  void PostSetup();
  void InstrumentTrace(TRACE trace, VOID *v);
  void ImageLoad(IMG img, VOID *v);
  void ImageUnload(IMG img, VOID *v);
  void SyscallEntry(THREADID tid, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v);
  void SyscallExit(THREADID tid, CONTEXT *ctxt, SYSCALL_STANDARD std, VOID *v);
  void IntSignal(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler,
                 const EXCEPTION_INFO *pExceptInfo, VOID *v);
  void ContextChange(THREADID tid, CONTEXT_CHANGE_REASON reason,
                     const CONTEXT *from, CONTEXT *to, INT32 info, VOID *v);
  void ProgramStart();
  void ProgramExit(INT32 code, VOID *v);
  void ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v);
  void ThreadExit(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v);

 protected:
  typedef std::list<Analyzer *> AnalyzerContainer;

  virtual Mutex *CreateMutex() { return new PinMutex; }

  virtual Semaphore *CreateSemaphore(unsigned int value) {
    return new SysSemaphore(value);
  }

  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual bool HandleIgnoreInstCount(IMG img) { return false; }
  virtual bool HandleIgnoreMemAccess(IMG img) { return false; }
  virtual void HandlePreInstrumentTrace(TRACE trace);
  virtual void HandlePostInstrumentTrace(TRACE trace);
  virtual void HandleProgramStart();
  virtual void HandleProgramExit();
  virtual void HandleImageLoad(IMG img, Image *image);
  virtual void HandleImageUnload(IMG img, Image *image);
  virtual void HandleSyscallEntry(THREADID tid, CONTEXT *ctxt,
                                  SYSCALL_STANDARD std);
  virtual void HandleSyscallExit(THREADID tid, CONTEXT *ctxt,
                                 SYSCALL_STANDARD std);
  virtual void HandleSignalReceived(THREADID tid, INT32 sig,
                                    const CONTEXT *ctxt_from, CONTEXT *ctxt_to);
  virtual void HandleThreadStart();
  virtual void HandleThreadExit();
  virtual void HandleMain(THREADID tid, CONTEXT *ctxt);
  virtual void HandleThreadMain(THREADID tid, CONTEXT *ctxt);
  virtual void HandleBeforeMemRead(THREADID tid, Inst *inst, address_t addr,
                                   size_t size);
  virtual void HandleAfterMemRead(THREADID tid, Inst *inst, address_t addr,
                                  size_t size);
  virtual void HandleBeforeMemWrite(THREADID tid, Inst *inst, address_t addr,
                                    size_t size);
  virtual void HandleAfterMemWrite(THREADID tid, Inst *inst, address_t addr,
                                   size_t size);
  virtual void HandleBeforeAtomicInst(THREADID tid, Inst *inst, OPCODE opcode,
                                      address_t addr);
  virtual void HandleAfterAtomicInst(THREADID tid, Inst *inst, OPCODE opcode,
                                     address_t addr);
  virtual void HandlePthreadCreate(PthreadCreateContext *context);
  virtual void HandlePthreadJoin(PthreadJoinContext *context);
  virtual void HandlePthreadMutexTryLock(PthreadMutexTryLockContext *context);
  virtual void HandlePthreadMutexLock(PthreadMutexLockContext *context);
  virtual void HandlePthreadMutexUnlock(PthreadMutexUnlockContext *context);
  virtual void HandlePthreadCondSignal(PthreadCondSignalContext *context);
  virtual void HandlePthreadCondBroadcast(PthreadCondBroadcastContext *context);
  virtual void HandlePthreadCondWait(PthreadCondWaitContext *context);
  virtual void HandlePthreadCondTimedwait(PthreadCondTimedwaitContext *context);
  virtual void HandlePthreadBarrierInit(PthreadBarrierInitContext *context);
  virtual void HandlePthreadBarrierWait(PthreadBarrierWaitContext *context);
  virtual void HandleSleep(SleepContext *context);
  virtual void HandleUsleep(UsleepContext *context);
  virtual void HandleSchedYield(SchedYieldContext *context);
  virtual void HandleMalloc(MallocContext *context);
  virtual void HandleCalloc(CallocContext *context);
  virtual void HandleRealloc(ReallocContext *context);
  virtual void HandleFree(FreeContext *context);
  virtual void HandleValloc(VallocContext *context);

  void LockKernel() { kernel_lock_->Lock(); }
  void UnlockKernel() { kernel_lock_->Unlock(); }
  void Abort(const std::string &msg);
  Inst *GetInst(ADDRINT pc);
  void UpdateInstOpcode(Inst *inst, INS ins);
  void UpdateInstDebugInfo(Inst *inst, ADDRINT pc);
  void AddAnalyzer(Analyzer *analyzer);
  thread_id_t GetThdID(pthread_t thread);
  thread_id_t GetParent();
  thread_id_t Self() { return PIN_ThreadUid(); }
  timestamp_t GetThdClk(THREADID tid) { return tls_thd_clock_[tid]; }
  thread_id_t WaitForNewChild(PthreadCreateContext *context);
  void ReplacePthreadCreateWrapper(IMG img);
  void ReplacePthreadWrappers(IMG img);
  void ReplaceYieldWrappers(IMG img);
  void ReplaceMallocWrappers(IMG img);

  Mutex *kernel_lock_;
  Knob *knob_;
  Descriptor desc_;
  LogFile *debug_file_;
  StaticInfo *sinfo_;
  AnalyzerContainer analyzers_;
  DebugAnalyzer *debug_analyzer_;
  volatile bool main_thread_started_;
  timestamp_t tls_thd_clock_[PIN_MAX_THREADS];
  address_t tls_read_addr_[PIN_MAX_THREADS];
  size_t tls_read_size_[PIN_MAX_THREADS];
  address_t tls_write_addr_[PIN_MAX_THREADS];
  size_t tls_write_size_[PIN_MAX_THREADS];
  address_t tls_read2_addr_[PIN_MAX_THREADS];
  address_t tls_atomic_addr_[PIN_MAX_THREADS];
  int tls_syscall_num_[PIN_MAX_THREADS];
  std::map<OS_THREAD_ID, Semaphore *> thd_create_sem_map_; // init = 0
  std::map<OS_THREAD_ID, thread_id_t> child_thd_map_;
  std::map<OS_THREAD_ID, thread_id_t> os_tid_map_;
  std::map<pthread_t, thread_id_t> pthread_handle_map_;
  thread_id_t main_thd_id_;

  static ExecutionControl *ctrl_;

 private:
  void InstrumentStartupFunc(IMG img);

  static void PIN_FAST_ANALYSIS_CALL __InstCount(THREADID tid);
  static void PIN_FAST_ANALYSIS_CALL __InstCount2(THREADID tid, UINT32 c);
  static void __Main(THREADID tid, CONTEXT *ctxt);
  static void __ThreadMain(THREADID tid, CONTEXT *ctxt);
  static void __BeforeMemRead(THREADID tid, Inst *inst, ADDRINT addr,
                              UINT32 size);
  static void __AfterMemRead(THREADID tid, Inst *inst);
  static void __BeforeMemWrite(THREADID tid, Inst *inst, ADDRINT addr,
                               UINT32 size);
  static void __AfterMemWrite(THREADID tid, Inst *inst);
  static void __BeforeMemRead2(THREADID tid, Inst *inst, ADDRINT addr,
                               UINT32 size);
  static void __AfterMemRead2(THREADID tid, Inst *inst);
  static void __BeforeAtomicInst(THREADID tid, Inst *inst, UINT32 opcode,
                                 ADDRINT addr);
  static void __AfterAtomicInst(THREADID tid, Inst *inst, UINT32 opcode);
  // pthread wrappers
  static void __PthreadCreate(PthreadCreateContext *context);
  static void __PthreadJoin(PthreadJoinContext *context);
  static void __PthreadMutexTryLock(PthreadMutexTryLockContext *context);
  static void __PthreadMutexLock(PthreadMutexLockContext *context);
  static void __PthreadMutexUnlock(PthreadMutexUnlockContext *context);
  static void __PthreadCondSignal(PthreadCondSignalContext *context);
  static void __PthreadCondBroadcast(PthreadCondBroadcastContext *context);
  static void __PthreadCondWait(PthreadCondWaitContext *context);
  static void __PthreadCondTimedwait(PthreadCondTimedwaitContext *context);
  static void __PthreadBarrierInit(PthreadBarrierInitContext *context);
  static void __PthreadBarrierWait(PthreadBarrierWaitContext *context);
  // yield wrappers
  static void __Sleep(SleepContext *context);
  static void __Usleep(UsleepContext *context);
  static void __SchedYield(SchedYieldContext *context);
  // malloc wrappers
  static void __Malloc(MallocContext *context);
  static void __Calloc(CallocContext *context);
  static void __Realloc(ReallocContext *context);
  static void __Free(FreeContext *context);
  static void __Valloc(VallocContext *context);

  DISALLOW_COPY_CONSTRUCTORS(ExecutionControl);
};

#endif

