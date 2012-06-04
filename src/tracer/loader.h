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

// File: tracer/loader.h - Define trace loader.

#ifndef TRACER_LOADER_H_
#define TRACER_LOADER_H_

#include <list>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/knob.h"
#include "core/logging.h"
#include "core/offline_tool.h"
#include "core/descriptor.h"
#include "core/analyzer.h"
#include "core/debug_analyzer.h"
#include "tracer/log.h"

namespace tracer {

class Loader : public OfflineTool {
 public:
  Loader();
  virtual ~Loader() {}

 protected:
  typedef std::list<Analyzer *> AnalyzerContainer;

  virtual void HandlePreSetup();
  virtual void HandlePostSetup();
  virtual void HandleStart();
  virtual void HandleProgramStart(LogEntry *e);
  virtual void HandleProgramExit(LogEntry *e);
  virtual void HandleImageLoad(LogEntry *e);
  virtual void HandleImageUnload(LogEntry *e);
  virtual void HandleSyscallEntry(LogEntry *e);
  virtual void HandleSyscallExit(LogEntry *e);
  virtual void HandleSignalReceived(LogEntry *e);
  virtual void HandleThreadStart(LogEntry *e);
  virtual void HandleThreadExit(LogEntry *e);
  virtual void HandleMain(LogEntry *e);
  virtual void HandleThreadMain(LogEntry *e);
  virtual void HandleBeforeMemRead(LogEntry *e);
  virtual void HandleAfterMemRead(LogEntry *e);
  virtual void HandleBeforeMemWrite(LogEntry *e);
  virtual void HandleAfterMemWrite(LogEntry *e);
  virtual void HandleBeforeAtomicInst(LogEntry *e);
  virtual void HandleAfterAtomicInst(LogEntry *e);
  virtual void HandleBeforePthreadCreate(LogEntry *e);
  virtual void HandleAfterPthreadCreate(LogEntry *e);
  virtual void HandleBeforePthreadJoin(LogEntry *e);
  virtual void HandleAfterPthreadJoin(LogEntry *e);
  virtual void HandleBeforePthreadMutexTryLock(LogEntry *e);
  virtual void HandleAfterPthreadMutexTryLock(LogEntry *e);
  virtual void HandleBeforePthreadMutexLock(LogEntry *e);
  virtual void HandleAfterPthreadMutexLock(LogEntry *e);
  virtual void HandleBeforePthreadMutexUnlock(LogEntry *e);
  virtual void HandleAfterPthreadMutexUnlock(LogEntry *e);
  virtual void HandleBeforePthreadCondSignal(LogEntry *e);
  virtual void HandleAfterPthreadCondSignal(LogEntry *e);
  virtual void HandleBeforePthreadCondBroadcast(LogEntry *e);
  virtual void HandleAfterPthreadCondBroadcast(LogEntry *e);
  virtual void HandleBeforePthreadCondWait(LogEntry *e);
  virtual void HandleAfterPthreadCondWait(LogEntry *e);
  virtual void HandleBeforePthreadCondTimedwait(LogEntry *e);
  virtual void HandleAfterPthreadCondTimedwait(LogEntry *e);
  virtual void HandleBeforePthreadBarrierInit(LogEntry *e);
  virtual void HandleAfterPthreadBarrierInit(LogEntry *e);
  virtual void HandleBeforePthreadBarrierWait(LogEntry *e);
  virtual void HandleAfterPthreadBarrierWait(LogEntry *e);
  virtual void HandleBeforeMalloc(LogEntry *e);
  virtual void HandleAfterMalloc(LogEntry *e);
  virtual void HandleBeforeCalloc(LogEntry *e);
  virtual void HandleAfterCalloc(LogEntry *e);
  virtual void HandleBeforeRealloc(LogEntry *e);
  virtual void HandleAfterRealloc(LogEntry *e);
  virtual void HandleBeforeFree(LogEntry *e);
  virtual void HandleAfterFree(LogEntry *e);
  virtual void HandleBeforeValloc(LogEntry *e);
  virtual void HandleAfterValloc(LogEntry *e);

  void EventLoop();
  void HandleEvent(LogEntry *e);
  void AddAnalyzer(Analyzer *analyzer);

  TraceLog *trace_log_;
  AnalyzerContainer analyzers_;
  Descriptor desc_;
  DebugAnalyzer *debug_analyzer_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Loader);
};

} // namespace tracer

#endif

