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

// File: delta/scheduler_keep_main.cpp - The main entrance of the reorder scheduler.

#include "delta/scheduler_keep.hpp"

#include <csignal>

static delta::SchedulerKeep *sched_ctrl = new delta::SchedulerKeep;

static VOID I_InstrumentTrace(TRACE trace, VOID *v) {
  sched_ctrl->InstrumentTrace(trace, v);
}

static VOID I_ImageLoad(IMG img, VOID *v) {
  sched_ctrl->ImageLoad(img, v);
}

static VOID I_ImageUnload(IMG img, VOID *v) {
  sched_ctrl->ImageUnload(img, v);
}

static VOID I_SyscallEntry(THREADID tid, CONTEXT *ctxt, SYSCALL_STANDARD std,
                           VOID *v) {
  sched_ctrl->SyscallEntry(tid, ctxt, std, v);
}

static VOID I_SyscallExit(THREADID tid, CONTEXT *ctxt, SYSCALL_STANDARD std,
                          VOID *v) {
  sched_ctrl->SyscallExit(tid, ctxt, std, v);
}

static BOOL I_IntSignal(THREADID tid, INT32 sig, CONTEXT *ctxt, BOOL hasHandler,
                        const EXCEPTION_INFO *pExceptInfo, VOID *v) {
  sched_ctrl->IntSignal(tid, sig, ctxt, hasHandler, pExceptInfo, v);
  return FALSE;
}

static VOID I_ContextChange(THREADID tid, CONTEXT_CHANGE_REASON reason,
                            const CONTEXT *from, CONTEXT *to, INT32 info,
                            VOID *v) {
  sched_ctrl->ContextChange(tid, reason, from, to, info, v);
}

static VOID I_ProgramStart() {
  sched_ctrl->ProgramStart();
}

static VOID I_ProgramExit(INT32 code, VOID *v) {
  sched_ctrl->ProgramExit(code, v);
}

static VOID I_ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  sched_ctrl->ThreadStart(tid, ctxt, flags, v);
}

static VOID I_ThreadExit(THREADID tid, const CONTEXT *ctxt, INT32 code,
                         VOID *v) {
  sched_ctrl->ThreadExit(tid, ctxt, code, v);
}

int main(int argc, char *argv[]) {
  sched_ctrl->Initialize();
  sched_ctrl->PreSetup();
  PIN_InitSymbols();
  PIN_Init(argc, argv);
  sched_ctrl->PostSetup();
  TRACE_AddInstrumentFunction(I_InstrumentTrace, NULL);
  IMG_AddInstrumentFunction(I_ImageLoad, NULL);
  IMG_AddUnloadFunction(I_ImageUnload, NULL);
  PIN_AddSyscallEntryFunction(I_SyscallEntry, NULL);
  PIN_AddSyscallExitFunction(I_SyscallExit, NULL);
  PIN_InterceptSignal(SIGUSR2, I_IntSignal, NULL);
  PIN_AddContextChangeFunction(I_ContextChange, NULL);
  PIN_AddFiniFunction(I_ProgramExit, NULL);
  PIN_AddThreadStartFunction(I_ThreadStart, NULL);
  PIN_AddThreadFiniFunction(I_ThreadExit, NULL);
  I_ProgramStart();
  // start the program
  PIN_StartProgram();
}

