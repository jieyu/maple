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

// File: idiom/predictor_new.cc - Implementation of the new iRoot
// predictor analyzer.

#include "idiom/predictor_new.h"

#include <cassert>
#include <cstdarg>
#include <csignal>
#include <sys/syscall.h>
#include "core/logging.h"
#include "core/stat.h"

namespace idiom {

// each thread has a buffer of 10M bytes
#define META_QUEUE_LIMIT (64 * 1024 * 10)

// public methods

PredictorNew::PredictorNew()
    : internal_lock_(NULL),
      sinfo_(NULL),
      iroot_db_(NULL),
      memo_(NULL),
      sinst_db_(NULL),
      sync_only_(false),
      complex_idioms_(false),
      single_var_idioms_(false),
      racy_only_(false),
      predict_deadlock_(false),
      unit_size_(4),
      vw_(1000),
      filter_(NULL) {
  // empty
}

PredictorNew::~PredictorNew() {
  // empty
}

void PredictorNew::Register() {
  knob_->RegisterBool("enable_predictor_new", "whether enable the iroot predictor (NEW)", "0");
  knob_->RegisterBool("sync_only", "whether only monitor synchronization accesses", "0");
  knob_->RegisterBool("complex_idioms", "whether target complex idioms", "0");
  knob_->RegisterBool("single_var_idioms", "whether only consider single variable idioms", "0");
  knob_->RegisterBool("racy_only", "whether only consider sync and racy memory dependencies", "0");
  knob_->RegisterBool("predict_deadlock", "whether predict and trigger deadlocks (experimental)", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("vw", "the vulnerability window (# dynamic inst)", "1000");
}

bool PredictorNew::Enabled() {
  return knob_->ValueBool("enable_predictor_new");
}

void PredictorNew::Setup(Mutex *lock,
                         StaticInfo *sinfo,
                         iRootDB *iroot_db,
                         Memo *memo,
                         sinst::SharedInstDB *sinst_db) {
  // link common databases
  internal_lock_ = lock;
  sinfo_ = sinfo;
  iroot_db_ = iroot_db;
  memo_ = memo;
  sinst_db_ = sinst_db;

  // read settings and flags
  sync_only_ = knob_->ValueBool("sync_only");
  complex_idioms_ = knob_->ValueBool("complex_idioms");
  single_var_idioms_ = knob_->ValueBool("single_var_idioms");
  racy_only_ = knob_->ValueBool("racy_only");
  predict_deadlock_ = knob_->ValueBool("predict_deadlock");
  unit_size_ = knob_->ValueInt("unit_size");
  vw_ = knob_->ValueInt("vw");

  // init global analysis state
  filter_ = new RegionFilter(internal_lock_->Clone());
  InitConflictTable();

  // setup analysis descriptor
  if (!sync_only_) {
    desc_.SetHookBeforeMem();
  }
  desc_.SetHookSyscall();
  desc_.SetHookSignal();
  desc_.SetHookAtomicInst();
  desc_.SetHookPthreadFunc();
  desc_.SetHookMallocFunc();
  desc_.SetTrackInstCount();
}

void PredictorNew::ProgramExit() {
  // process free for all the remaining meta (user forgot to call free)
  for (Meta::Table::iterator it = meta_table_.begin();
       it != meta_table_.end(); ++it) {
    ProcessFree(it->second);
  }
  // predict iroots
  PredictiRoot();
  if (complex_idioms_) {
    PredictComplexiRoot();
  }
}

void PredictorNew::ImageLoad(Image *image,
                             address_t low_addr,
                             address_t high_addr,
                             address_t data_start,
                             size_t data_size,
                             address_t bss_start,
                             size_t bss_size) {
  DEBUG_ASSERT(low_addr && high_addr && high_addr > low_addr);
  if (data_start) {
    DEBUG_ASSERT(data_size);
    AllocAddrRegion(data_start, data_size);
  }
  if (bss_start) {
    DEBUG_ASSERT(bss_size);
    AllocAddrRegion(bss_start, bss_size);
  }
}

void PredictorNew::ImageUnload(Image *image,
                               address_t low_addr,
                               address_t high_addr,
                               address_t data_start,
                               size_t data_size,
                               address_t bss_start,
                               size_t bss_size) {
  DEBUG_ASSERT(low_addr);
  if (data_start)
    FreeAddrRegion(data_start);
  if (bss_start)
    FreeAddrRegion(bss_start);
}

void PredictorNew::SyscallEntry(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk,
                                int syscall_num) {
  ScopedLock locker(internal_lock_);
  switch (syscall_num) {
    case SYS_accept:
    case SYS_select:
    case SYS_pselect6:
    case SYS_rt_sigtimedwait:
      if (async_start_time_map_.find(curr_thd_id)
              == async_start_time_map_.end()) {
        async_start_time_map_[curr_thd_id] = curr_thd_clk;
      }
      break;
    default:
      break;
  }
}

void PredictorNew::SignalReceived(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk,
                                  int signal_num) {
  switch (signal_num) {
    case SIGINT:
    case SIGALRM:
      if (async_start_time_map_.find(curr_thd_id)
              == async_start_time_map_.end()) {
        async_start_time_map_[curr_thd_id] = curr_thd_clk;
      }
      break;
    default:
      break;
  }
}

void PredictorNew::ThreadStart(thread_id_t curr_thd_id,
                               thread_id_t parent_thd_id) {

  // create thread local vector clock and lockset
  VectorClock *curr_vc = new VectorClock;
  LockSet *curr_ls = new LockSet;

  ScopedLock locker(internal_lock_);
  // init vector clock
  curr_vc->Increment(curr_thd_id);
  if (parent_thd_id != INVALID_THD_ID) {
    // this is not the main thread
    VectorClock *parent_vc = curr_vc_map_[parent_thd_id];
    DEBUG_ASSERT(parent_vc);
    curr_vc->Join(parent_vc);
    parent_vc->Increment(parent_thd_id);
  }
  curr_vc_map_[curr_thd_id] = curr_vc;
  // init lock set
  curr_ls_map_[curr_thd_id] = curr_ls;
}

void PredictorNew::ThreadExit(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk) {
  ScopedLock locker(internal_lock_);
  ProcessThreadExit(curr_thd_id);
}

void PredictorNew::BeforeMemRead(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk,
                                 Inst *inst,
                                 address_t addr,
                                 size_t size) {
    
  // XXX: this function needs to be implemented as fast as possible
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize addresses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // whether this access to iaddr is a shared access
    bool shared_access = false;
    // check shared for iaddr
    SharedMeta::Table::iterator sit = shared_meta_table_.find(iaddr);
    if (sit == shared_meta_table_.end()) {
      // no shared info
      SharedMeta &shared_meta = shared_meta_table_[iaddr];
      if (sinst_db_->Shared(inst)) {
        shared_meta.shared = true;
        shared_access = true;
      } else {
        shared_meta.last_thd_id = curr_thd_id;
        shared_meta.first_inst = inst;
      }
    } else {
      // shared info exists
      SharedMeta &shared_meta = sit->second;
      if (shared_meta.shared) {
        sinst_db_->SetShared(inst);
        shared_access = true;
      } else {
        // meta is not currently shared
        if (sinst_db_->Shared(inst)) {
          shared_meta.shared = true;
          sinst_db_->SetShared(shared_meta.first_inst);
          shared_access = true;
        } else {
          if (curr_thd_id != shared_meta.last_thd_id) {
            if (shared_meta.has_write) {
              shared_meta.shared = true;
              sinst_db_->SetShared(inst);
              sinst_db_->SetShared(shared_meta.first_inst);
              shared_access = true;
            } else {
              shared_meta.multi_read = true;
              shared_meta.last_thd_id = curr_thd_id;
            }
          }
        }
      } // end of else meta shared
    } // end of else meta not exist

    // actuall processing
    if (shared_access) {
      Meta *meta = GetMemMeta(iaddr);
      if (!meta)
        continue; // acecss to sync variable, ignore
      ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                        IROOT_EVENT_MEM_READ, inst, meta);
    }
  } // end of for each iaddr
}

void PredictorNew::BeforeMemWrite(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk,
                                  Inst *inst,
                                  address_t addr,
                                  size_t size) {
  // XXX: this function needs to be implemented as fast as possible
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize addresses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // whether this access to iaddr is a shared access
    bool shared_access = false;
    // check shared for iaddr
    SharedMeta::Table::iterator sit = shared_meta_table_.find(iaddr);
    if (sit == shared_meta_table_.end()) {
      // no shared info
      SharedMeta &shared_meta = shared_meta_table_[iaddr];
      if (sinst_db_->Shared(inst)) {
        shared_meta.shared = true;
        shared_access = true;
      } else {
        shared_meta.has_write = true;
        shared_meta.last_thd_id = curr_thd_id;
        shared_meta.first_inst = inst;
      }
    } else {
      // shared info exists
      SharedMeta &shared_meta = sit->second;
      if (shared_meta.shared) {
        sinst_db_->SetShared(inst);
        shared_access = true;
      } else {
        // meta is not currently shared
        if (sinst_db_->Shared(inst)) {
          shared_meta.shared = true;
          sinst_db_->SetShared(shared_meta.first_inst);
          shared_access = true;
        } else {
          shared_meta.has_write = true;
          if (curr_thd_id != shared_meta.last_thd_id || shared_meta.multi_read){
            shared_meta.shared = true;
            sinst_db_->SetShared(inst);
            sinst_db_->SetShared(shared_meta.first_inst);
            shared_access = true;
          }
        }
      } // end of else meta shared
    } // end of else meta not exist

    // actuall processing
    if (shared_access) {
      Meta *meta = GetMemMeta(iaddr);
      if (!meta)
        continue; // acecss to sync variable, ignore
      ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                        IROOT_EVENT_MEM_WRITE, inst, meta);
    }
  } // end of for each iaddr
}

void PredictorNew::BeforeAtomicInst(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk,
                                    Inst *inst,
                                    std::string type,
                                    address_t addr) {
  ScopedLock locker(internal_lock_);
  atomic_inst_set_.insert(inst);
  // use heuristics to find locks and unlocks in libc library
  // the main idea is to identify special lock prefixed instructions
  if (inst->image()->IsLibc() && type.compare("DEC") == 0) {
    LockSet *curr_ls = curr_ls_map_[curr_thd_id];
    DEBUG_ASSERT(curr_ls);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
    curr_ls->Remove(addr);
  }
}

void PredictorNew::AfterAtomicInst(thread_id_t curr_thd_id,
                                   timestamp_t curr_thd_clk,
                                   Inst *inst,
                                   std::string type,
                                   address_t addr) {
  ScopedLock locker(internal_lock_);
  // use heuristics to find locks and unlocks in libc library
  // the main idea is to identify special lock prefixed instructions
  if (inst->image()->IsLibc() && type.compare("CMPXCHG") == 0) {
    LockSet *curr_ls = curr_ls_map_[curr_thd_id];
    DEBUG_ASSERT(curr_ls);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
    curr_ls->Add(addr);
  }
}

void PredictorNew::AfterPthreadJoin(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk,
                                    Inst *inst,
                                    thread_id_t child_thd_id) {
  ScopedLock locker(internal_lock_);
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  VectorClock *child_vc = curr_vc_map_[child_thd_id];
  curr_vc->Join(child_vc);
}

void PredictorNew::AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk,
                                         Inst *inst,
                                         address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  Meta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Add(addr);
}

void PredictorNew::BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                            timestamp_t curr_thd_clk,
                                            Inst *inst,
                                            address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  Meta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Remove(addr);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, meta);
}

void PredictorNew::BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk,
                                           Inst *inst,
                                           address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  CondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessSignal(curr_thd_id, meta);
}

void PredictorNew::BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                              timestamp_t curr_thd_clk,
                                              Inst *inst,
                                              address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  CondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessBroadcast(curr_thd_id, meta);
}

void PredictorNew::BeforePthreadCondWait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk,
                                         Inst *inst,
                                         address_t cond_addr,
                                         address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  Meta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Remove(mutex_addr);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, mutex_meta);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPreWait(curr_thd_id, cond_meta, false);
}

void PredictorNew::AfterPthreadCondWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk,
                                        Inst *inst,
                                        address_t cond_addr,
                                        address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPostWait(curr_thd_id, cond_meta);
  // lock
  Meta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, mutex_meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Add(mutex_addr);
}

void PredictorNew::BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                              timestamp_t curr_thd_clk,
                                              Inst *inst,
                                              address_t cond_addr,
                                              address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  Meta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Remove(mutex_addr);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, mutex_meta);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPreWait(curr_thd_id, cond_meta, true);
}

void PredictorNew::AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                             timestamp_t curr_thd_clk,
                                             Inst *inst,
                                             address_t cond_addr,
                                             address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPostWait(curr_thd_id, cond_meta);
  // lock
  Meta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, mutex_meta);
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);
  curr_ls->Add(mutex_addr);
}

void PredictorNew::BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                            timestamp_t curr_thd_clk,
                                            Inst *inst,
                                            address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  BarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessPreBarrier(curr_thd_id, meta);
}

void PredictorNew::AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk,
                                           Inst *inst,
                                           address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  BarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessPostBarrier(curr_thd_id, meta);
}

void PredictorNew::AfterMalloc(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst,
                               size_t size,
                               address_t addr) {
  AllocAddrRegion(addr, size);
}

void PredictorNew::AfterCalloc(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst,
                               size_t nmemb,
                               size_t size,
                               address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void PredictorNew::BeforeRealloc(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk,
                                 Inst *inst,
                                 address_t ori_addr,
                                 size_t size) {
  FreeAddrRegion(ori_addr);
}

void PredictorNew::AfterRealloc(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk,
                                Inst *inst,
                                address_t ori_addr,
                                size_t size,
                                address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void PredictorNew::BeforeFree(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk,
                              Inst *inst,
                              address_t addr) {
  FreeAddrRegion(addr);
}

void PredictorNew::AfterValloc(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst,
                               size_t size,
                               address_t addr) {
  AllocAddrRegion(addr, size);
}

// protected internal methods

size_t PredictorNew::Hash(VectorClock *vc) {
  size_t hash_val = 0;
  for (vc->IterBegin(); !vc->IterEnd(); vc->IterNext()) {
    hash_val += vc->IterCurrThd();
    hash_val += vc->IterCurrClk();
  }
  return hash_val;
}

size_t PredictorNew::Hash(FLockSet *fls) {
  size_t hash_val = 0;
  for (FLockSet::LockFlagTable::iterator it = fls->lock_flag_table.begin();
       it != fls->lock_flag_table.end(); ++it) {
    hash_val += it->first;
    hash_val += it->second.first;
    hash_val += it->second.second;
  }
  return hash_val;
}

size_t PredictorNew::Hash(AccSum *acc_sum) {
  size_t hash_val = 0;
  hash_val += Hash(acc_sum->meta);
  hash_val += acc_sum->thd_id;
  hash_val += Hash(acc_sum->type);
  hash_val += Hash(acc_sum->inst);
  hash_val += Hash(&acc_sum->fls);
  return hash_val;
}

size_t PredictorNew::Hash(DynAcc *dyn_acc) {
  size_t hash_val = 0;
  hash_val += Hash(dyn_acc->meta);
  hash_val += dyn_acc->thd_id;
  hash_val += Hash(dyn_acc->type);
  hash_val += Hash(dyn_acc->inst);
  hash_val += Hash(&dyn_acc->fls);
  return hash_val;
}

bool PredictorNew::Match(FLockSet *fls1, FLockSet *fls2) {
  // check size first
  if (fls1->lock_flag_table.size() != fls2->lock_flag_table.size())
    return false;
  // check each element matches
  for (FLockSet::LockFlagTable::iterator it1 = fls1->lock_flag_table.begin();
       it1 != fls1->lock_flag_table.end(); ++it1) {
    FLockSet::LockFlagTable::iterator it2
        = fls2->lock_flag_table.find(it1->first);
    if (it2 == fls2->lock_flag_table.end())
      return false;
    if (it2->second.first != it1->second.first ||
        it2->second.second != it1->second.second)
      return false;
  }
  return true;
}

void PredictorNew::AddThdClk(ThdClkInfo *info, timestamp_t thd_clk) {
  // add a thread local clock to the thread clock set
  if (info->start == INVALID_TIMESTAMP)
    info->start = thd_clk;
  info->end = thd_clk;
}

void PredictorNew::ClearFlag(FLockSet *fls) {
  // clear the flagged lock set
  fls->lock_flag_table.clear();
}

void PredictorNew::UpdateFirstFlag(FLockSet *fls,
                                   LockSet *last_ls,
                                   LockSet *curr_ls) {
  // update the first flags in fls according to last_ls and curr_ls
  // last_ls could be NULL
  DEBUG_ASSERT(fls && curr_ls);
  // find those locks that are in curr_ls but not in last_ls
  // make sure to check version number also
  for (curr_ls->IterBegin(); !curr_ls->IterEnd(); curr_ls->IterNext()) {
    address_t addr = curr_ls->IterCurrAddr();
    LockSet::lock_version_t version = curr_ls->IterCurrVersion();
    FLockSet::Flag &flag = fls->lock_flag_table[addr];
    if (!last_ls || !last_ls->Exist(addr, version))
      flag.first = true; // first flag
    flag.second = false; // last flag
  }
}

void PredictorNew::UpdateLastFlag(FLockSet *fls,
                                  LockSet *last_ls,
                                  LockSet *curr_ls) {
  // update the last flags in fls according to last_ls and curr_ls
  // curr_ls could be NULL
  DEBUG_ASSERT(fls && last_ls);
  // find those locks that are in last_ls but not in curr_ls
  // make sure to check version number also
  for (last_ls->IterBegin(); !last_ls->IterEnd(); last_ls->IterNext()) {
    address_t addr = last_ls->IterCurrAddr();
    LockSet::lock_version_t version = last_ls->IterCurrVersion();
    FLockSet::Flag &flag = fls->lock_flag_table[addr];
    if (!curr_ls || !curr_ls->Exist(addr, version))
      flag.second = true;
  }
}

void PredictorNew::CommonLockSet(FLockSet *fls,
                                 LockSet *prev_ls,
                                 LockSet *curr_ls) {
  for (prev_ls->IterBegin(); !prev_ls->IterEnd(); prev_ls->IterNext()) {
    address_t addr = prev_ls->IterCurrAddr();
    LockSet::lock_version_t version = prev_ls->IterCurrVersion();
    if (curr_ls->Exist(addr, version)) {
      fls->lock_flag_table[addr] = FLockSet::Flag(false, false);
    }
  }
}

bool PredictorNew::ExistAccSumPair(AccSum *src, AccSum *dst) {
  // check whether src->dst exists
  AccSum::PairIndex::iterator iit = acc_sum_succ_index_.find(src);
  if (iit != acc_sum_succ_index_.end()) {
    for (AccSum::Vec::iterator vit = iit->second.begin();
         vit != iit->second.end(); ++vit) {
      if (*vit == dst)
        return true;
    }
  }
  return false;
}

void PredictorNew::AddAccSumPair(AccSum *src, AccSum *dst) {

    // only add the count pair for mem accsum
    if ((src->type == IROOT_EVENT_MEM_READ || src->type == IROOT_EVENT_MEM_WRITE) && 
        (dst->type == IROOT_EVENT_MEM_READ || dst->type == IROOT_EVENT_MEM_WRITE)) {
        
        int src_count =  getNumAcc(src);
        
        int dst_count = getNumAcc(dst);
        
        std::pair <AccSum*, AccSum*>* acc_sum_pair =  new std::pair <AccSum*, AccSum*> (src, dst);

        std::pair <int, int>* count_pair = new std::pair <int, int> (src_count, dst_count);

        iroot_inst_count_map[acc_sum_pair] = count_pair;
    }

  // add src->dst (precondition: no duplication)
  acc_sum_succ_index_[src].push_back(dst);
  acc_sum_pred_index_[dst].push_back(src);
  DEBUG_STAT_INC("pd_acc_sum_deps", 1);
}

PredictorNew::AccSum *PredictorNew::MatchAccSum(DynAcc *dyn_acc) {
  // find a matching access summary, return NULL if not found
  size_t hash_val = Hash(dyn_acc);
  AccSum::HashIndex::iterator iit = acc_sum_hash_index_.find(hash_val);
  if (iit != acc_sum_hash_index_.end()) {
    for (AccSum::Vec::iterator vit = iit->second.begin();
         vit != iit->second.end(); ++vit) {
      AccSum *acc_sum = *vit;
      if (acc_sum->meta == dyn_acc->meta &&
          acc_sum->thd_id == dyn_acc->thd_id &&
          acc_sum->type == dyn_acc->type &&
          acc_sum->inst == dyn_acc->inst &&
          Match(&acc_sum->fls, &dyn_acc->fls)) {
        return acc_sum;
      }
    }
  }
  return NULL;
}

bool PredictorNew::CheckConflict(iRootEventType src, iRootEventType dst) {
  // return true if src->dst is a valid dependency
  // just need to check the conflict table
  return conflict_table_[src][dst];
}

bool PredictorNew::CheckMutexExclution(FLockSet *src_fls, FLockSet *dst_fls) {
  // return true if mutex exclution check succeeds
  for (FLockSet::LockFlagTable::iterator src_it =
          src_fls->lock_flag_table.begin();
       src_it != src_fls->lock_flag_table.end(); ++src_it) {
    FLockSet::LockFlagTable::iterator dst_it
        = dst_fls->lock_flag_table.find(src_it->first);
    if (dst_it != dst_fls->lock_flag_table.end()) {
      // common lock found, check flags
      if (!src_it->second.second || !dst_it->second.first) {
        // src is not the last in the critical section
        // dst is not the first in the critical section
        return false;
      }
    }
  }
  return true;
}

bool PredictorNew::CheckConcurrent(VectorClock *vc, AccSum *rmt) {
  // return true if there exists at least one pair such that they
  // are concurrent according to non-mutex happens before graph
  // XXX: we should use binary search here
  for (AccSum::TimeInfo::reverse_iterator it = rmt->tinfo.rbegin();
       it != rmt->tinfo.rend(); ++it) {
    if (it->first.HappensBefore(vc))
      return false;
    if (!vc->HappensBefore(&it->first))
      return true;
  }
  return false;
}

bool PredictorNew::CheckAsync(AccSum *acc_sum) {
  // check whether the access summary depends on asynchronous events
  std::map<thread_id_t, timestamp_t>::iterator it
      = async_start_time_map_.find(acc_sum->thd_id);
  if (it == async_start_time_map_.end()) {
    return false;
  } else {
    timestamp_t start = it->second;
    DEBUG_ASSERT(!acc_sum->tinfo.empty());
    return GetLargestThdClk(&acc_sum->tinfo.back().second) > start;
  }
}

bool PredictorNew::CheckAtomic(AccSum *src, AccSum *dst) {
  // return true if src->dst is not constrained by atomic instructions
  if (atomic_inst_set_.find(src->inst) != atomic_inst_set_.end()) {
    if (src->type == IROOT_EVENT_MEM_READ)
      return false;
  }
  if (atomic_inst_set_.find(dst->inst) != atomic_inst_set_.end()) {
    if (dst->type == IROOT_EVENT_MEM_WRITE)
      return false;
  }
  return true;
}

void PredictorNew::InitConflictTable() {
  for (int i = 0; i < IROOT_EVENT_TYPE_ARRAYSIZE; i++) {
    for (int j = 0; j < IROOT_EVENT_TYPE_ARRAYSIZE; j++) {
      conflict_table_[i][j] = false;
    }
  }
  conflict_table_[IROOT_EVENT_MEM_READ][IROOT_EVENT_MEM_WRITE] = true;
  conflict_table_[IROOT_EVENT_MEM_WRITE][IROOT_EVENT_MEM_READ] = true;
  conflict_table_[IROOT_EVENT_MEM_WRITE][IROOT_EVENT_MEM_WRITE] = true;
  conflict_table_[IROOT_EVENT_MUTEX_UNLOCK][IROOT_EVENT_MUTEX_LOCK] = true;
}

void PredictorNew::AllocAddrRegion(address_t addr, size_t size) {
  DEBUG_ASSERT(addr && size);
  ScopedLock locker(internal_lock_);
  filter_->AddRegion(addr, size, false);
}

void PredictorNew::FreeAddrRegion(address_t addr) {
  if (!addr) return;
  ScopedLock locker(internal_lock_);
  size_t size = filter_->RemoveRegion(addr, false);
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // free cond meta
    CondMeta::Table::iterator cit = cond_meta_table_.find(iaddr);
    if (cit != cond_meta_table_.end()) {
      cond_meta_table_.erase(cit);
    }
    // free barrier meta
    BarrierMeta::Table::iterator bit = barrier_meta_table_.find(iaddr);
    if (bit != barrier_meta_table_.end()) {
      barrier_meta_table_.erase(bit);
    }
    // free shared meta
    SharedMeta::Table::iterator sit = shared_meta_table_.find(iaddr);
    if (sit != shared_meta_table_.end()) {
      shared_meta_table_.erase(sit);
    }
    // free meta
    Meta::Table::iterator it = meta_table_.find(iaddr);
    if (it != meta_table_.end()) {
      ProcessFree(it->second);
      meta_table_.erase(it);
    }
  }
}

PredictorNew::Meta *PredictorNew::GetMemMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new Meta(Meta::TYPE_MEM);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    Meta *meta = it->second;
    switch (meta->type) {
      case Meta::TYPE_MEM:
        return meta;
      case Meta::TYPE_MUTEX:
        return NULL;
      default:
        DEBUG_ASSERT(0); // should not reach here
        return NULL;
    }
  }
}

PredictorNew::Meta *PredictorNew::GetMutexMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new Meta(Meta::TYPE_MUTEX);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    Meta *meta = it->second;
    switch (meta->type) {
      case Meta::TYPE_MEM:
        // XXX: expect this case to be very rare
        ProcessFree(meta);
        meta = new Meta(Meta::TYPE_MUTEX);
        it->second = meta;
        return meta;
      case Meta::TYPE_MUTEX:
        return meta;
      default:
        DEBUG_ASSERT(0); // should not reach here
        return NULL;
    }
  }
}

PredictorNew::CondMeta *PredictorNew::GetCondMeta(address_t iaddr) {
  CondMeta::Table::iterator it = cond_meta_table_.find(iaddr);
  if (it == cond_meta_table_.end()) {
    CondMeta *meta = new CondMeta;
    cond_meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

PredictorNew::BarrierMeta *PredictorNew::GetBarrierMeta(address_t iaddr) {
  BarrierMeta::Table::iterator it = barrier_meta_table_.find(iaddr);
  if (it == barrier_meta_table_.end()) {
    BarrierMeta *meta = new BarrierMeta;
    barrier_meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

// return the number of mem accesses to the  same memory before current access
int PredictorNew::getNumAcc(AccSum* acc_sum) {

    int count = 0;

  AccSum::TimeInfoEntry &tinfo_entry = acc_sum->tinfo.back();

  ThdClkInfo thd_clk_info = tinfo_entry.second;

  timestamp_t thd_start =  thd_clk_info.start;

  AccHisto *acc_histo = acc_sum->meta->acc_histo;

  thread_id_t thd_id = acc_sum->thd_id;

  for (AccHisto::AccSumTable::iterator tit = acc_histo->acc_sum_table.begin();
       tit != acc_histo->acc_sum_table.end(); ++tit) {
      
      //check this thread's access history 
      if (tit->first == thd_id) {

        for (AccSum::Vec::iterator vit = tit->second.begin();
               vit != tit->second.end(); ++vit) {
            
            AccSum *rmt_acc_sum = *vit;

            bool same = false;

            for (AccSum::TimeInfo::reverse_iterator it = rmt_acc_sum->tinfo.rbegin();
                it != rmt_acc_sum->tinfo.rend(); ++it) {
                
                AccSum::TimeInfoEntry &entry = *it;
                
                ThdClkInfo thd_clk_info = entry.second;

                timestamp_t rmt_thd_start =  thd_clk_info.start;

                if (rmt_thd_start < thd_start && !same) {

                    count++;
                }
            }
        }
      }
  }

  return count;
}

// used for number of accesses, returns the number of memory accesses before this in both src and dst
std::pair<int, int>* PredictorNew::searchForAccSumPair(AccSum* src, AccSum* dst) {

    std::map <std::pair<AccSum*, AccSum*>*, std::pair<int, int>*>::iterator iter;

    for (iter = iroot_inst_count_map.begin(); iter != iroot_inst_count_map.end(); ++iter) {

        std::pair<AccSum*, AccSum*>* acc_sum_pair = iter->first;

        if ((src == acc_sum_pair->first) && (dst == acc_sum_pair->second) && 
            (src->thd_id == acc_sum_pair->first->thd_id) && 
            (dst->thd_id == acc_sum_pair->second->thd_id)) {

            return iter->second;
        }
    }

    return NULL;
}

void PredictorNew::PredictiRoot() {
  // predict idiom1 iroots according to access summary pairs
  for (AccSum::PairIndex::iterator iit = acc_sum_succ_index_.begin();
       iit != acc_sum_succ_index_.end(); ++iit) {
    AccSum *src = iit->first;
    for (AccSum::Vec::iterator vit = iit->second.begin();
         vit != iit->second.end(); ++vit) {
      AccSum *dst = *vit;
      // predict iroot according to src->dst
      iRootEvent *e0 = iroot_db_->GetiRootEvent(src->inst, src->type, false);
      iRootEvent *e1 = iroot_db_->GetiRootEvent(dst->inst, dst->type, false);
      iRoot *iroot = iroot_db_->GetiRoot(IDIOM_1, false, e0, e1);
        if ((src->type == IROOT_EVENT_MEM_READ || src->type == IROOT_EVENT_MEM_WRITE) && 
        (dst->type == IROOT_EVENT_MEM_READ || dst->type == IROOT_EVENT_MEM_WRITE)) {

      std::pair<int, int>* count_pair = searchForAccSumPair(src, dst);

      if (count_pair) {

          int src_count = count_pair->first;

          int dst_count = count_pair->second;

          int old_src_count = iroot->getSrcCount();

          int old_dst_count = iroot->getDstCount();

          // we need the minimum number of mem access before. Therfore, take the min of all the mem acc counts
          if (iroot->getCountPairBool()) {

            if (old_src_count < src_count) {

                count_pair->first = old_src_count;
            }

            if (old_dst_count < dst_count) {

                count_pair->second = old_dst_count;
            }
          }
            iroot->AddCountPair(count_pair);
      }
        }

      memo_->Predicted(iroot, false);
      if (CheckAsync(src) || CheckAsync(dst)) {
        memo_->SetAsync(iroot, false);
      }
    }
  }
}

PredictorNew::AccSum *PredictorNew::ProcessAccSumUpdate(DynAcc *dyn_acc) {
  // obtain the access history of this meta
  AccHisto *acc_histo = dyn_acc->meta->acc_histo;
  DEBUG_ASSERT(acc_histo);
  // the flag indicates whether to skip full search
  bool skip_search = false;
  // check whether a matching access summary can be found
  AccSum *curr_acc_sum = MatchAccSum(dyn_acc);
  if (curr_acc_sum) {

    // check whether a new vector clock value has been observed
    // for this access summary. only need to check the last vc.
    DEBUG_ASSERT(!curr_acc_sum->tinfo.empty());
    AccSum::TimeInfoEntry &tinfo_entry = curr_acc_sum->tinfo.back();
    if (dyn_acc->vc.Equal(&tinfo_entry.first)) {
      // no need to do a full search
      AddThdClk(&tinfo_entry.second, dyn_acc->thd_clk);
      skip_search = true;
      DEBUG_STAT_INC("pd_acc_sum_hit", 1);
    } else {
      // add a new tinfo entry
      ThdClkInfo thd_clk_info(dyn_acc->thd_clk);
      curr_acc_sum->tinfo.push_back(
          AccSum::TimeInfoEntry(dyn_acc->vc, thd_clk_info));
      DEBUG_STAT_INC("pd_new_vc", 1);
      DEBUG_STAT_MAX("pd_max_vc", curr_acc_sum->tinfo.size());
    }
  } else {
    // create a new access summary and update hash index
    curr_acc_sum = new AccSum;
    curr_acc_sum->meta = dyn_acc->meta;
    curr_acc_sum->thd_id = dyn_acc->thd_id;
    curr_acc_sum->type = dyn_acc->type;
    curr_acc_sum->inst = dyn_acc->inst;
    curr_acc_sum->fls = dyn_acc->fls;
    ThdClkInfo thd_clk_info(dyn_acc->thd_clk);
    curr_acc_sum->tinfo.push_back(
        AccSum::TimeInfoEntry(dyn_acc->vc, thd_clk_info));
    acc_histo->acc_sum_table[dyn_acc->thd_id].push_back(curr_acc_sum);
    acc_sum_hash_index_[Hash(curr_acc_sum)].push_back(curr_acc_sum);
    DEBUG_STAT_INC("pd_acc_sum_new", 1);
    DEBUG_STAT_MAX("pd_max_acc_sum",
                   acc_histo->acc_sum_table[dyn_acc->thd_id].size());
  } // end of else (curr_acc_sum)

  // do a full search against all access summaries in other threads
  if (!skip_search) {
    DEBUG_ASSERT(curr_acc_sum);
    // iterate over each remote thread
    for (AccHisto::AccSumTable::iterator tit = acc_histo->acc_sum_table.begin();
         tit != acc_histo->acc_sum_table.end(); ++tit) {
      if (tit->first == curr_acc_sum->thd_id)
        continue; // skip the current thread
      // iterate over each access summary
      for (AccSum::Vec::iterator vit = tit->second.begin();
           vit != tit->second.end(); ++vit) {
        AccSum *rmt_acc_sum = *vit;
        DEBUG_ASSERT(curr_acc_sum->meta == rmt_acc_sum->meta);
        // predict rmt->curr (find curr's predecessors)
        if (CheckConflict(rmt_acc_sum->type, curr_acc_sum->type)) {
          if (!ExistAccSumPair(rmt_acc_sum, curr_acc_sum)) {
            if (CheckMutexExclution(&rmt_acc_sum->fls, &curr_acc_sum->fls)) {
              if (CheckConcurrent(&dyn_acc->vc, rmt_acc_sum)) {
                if (CheckAtomic(rmt_acc_sum, curr_acc_sum)) {
                  AddAccSumPair(rmt_acc_sum, curr_acc_sum);
                }
              }
            }
          }
        }
        // predict curr->rmt (find curr's successors)
        if (CheckConflict(curr_acc_sum->type, rmt_acc_sum->type)) {
          if (!ExistAccSumPair(curr_acc_sum, rmt_acc_sum)) {
            if (CheckMutexExclution(&curr_acc_sum->fls, &rmt_acc_sum->fls)) {
              if (CheckConcurrent(&dyn_acc->vc, rmt_acc_sum)) {
                if (CheckAtomic(curr_acc_sum, rmt_acc_sum)) {
                  AddAccSumPair(curr_acc_sum, rmt_acc_sum);
                }
              }
            }
          }
        }
      } // end of for each access summary
    } // end of for each thread
  } // end of if (!skip_search)

  // return the updated access summary
  return curr_acc_sum;
}

void PredictorNew::ProcessAccSumPairUpdate(Meta *meta) {
  AccHisto *acc_histo = meta->acc_histo;
  DEBUG_ASSERT(acc_histo);

  // no need to continue if only one thread is found
  if (acc_histo->acc_sum_table.size() < 2)
    return;

  // predict all possible access summary pairs for the given meta
  // this function is called when the meta is about to be freed
  // not only need us to find concurrent acc sum pairs, we need to
  // find valid non-concurrent acc sum pairs as well. for example:
  //
  //   T1           T2           T3
  //   I1:R(A)
  //   barrier()    barrier()    barrier()
  //                             I2:W(A)
  //                I3:W(A)
  //   barrier()    barrier()    barrier()
  //                I4:W(A)
  //                I5:W(A)
  //
  // In this case, we should predict I2->I4 even if I2 happens
  // before I4 (non-concurrent). However, we need to avoid predicting
  // I1->I4. Also, we need to avoid predicting I2->I5.

  DEBUG_STAT_INC("pd_non_concur_upd", 1);
  typedef std::pair<VectorClock *, AccSum::Pair> TimedEntry;
  typedef std::vector<TimedEntry> TimedEntryVec;
  typedef std::map<thread_id_t, TimedEntryVec> TimedEntryTable;

  // step 1: construct a vector for each thread. each element in
  // the vector has a unique vector clock value and a pair of access
  // summaries indicating the earliest and latest access summaries
  // in this vector clock range.
  TimedEntryTable timed_entry_table;
  for (AccHisto::AccSumTable::iterator tit = acc_histo->acc_sum_table.begin();
       tit != acc_histo->acc_sum_table.end(); ++tit) {
    thread_id_t thd_id = tit->first;
    AccSum::Vec &acc_sum_vec = tit->second;
    // create the timed entry vector
    TimedEntryVec &timed_entry_vec = timed_entry_table[thd_id];
    // initialize the time cursor vector
    std::vector<size_t> time_cursor;
    for (size_t i = 0; i < acc_sum_vec.size(); i++) {
      time_cursor.push_back(0);
      DEBUG_ASSERT(!acc_sum_vec[i]->tinfo.empty());
    }
    // loop until all acc_sum are finished
    size_t num_finished = 0;
    while (num_finished != acc_sum_vec.size()) {
      // we need two passes here. in the first pass, find the earliest
      // vector clock value. in the second pass, advance the cursor
      // for all (many have the same vc).
      VectorClock *earliest_vc = NULL;
      // first pass
      for (size_t i = 0; i < acc_sum_vec.size(); i++) {
        AccSum *acc_sum = acc_sum_vec[i];
        if (time_cursor[i] == acc_sum->tinfo.size())
          continue;
        VectorClock *vc = &acc_sum->tinfo[time_cursor[i]].first;
        if (!earliest_vc) {
          earliest_vc = vc;
        } else {
          if (vc->HappensBefore(earliest_vc)) {
            earliest_vc = vc;
          }
        }
      } // end of first pass

      // second pass
      DEBUG_ASSERT(earliest_vc);
      AccSum *start_acc_sum = NULL;
      AccSum *end_acc_sum = NULL;
      timestamp_t start_time = INVALID_TIMESTAMP;
      timestamp_t end_time = INVALID_TIMESTAMP;
      for (size_t i = 0; i < acc_sum_vec.size(); i++) {
        AccSum *acc_sum = acc_sum_vec[i];
        if (time_cursor[i] == acc_sum->tinfo.size())
          continue;
        AccSum::TimeInfoEntry &tinfo_entry = acc_sum->tinfo[time_cursor[i]];
        VectorClock *vc = &tinfo_entry.first;
        if (vc->Equal(earliest_vc)) {
          // set start acc sum and time
          if (!start_acc_sum) {
            start_acc_sum = acc_sum;
            start_time = GetSmallestThdClk(&tinfo_entry.second);
          } else {
            timestamp_t time = GetSmallestThdClk(&tinfo_entry.second);
            if (time < start_time) {
              start_acc_sum = acc_sum;
              start_time = time;
            } else if (time == start_time) {
              if (acc_sum->type == IROOT_EVENT_MEM_READ)
                start_acc_sum = acc_sum;
            }
          }
          // set end acc sum and time
          if (!end_acc_sum) {
            end_acc_sum = acc_sum;
            end_time = GetLargestThdClk(&tinfo_entry.second);
          } else {
            timestamp_t time = GetLargestThdClk(&tinfo_entry.second);
            if (time > end_time) {
              end_acc_sum = acc_sum;
              end_time = time;
            } else if (time == end_time) {
              if (acc_sum->type == IROOT_EVENT_MEM_WRITE)
                end_acc_sum = acc_sum;
            }
          }
          // advance the cursor
          time_cursor[i]++;
          if (time_cursor[i] == acc_sum->tinfo.size()) {
            num_finished++;
          }
        }
      } // end of second pass

      // update timed entry
      TimedEntry timed_entry;
      timed_entry.first = earliest_vc;
      timed_entry.second.first = start_acc_sum;
      timed_entry.second.second = end_acc_sum;
      timed_entry_vec.push_back(timed_entry);
    } // end of while loop
  } // end of for each thread in access summary table

  // step 2: for each thread, for each element in the vector, find
  // its immediate predecessor (no need to find successors)
  for (TimedEntryTable::iterator curr_tit = timed_entry_table.begin();
       curr_tit != timed_entry_table.end(); ++curr_tit) {
    thread_id_t curr_thd_id = curr_tit->first;
    TimedEntryVec &curr_timed_entry_vec = curr_tit->second;
    // for each timed entry in the current thread
    for (size_t curr_i = 0; curr_i < curr_timed_entry_vec.size(); curr_i++) {
      TimedEntry &curr_timed_entry = curr_timed_entry_vec[curr_i];
      // for each remote thread, find immediate predecessor candidates
      TimedEntryVec cand_preds;
      for (TimedEntryTable::iterator rmt_tit = timed_entry_table.begin();
           rmt_tit != timed_entry_table.end(); ++rmt_tit) {
        thread_id_t rmt_thd_id = rmt_tit->first;
        if (rmt_thd_id == curr_thd_id)
          continue; // skip the current thread
        TimedEntryVec &rmt_timed_entry_vec = rmt_tit->second;
        TimedEntry *cand_pred = NULL;
        for (size_t rmt_i = 0; rmt_i < rmt_timed_entry_vec.size(); rmt_i++) {
          TimedEntry &rmt_timed_entry = rmt_timed_entry_vec[rmt_i];
          // check whether rmt happens befor curr
          if (rmt_timed_entry.first->HappensBefore(curr_timed_entry.first)) {
            cand_pred = &rmt_timed_entry;
          } else {
            break;
          }
        }
        if (cand_pred) {
          cand_preds.push_back(*cand_pred);
        }
      } // end of for each remote thread
      // scan cand_preds
      for (size_t i = 0; i < cand_preds.size(); i++) {
        bool cand = true;
        for (size_t j = 0; j < cand_preds.size(); j++) {
          if (i == j)
            continue;
          if (cand_preds[i].first->HappensBefore(cand_preds[j].first)) {
            cand = false;
            break;
          }
        }
        if (cand) {
          if (curr_i > 0) {
            TimedEntry &prev_timed_entry = curr_timed_entry_vec[curr_i - 1];
            if (cand_preds[i].first->HappensBefore(prev_timed_entry.first))
              continue;
          }
          // predict rmt->curr (find curr's predecessors)
          AccSum *curr_acc_sum = curr_timed_entry.second.first;
          AccSum *rmt_acc_sum = cand_preds[i].second.second;
          if (CheckConflict(rmt_acc_sum->type, curr_acc_sum->type)) {
            if (!ExistAccSumPair(rmt_acc_sum, curr_acc_sum)) {
              if (CheckMutexExclution(&rmt_acc_sum->fls, &curr_acc_sum->fls)) {
                AddAccSumPair(rmt_acc_sum, curr_acc_sum);
              }
            }
          }
        } // end of if cand
      } // end of scan each cand_pred
    } // end of for each current timed entry
  } // end of for each thread
}

// helper functions for complex idioms
void PredictorNew::UpdateLocalPair(thread_id_t thd_id,
                                   RecentInfo::Entry *prev_entry,
                                   RecentInfo::Entry *curr_entry,
                                   AccSum *succ_acc_sum,
                                   AccSum *pred_acc_sum) {
  // add the local pair
  LocalPair::Vec &lp_vec = lp_table_[thd_id];
  LocalPair lp;
  lp.prev_entry = prev_entry;
  lp.curr_entry = curr_entry;
  lp.succ_acc_sum = succ_acc_sum;
  lp.pred_acc_sum = pred_acc_sum;
  lp_vec.push_back(lp);
  // update index
  lp_pair_index_[prev_entry->acc_sum][curr_entry->acc_sum].push_back(lp);
}

void PredictorNew::UpdateDeadlockPair(thread_id_t thd_id,
                                      RecentInfo::Entry *prev_entry,
                                      RecentInfo::Entry *curr_entry) {
  // add the deadlock pair
  LocalPair::Vec &dl_vec = dl_table_[thd_id];
  LocalPair dl;
  dl.prev_entry = prev_entry;
  dl.curr_entry = curr_entry;
  dl_vec.push_back(dl);
  // update index
  dl_pair_index_[prev_entry->acc_sum][curr_entry->acc_sum].push_back(dl);
}

bool PredictorNew::CheckCompound(RecentInfo::Entry *prev_entry,
                                 RecentInfo::Entry *curr_entry,
                                 AccSum *succ_acc_sum,
                                 AccSum *pred_acc_sum) {
  // This function is for idiom2/idiom3/idiom4
  DEBUG_ASSERT(succ_acc_sum->thd_id == pred_acc_sum->thd_id);
  DEBUG_ASSERT(curr_entry->acc_sum->thd_id != succ_acc_sum->thd_id);
  // XXX: we can use binary search here

  // we return true if the follow conditions hold:
  // 1) there should exist a vc in succ_acc_sum such that it
  //    does not happens before prev_acc_sum's vc
  // 2) there should exist a vc in pred_acc_sum such that it
  //    does not happens after curr_acc_sum's vc
  // 3) the earliest local timestamp for succ should be less
  //    than the latest local timestamp for pred
  // 4) if it is multi-var idiom, validate with the common
  //    lock set to address the following case:
  //
  //      T1              T2
  //                      I3: W(A)
  //      lock(m)
  //      I1: R(A)
  //      I2: W(B)
  //      unlock(m)
  //                      lock(m)
  //                      I4: R(B)
  //                      unlock(m)
  //
  //    Because of the critical section, I1->I3...I4->I2 is
  //    infeasible even if each dependency is valid and passes
  //    the checks (1), (2) and (3)

  // step 1: find earliest timeinfo in succ_acc_sum such that it
  // does not happens before prev_acc_sum's vc
  AccSum::TimeInfoEntry *succ_entry = NULL;
  for (AccSum::TimeInfo::reverse_iterator it = succ_acc_sum->tinfo.rbegin();
       it != succ_acc_sum->tinfo.rend(); ++it) {
    AccSum::TimeInfoEntry &entry = *it;
    if (!entry.first.HappensBefore(&prev_entry->vc)) {
      succ_entry = &entry;
    } else {
      break;
    }
  }
  if (!succ_entry)
    return false;

  // step 2: find latest timeinfo in pred_acc_sum such that it
  // does not happens after curr_acc_sum's vc
  AccSum::TimeInfoEntry *pred_entry = NULL;
  for (AccSum::TimeInfo::iterator it = pred_acc_sum->tinfo.begin();
       it != pred_acc_sum->tinfo.end(); ++it) {
    AccSum::TimeInfoEntry &entry = *it;
    if (!curr_entry->vc.HappensBefore(&entry.first)) {
      pred_entry = &entry;
    } else {
      break;
    }
  }
  if (!pred_entry)
    return false;

  // step 3: validate that the smallest timestamp in succ_entry is
  // smaller than the largest timestamp in pred_entry
  timestamp_t smallest = GetSmallestThdClk(&succ_entry->second);
  timestamp_t largest = GetLargestThdClk(&pred_entry->second);
  if (largest < smallest)
    return false;

  // step 4: validate lock set for multi-var idioms
  if (curr_entry->meta != prev_entry->meta) {
    // calculate common lock set
    FLockSet fls;
    CommonLockSet(&fls, &prev_entry->ls, &curr_entry->ls);
    if (!CheckMutexExclution(&fls, &succ_acc_sum->fls) ||
        !CheckMutexExclution(&pred_acc_sum->fls, &fls)) {
      return false;
    }
  }

  // return true if reaches here
  return true;
}

bool PredictorNew::CheckCompound2(RecentInfo::Entry *prev_entry,
                                  RecentInfo::Entry *curr_entry,
                                  AccSum *succ_acc_sum,
                                  AccSum *pred_acc_sum) {
  // This function is for idiom5
  DEBUG_ASSERT(succ_acc_sum->thd_id == pred_acc_sum->thd_id);
  DEBUG_ASSERT(curr_entry->acc_sum->thd_id != succ_acc_sum->thd_id);
  DEBUG_ASSERT(succ_acc_sum->meta != pred_acc_sum->meta);
  // XXX: we can use binary search here

  // we return true if the follow conditions hold:
  // 1) there should exist a vc in succ_acc_sum such that it
  //    does not happens before prev_acc_sum's vc
  // 2) there should exist a vc in pred_acc_sum such that it
  //    does not happens after curr_acc_sum's vc
  // 3) the earliest local timestamp for pred should be less
  //    than the latest local timestamp for succ
  // 4) <pred,succ> should construct a valid local pair (NEW)

  // step 1: find latest timeinfo in succ_acc_sum such that it
  // does not happens before prev_acc_sum's vc
  AccSum::TimeInfoEntry *succ_entry = NULL;
  for (AccSum::TimeInfo::reverse_iterator it = succ_acc_sum->tinfo.rbegin();
       it != succ_acc_sum->tinfo.rend(); ++it) {
    AccSum::TimeInfoEntry &entry = *it;
    if (!entry.first.HappensBefore(&prev_entry->vc)) {
      succ_entry = &entry;
      break;
    }
  }
  if (!succ_entry)
    return false;

  // step 2: find earliest timeinfo in pred_acc_sum such that it
  // does not happens after curr_acc_sum's vc
  AccSum::TimeInfoEntry *pred_entry = NULL;
  for (AccSum::TimeInfo::iterator it = pred_acc_sum->tinfo.begin();
       it != pred_acc_sum->tinfo.end(); ++it) {
    AccSum::TimeInfoEntry &entry = *it;
    if (!curr_entry->vc.HappensBefore(&entry.first)) {
      pred_entry = &entry;
      break;
    }
  }
  if (!pred_entry)
    return false;

  // step 3: validate that the smallest timestamp in pred_entry is
  // smaller than the largest timestamp in succ_entry
  timestamp_t smallest = GetSmallestThdClk(&pred_entry->second);
  timestamp_t largest = GetLargestThdClk(&succ_entry->second);
  if (largest < smallest)
    return false;

  // return true if reaches here
  return true;
}

bool PredictorNew::CheckDeadlock(LocalPair *dl, LocalPair *rmt_dl) {
  // This function is for deadlock idiom5
  AccSum *curr_acc_sum = dl->curr_entry->acc_sum;
  AccSum *prev_acc_sum = dl->prev_entry->acc_sum;
  AccSum *pred_acc_sum = rmt_dl->prev_entry->acc_sum;
  AccSum *succ_acc_sum = rmt_dl->curr_entry->acc_sum;
  // we return true if the follow conditions hold:
  // TODO: can be more accurate here (e.g. see whether prev->succ is possible)
  // 1) prev should not happens after succ
  // 2) pred should not happens after curr
  // 3) both prev->succ and pred->curr pass lock set check
  if (rmt_dl->curr_entry->vc.HappensBefore(&dl->prev_entry->vc))
    return false;
  if (dl->curr_entry->vc.HappensBefore(&rmt_dl->prev_entry->vc))
    return false;
  if (!CheckMutexExclution(&prev_acc_sum->fls, &succ_acc_sum->fls) ||
      !CheckMutexExclution(&pred_acc_sum->fls, &curr_acc_sum->fls))
    return false;

  // return true if reaches here
  return true;
}

iRoot *PredictorNew::Predict(IdiomType idiom, ...) {
  // read arguments
  int num_args = iRoot::GetNumEvents(idiom);
  AccSum::Vec av;
  std::vector<iRootEvent *> ev;
  va_list vl;
  va_start(vl, idiom);
  for (int i = 0; i < num_args; i++) {
    AccSum *a = va_arg(vl, AccSum *);
    iRootEvent *e = iroot_db_->GetiRootEvent(a->inst, a->type, false);
    av.push_back(a);
    ev.push_back(e);
  }
  va_end(vl);

  // predict the iroot
  iRoot *iroot = NULL;
  switch (idiom) {
    case IDIOM_1:
      iroot = iroot_db_->GetiRoot(idiom, false, ev[0], ev[1]);
      memo_->Predicted(iroot, false);
      if (CheckAsync(av[0]) || CheckAsync(av[1]))
        memo_->SetAsync(iroot, false);
      break;
    case IDIOM_2:
      iroot = iroot_db_->GetiRoot(idiom, false, ev[0], ev[1], ev[2]);
      memo_->Predicted(iroot, false);
      if (CheckAsync(av[2]) || CheckAsync(av[1]))
        memo_->SetAsync(iroot, false);
      break;
    case IDIOM_3:
    case IDIOM_4:
      iroot = iroot_db_->GetiRoot(idiom, false, ev[0], ev[1], ev[2], ev[3]);
      memo_->Predicted(iroot, false);
      if (CheckAsync(av[3]) || CheckAsync(av[2]))
        memo_->SetAsync(iroot, false);
      break;
    case IDIOM_5:
      iroot = iroot_db_->GetiRoot(idiom, false, ev[0], ev[1], ev[2], ev[3]);
      memo_->Predicted(iroot, false);
      if (CheckAsync(av[3]) || CheckAsync(av[1]))
        memo_->SetAsync(iroot, false);
      break;
    default:
      assert(0);
      break;
  }
  return iroot;
}

void PredictorNew::PredictComplexiRoot() {
  // Phase 1 (for idiom2/3/4)
  // iterate recent info of each thread
  for (RecentInfo::Table::iterator tit = ri_table_.begin();
       tit != ri_table_.end(); ++tit) {
    thread_id_t thd_id = tit->first;
    RecentInfo &ri = tit->second;
    // iterate each entry
    for (long curr_idx = 0; curr_idx < (long)ri.entry_vec.size(); curr_idx++) {
      RecentInfo::Entry &curr_entry = ri.entry_vec[curr_idx];
      DEBUG_ASSERT(curr_entry.acc_sum);
      // find curr's predecessors
      // skip curr if no predecessor is found
      AccSum::PairIndex::iterator preds_it
          = acc_sum_pred_index_.find(curr_entry.acc_sum);
      if (preds_it == acc_sum_pred_index_.end())
        continue;

      // search local recent previous accesses
      // for multi var idioms, we need to search recent accesses
      // to each meta and then calculate the distances. to make
      // sure that there is no access to the same locations as
      // those instructions in the local pair do, we need to first
      // find out the recent access of the current meta.
      //
      //   T1               T2
      //
      //   I1:W(A)
      //   I2:R(B)          I4: R(A)
      //   ...              I5: W(B)
      //   I3:W(B)
      //
      // (in this example, we should not report a local pair <I1,I3>
      //  because I2 access B as I3 does in between I1 and I3)
      Meta::HashSet visited_meta;
      for (long prev_idx = curr_idx - 1; prev_idx >= 0; prev_idx--) {
        RecentInfo::Entry &prev_entry = ri.entry_vec[prev_idx];
        DEBUG_ASSERT(prev_entry.acc_sum);
        // no need to continue if goes beyond the time window
        if (TIME_DISTANCE(prev_entry.thd_clk, curr_entry.thd_clk) >= vw_)
          break;
        // skip if a later access to the same meta is found
        if (visited_meta.find(prev_entry.meta) != visited_meta.end())
          continue;
        // find prev's successors
        // skip prev if no successor is found
        AccSum::PairIndex::iterator succs_it
            = acc_sum_succ_index_.find(prev_entry.acc_sum);
        if (succs_it != acc_sum_succ_index_.end()) {
          AccSum *curr_acc_sum = curr_entry.acc_sum;
          AccSum *prev_acc_sum = prev_entry.acc_sum;
          DEBUG_ASSERT(curr_acc_sum->meta == curr_entry.meta);
          DEBUG_ASSERT(prev_acc_sum->meta == prev_entry.meta);
          // for each successors
          for (AccSum::Vec::iterator sit = succs_it->second.begin();
               sit != succs_it->second.end(); ++sit) {
            AccSum *succ_acc_sum = *sit;
            // whether exists a access summary that is both pred and succ
            bool same_acc_sum_exist = false;
            // for each predecessors
            for (AccSum::Vec::iterator pit = preds_it->second.begin();
                 pit != preds_it->second.end(); ++pit) {
              AccSum *pred_acc_sum = *pit;
              // make sure that succ and pred are in the same thread
              if (succ_acc_sum->thd_id == pred_acc_sum->thd_id) {
                DEBUG_ASSERT(succ_acc_sum->thd_id != curr_acc_sum->thd_id);
                if (prev_acc_sum->meta == curr_acc_sum->meta) {
                  // for idiom3
                  if (CheckCompound(&prev_entry, &curr_entry,
                                    succ_acc_sum, pred_acc_sum)) {
                    Predict(IDIOM_3,
                            prev_acc_sum, succ_acc_sum,
                            pred_acc_sum, curr_acc_sum);
                  }
                } else {
                  DEBUG_ASSERT(succ_acc_sum->meta != pred_acc_sum->meta);
                  if (!single_var_idioms_) {
                    // for idiom4
                    if (CheckCompound(&prev_entry, &curr_entry,
                                      succ_acc_sum, pred_acc_sum)) {
                      Predict(IDIOM_4,
                              prev_acc_sum, succ_acc_sum,
                              pred_acc_sum, curr_acc_sum);
                    }
                    // for idiom5
                    if (CheckCompound2(&prev_entry, &curr_entry,
                                       succ_acc_sum, pred_acc_sum)) {
                      UpdateLocalPair(thd_id,
                                      &prev_entry, &curr_entry,
                                      succ_acc_sum, pred_acc_sum);
                    }
                  }
                }
                // for idiom2
                if (succ_acc_sum == pred_acc_sum) {
                  same_acc_sum_exist = true;
                }
              } // end of if pred and succ are in the same thread
            } // end of for each pred

            // for idiom2
            if (same_acc_sum_exist) {
              if (CheckCompound(&prev_entry, &curr_entry,
                                succ_acc_sum, succ_acc_sum)) {
                Predict(IDIOM_2, prev_acc_sum, succ_acc_sum, curr_acc_sum);
              }
            }
          } // end of for each succ
        } // end of if prev's successors are found

        // for deadlock (idiom5)
        if (!single_var_idioms_ && predict_deadlock_) {
          if (prev_entry.acc_sum->type == IROOT_EVENT_MUTEX_LOCK &&
              curr_entry.acc_sum->type == IROOT_EVENT_MUTEX_LOCK) {
            UpdateDeadlockPair(thd_id, &prev_entry, &curr_entry);
          }
        }

        // no need to continue if it goes beyond the time on which
        // the last access to the current meta is performed
        if (prev_entry.meta == curr_entry.meta)
          break;
        // mark prev meta as visited
        visited_meta.insert(prev_entry.meta);
      } // end of for each recent access (prev)
    } // end of for each recent info entry (curr)
  } // end of for each thread

  // Phase 2 (for idiom5)
  for (LocalPair::Table::iterator tit = lp_table_.begin();
       tit != lp_table_.end(); ++tit) {
    LocalPair::Vec &lp_vec = tit->second;
    for (LocalPair::Vec::iterator vit = lp_vec.begin();
         vit != lp_vec.end(); ++vit) {
      LocalPair &lp = *vit;
      RecentInfo::Entry *prev_entry = lp.prev_entry;
      RecentInfo::Entry *curr_entry = lp.curr_entry;
      AccSum *succ_acc_sum = lp.succ_acc_sum;
      AccSum *pred_acc_sum = lp.pred_acc_sum;
      DEBUG_ASSERT(tit->first != succ_acc_sum->thd_id);
      // check local pair index
      // TODO: can be more precise here that is, check against each other
      LocalPair::PairIndex::iterator pit = lp_pair_index_.find(pred_acc_sum);
      if (pit == lp_pair_index_.end())
        continue;
      LocalPair::Index::iterator sit = pit->second.find(succ_acc_sum);
      if (sit == pit->second.end())
        continue;
      // predict iroot
      Predict(IDIOM_5,
              prev_entry->acc_sum, succ_acc_sum,
              pred_acc_sum, curr_entry->acc_sum);
    } // end of for each entry in lp_vec
  } // end of for each thread

  if (predict_deadlock_) {
    // XXX: we expect the number of deadlock pairs to be small
    // therefore, we can use an O(N^2) algorithm here. we need
    // a better solution for the future.
    // outer loop
    for (LocalPair::Table::iterator tit = dl_table_.begin();
         tit != dl_table_.end(); ++tit) {
      LocalPair::Vec &dl_vec = tit->second;
      for (LocalPair::Vec::iterator vit = dl_vec.begin();
           vit != dl_vec.end(); ++vit) {
        LocalPair &dl = *vit;
        // inner loop
        for (LocalPair::Table::iterator rmt_tit = dl_table_.begin();
             rmt_tit != dl_table_.end(); ++rmt_tit) {
          if (rmt_tit->first == tit->first)
            continue; // skip the same thread
          LocalPair::Vec &rmt_dl_vec = rmt_tit->second;
          for (LocalPair::Vec::iterator rmt_vit = rmt_dl_vec.begin();
               rmt_vit != rmt_dl_vec.end(); ++rmt_vit) {
            LocalPair &rmt_dl = *rmt_vit;
            // skip if meta does not match
            if (dl.prev_entry->meta != rmt_dl.curr_entry->meta)
              continue;
            if (dl.curr_entry->meta != rmt_dl.prev_entry->meta)
              continue;
            // for deadlock idiom5
            if (CheckDeadlock(&dl, &rmt_dl)) {
              Predict(IDIOM_5,
                      dl.prev_entry->acc_sum,
                      rmt_dl.curr_entry->acc_sum,
                      rmt_dl.prev_entry->acc_sum,
                      dl.curr_entry->acc_sum);
            }
          }
        } // end of inner loop
      }
    } // end of outer loop
  } // end of if predict_deadlock_
}

void PredictorNew::ProcessRecentInfoUpdate(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk,
                                           VectorClock *curr_vc,
                                           LockSet *curr_ls,
                                           Meta *meta) {
  // get recent access info for current thread
  RecentInfo &ri = ri_table_[curr_thd_id];
  // in this algorith, we just keep a vector of access information
  // for each thread and postpone the complex idiom iroots search
  // to the end. the assumption here is that the number of access
  // summary dependencies is usually small compared to the number
  // of unique local pairs.
  RecentInfo::Entry ri_entry;
  ri_entry.thd_clk = curr_thd_clk;
  ri_entry.acc_sum = NULL;
  ri_entry.vc = *curr_vc;
  ri_entry.ls = *curr_ls;
  ri_entry.meta = meta;
  // obtain the current size of the entry vector as the index
  size_t ri_index = ri.entry_vec.size();
  // push the current entry to the vector
  ri.entry_vec.push_back(ri_entry);
  // update raw entry index
  ri.raw_entry_index[meta] = ri_index;
}

void PredictorNew::ProcessRecentInfoMaturize(AccSum *acc_sum) {
  if (!acc_sum)
    return;
  thread_id_t thd_id = acc_sum->thd_id;
  Meta *meta = acc_sum->meta;
  RecentInfo &ri = ri_table_[thd_id];
  RecentInfo::RawEntryIndex::iterator iit = ri.raw_entry_index.find(meta);
  if (iit != ri.raw_entry_index.end()) {
    // obtain the entry according to the index
    size_t ri_index = iit->second;
    DEBUG_ASSERT(ri_index < ri.entry_vec.size());
    RecentInfo::Entry &ri_entry = ri.entry_vec[ri_index];
    // maturize the access summary in the entry
    DEBUG_ASSERT(!ri_entry.acc_sum);
    ri_entry.acc_sum = acc_sum;
  }
}

// main entries
void PredictorNew::ProcessiRootEvent(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk,
                                     iRootEventType type,
                                     Inst *inst,
                                     Meta *meta) {
  DEBUG_STAT_INC("pd_iroot_event", 1);
  // obtain the access history of this meta
  AccHisto *acc_histo = meta->acc_histo;
  DEBUG_ASSERT(meta->acc_histo);
  // obtain the current vector clock and lockset
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc && curr_ls);
  // obtain the recent dynamic access info
  DynAcc &dyn_acc = acc_histo->last_dyn_acc_table[curr_thd_id];
  AccSum *acc_sum = NULL;
  if (dyn_acc.meta) {
    // the current access is not the first access
    // update the flagged lockset
    UpdateLastFlag(&dyn_acc.fls, &dyn_acc.ls, curr_ls);
    // update access summaries
    acc_sum = ProcessAccSumUpdate(&dyn_acc);
    DEBUG_ASSERT(acc_sum);
  }
  // process recent info for complex idioms
  if (complex_idioms_) {
    ProcessRecentInfoMaturize(acc_sum);
    ProcessRecentInfoUpdate(curr_thd_id, curr_thd_clk, curr_vc, curr_ls, meta);
  }
  // update the recent dynamic access
  dyn_acc.meta = meta;
  dyn_acc.thd_id = curr_thd_id;
  dyn_acc.thd_clk = curr_thd_clk;
  dyn_acc.type = type;
  dyn_acc.inst = inst;
  dyn_acc.vc = *curr_vc;
  ClearFlag(&dyn_acc.fls);
  UpdateFirstFlag(&dyn_acc.fls, &dyn_acc.ls, curr_ls);
  dyn_acc.ls = *curr_ls;
}

void PredictorNew::ProcessFree(Meta *meta) {
  if (!meta->acc_histo)
    return;

  AccHisto *acc_histo = meta->acc_histo;
  for (AccHisto::DynAccTable::iterator dit =
          acc_histo->last_dyn_acc_table.begin();
          dit != acc_histo->last_dyn_acc_table.end(); ++dit) {
    DynAcc &dyn_acc = dit->second;
    // update the flagged lockset for this dynamic access
    UpdateLastFlag(&dyn_acc.fls, &dyn_acc.ls, NULL);
    // update access summaries according to this dynamic access
    AccSum *acc_sum = ProcessAccSumUpdate(&dyn_acc);
    // for complex idioms
    if (complex_idioms_) {
      ProcessRecentInfoMaturize(acc_sum);
    }
  }
  ProcessAccSumPairUpdate(meta);
  // free meta data structure
  delete meta->acc_histo;
  meta->acc_histo = NULL; // make sure no accidental illegal reference
}

void PredictorNew::ProcessThreadExit(thread_id_t curr_thd_id) {
  // TODO: confirm the implementation of this function
  // no need to do any thing because ProcessFree will take care
}

void PredictorNew::ProcessSignal(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  CondMeta::signal_id_t next_signal_id = ++meta->curr_signal_id;
  for (CondMeta::WaitMap::iterator mit = meta->wait_map.begin();
       mit != meta->wait_map.end(); ++mit) {
    CondMeta::WaitInfo &wait_info = mit->second;
    if (!wait_info.broadcasted) {
      wait_info.signal_map[next_signal_id] = *curr_vc;
    }
  }
  curr_vc->Increment(curr_thd_id);
}

void PredictorNew::ProcessBroadcast(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  for (CondMeta::WaitMap::iterator mit = meta->wait_map.begin();
       mit != meta->wait_map.end(); ++mit) {
    CondMeta::WaitInfo &wait_info = mit->second;
    if (!wait_info.broadcasted) {
      wait_info.broadcasted = true;
      wait_info.broadcast_vc = *curr_vc;
    }
  }
  curr_vc->Increment(curr_thd_id);
}

void PredictorNew::ProcessPreWait(thread_id_t curr_thd_id, CondMeta *meta,
                                  bool timedwait) {
  CondMeta::WaitInfo &self_wait_info = meta->wait_map[curr_thd_id];
  self_wait_info.timed = timedwait;
  self_wait_info.broadcasted = false;
}

void PredictorNew::ProcessPostWait(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  CondMeta::WaitInfo &self_wait_info = meta->wait_map[curr_thd_id];
  if (self_wait_info.signal_map.empty()) {
    if (self_wait_info.broadcasted) {
      curr_vc->Join(&self_wait_info.broadcast_vc);
    } else {
      assert(self_wait_info.timed);
    }
  } else {
    CondMeta::SignalMap::iterator sit = self_wait_info.signal_map.begin();
    CondMeta::signal_id_t signal_id = sit->first;
    curr_vc->Join(&sit->second);
    for (CondMeta::WaitMap::iterator mit = meta->wait_map.begin();
         mit != meta->wait_map.end(); ++mit) {
      thread_id_t thd_id = mit->first;
      CondMeta::WaitInfo &wait_info = mit->second;
      // skip self
      if (thd_id == curr_thd_id)
        continue;
      wait_info.signal_map.erase(signal_id);
    }
  }
  meta->wait_map.erase(curr_thd_id);
}

void PredictorNew::ProcessPreBarrier(thread_id_t curr_thd_id,
                                     BarrierMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  // choose which table to use
  BarrierMeta::VectorClockMap *wait_table = NULL;
  if (meta->pre_using_table1)
    wait_table = &meta->barrier_wait_table1;
  else
    wait_table = &meta->barrier_wait_table2;
  (*wait_table)[curr_thd_id] = std::pair<VectorClock, bool>(*curr_vc, false);
}

void PredictorNew::ProcessPostBarrier(thread_id_t curr_thd_id,
                                      BarrierMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  // choose which table to use
  BarrierMeta::VectorClockMap *wait_table = NULL;
  if (meta->post_using_table1)
    wait_table = &meta->barrier_wait_table1;
  else
    wait_table = &meta->barrier_wait_table2;
  bool all_flagged_ = true;
  bool all_not_flagged_ = true;
  for (BarrierMeta::VectorClockMap::iterator it = wait_table->begin();
       it != wait_table->end(); ++it) {
    // flag the current thread
    if (it->first == curr_thd_id) {
      DEBUG_ASSERT(it->second.second == false);
      it->second.second = true;
    } else {
      if (it->second.second == false) {
        all_flagged_ = false;
      } else {
        all_not_flagged_ = false;
      }
    }
    // join vector clock
    curr_vc->Join(&it->second.first);
  }
  // increment its own tick
  curr_vc->Increment(curr_thd_id);
  if (all_not_flagged_) {
    // switch pre
    meta->pre_using_table1 = !meta->pre_using_table1;
  }
  if (all_flagged_) {
    // clear table
    wait_table->clear();
    meta->post_using_table1 = !meta->post_using_table1;
  }
}

} // namespace idiom

