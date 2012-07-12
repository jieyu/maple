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

// File: idiom/predictor.cc - Implementation of the iRoot predictor analyzer.

#include "idiom/predictor.h"

#include <csignal>
#include <sys/syscall.h>
#include "core/logging.h"
#include "core/knob.h"

namespace idiom {

Predictor::Predictor()
    : internal_lock_(NULL),
      sinfo_(NULL),
      iroot_db_(NULL),
      memo_(NULL),
      sinst_db_(NULL),
      sync_only_(false),
      unit_size_(4),
      complex_idioms_(false),
      vw_(1000),
      racy_only_(false),
      predict_deadlock_(false),
      filter_(NULL) {
  // empty
}

Predictor::~Predictor() {
  // empty
}

void Predictor::Register() {
  knob_->RegisterBool("enable_predictor", "whether enable the iroot predictor", "0");
  knob_->RegisterBool("sync_only", "whether only monitor synchronization accesses", "0");
  knob_->RegisterBool("complex_idioms", "whether target complex idioms", "0");
  knob_->RegisterBool("racy_only", "whether only consider sync and racy memory dependencies", "0");
  knob_->RegisterBool("predict_deadlock", "whether predict and trigger deadlocks (experimental)", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("vw", "the vulnerability window (# dynamic inst)", "1000");
}

bool Predictor::Enabled() {
  return knob_->ValueBool("enable_predictor");
}

void Predictor::Setup(Mutex *lock, StaticInfo *sinfo, iRootDB *iroot_db,
                      Memo *memo, sinst::SharedInstDB *sinst_db) {
  internal_lock_ = lock;
  sinfo_ = sinfo;
  iroot_db_ = iroot_db;
  memo_ = memo;
  sinst_db_ = sinst_db;

  sync_only_ = knob_->ValueBool("sync_only");
  unit_size_ = knob_->ValueInt("unit_size");
  complex_idioms_ = knob_->ValueBool("complex_idioms");
  vw_ = knob_->ValueInt("vw");
  racy_only_ = knob_->ValueBool("racy_only");
  predict_deadlock_ = knob_->ValueBool("predict_deadlock");
  filter_ = new RegionFilter(internal_lock_->Clone());

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

void Predictor::ProgramExit() {
  if (complex_idioms_) {
    UpdateComplexiRoots();
  }
}

void Predictor::ImageLoad(Image *image, address_t low_addr,
                          address_t high_addr, address_t data_start,
                          size_t data_size, address_t bss_start,
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

void Predictor::ImageUnload(Image *image, address_t low_addr,
                            address_t high_addr, address_t data_start,
                            size_t data_size, address_t bss_start,
                            size_t bss_size) {
  DEBUG_ASSERT(low_addr);
  if (data_start)
    FreeAddrRegion(data_start);
  if (bss_start)
    FreeAddrRegion(bss_start);
}

void Predictor::SyscallEntry(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             int syscall_num) {
  ScopedLock locker(internal_lock_);

  switch (syscall_num) {
    case SYS_accept:
    case SYS_select:
    case SYS_pselect6:
    case SYS_rt_sigtimedwait:
      if (async_map_[curr_thd_id] == false) {
        async_map_[curr_thd_id] = true;
        async_start_time_map_[curr_thd_id] = curr_thd_clk;
      }
      break;
    default:
      break;
  }
}

void Predictor::SyscallExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            int syscall_num) {
  // do nothing
}

void Predictor::SignalReceived(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, int signal_num) {

  switch (signal_num) {
    case SIGINT:
    case SIGALRM:
      if (async_map_[curr_thd_id] == false) {
        async_map_[curr_thd_id] = true;
        async_start_time_map_[curr_thd_id] = curr_thd_clk;
      }
      break;
    default:
      break;
  }
}

void Predictor::ThreadStart(thread_id_t curr_thd_id,
                            thread_id_t parent_thd_id) {
  VectorClock *curr_vc = new VectorClock;
  LockSet *curr_ls = new LockSet;
  ScopedLock locker(internal_lock_);

  // initialize vector clock
  curr_vc->Increment(curr_thd_id);
  if (parent_thd_id != INVALID_THD_ID) {
    // this is not the main thread
    VectorClock *parent_vc = curr_vc_map_[parent_thd_id];
    DEBUG_ASSERT(parent_vc);
    curr_vc->Join(parent_vc);
    parent_vc->Increment(parent_thd_id);
  }
  curr_vc_map_[curr_thd_id] = curr_vc;

  // initialize lock set
  curr_ls_map_[curr_thd_id] = curr_ls;

  // decide whether to monitor this thread
  // TODO: selective monitoring
  monitored_thd_map_[curr_thd_id] = true;

  // set async map (whether the thread depends on external async events)
  async_map_[curr_thd_id] = false;
}

void Predictor::ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
  ScopedLock locker(internal_lock_);

  UpdateOnThreadExit(curr_thd_id);
  exit_vc_map_[curr_thd_id] = curr_vc_map_[curr_thd_id];
  curr_vc_map_.erase(curr_thd_id);
  curr_ls_map_.erase(curr_thd_id);
}

void Predictor::BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);

  if (FilterAccess(addr))
    return;

  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    PredictorMemMeta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue; // acecss to sync variable, ignore

    if (CheckShared(curr_thd_id, inst, meta)) {
      // the meta is shared
      UpdateForRead(curr_thd_id, curr_thd_clk, inst, meta);
    }
  }
}

void Predictor::BeforeMemWrite(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);

  if (FilterAccess(addr))
    return;

  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    PredictorMemMeta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue; // acecss to sync variable, ignore

    if (CheckShared(curr_thd_id, inst, meta)) {
      // the meta is shared
      UpdateForWrite(curr_thd_id, curr_thd_clk, inst, meta);
    }
  }
}

void Predictor::BeforeAtomicInst(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 std::string type, address_t addr) {
  // use heuristics to find locks and unlocks in libc library
  // the main idea is to identify special lock prefixed instructions
  if (!inst->image()->IsLibc())
    return;

  // an indication of lock in libc
  ScopedLock locker(internal_lock_);

  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);

  if (type.compare("DEC") == 0) {
    //DEBUG_FMT_PRINT_SAFE("[T%lx] internal libc unlock(0x%lx)\n",
    //                     curr_thd_id, addr);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
    curr_ls->Remove(addr);
  }

  // make sure that read/write in an atomic inst. are treated as a unit
  curr_ls->Add(~addr);
}

void Predictor::AfterAtomicInst(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                std::string type, address_t addr) {
  // use heuristics to find locks and unlocks in libc library
  // the main idea is to identify special lock prefixed instructions
  if (!inst->image()->IsLibc())
    return;

  // an indication of unlock in libc
  ScopedLock locker(internal_lock_);

  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_ls);

  // make sure that read/write in an atomic inst. are treated as a unit
  curr_ls->Remove(~addr);

  if (type.compare("CMPXCHG") == 0) {
    //DEBUG_FMT_PRINT_SAFE("[T%lx] internal libc lock(0x%lx)\n",
    //                     curr_thd_id, addr);
    DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
    curr_ls->Add(addr);
  }
}

void Predictor::AfterPthreadCreate(thread_id_t curr_thd_id,
                                   timestamp_t curr_thd_clk, Inst *inst,
                                   thread_id_t child_thd_id) {
  // empty
}

void Predictor::AfterPthreadJoin(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 thread_id_t child_thd_id) {
  ScopedLock locker(internal_lock_);

  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  VectorClock *child_exit_vc = exit_vc_map_[child_thd_id];
  curr_vc->Join(child_exit_vc);
}

void Predictor::AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                      timestamp_t curr_thd_clk, Inst *inst,
                                      address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorMutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, meta);
}

void Predictor::BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorMutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, meta);
}

void Predictor::BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorCondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForNotify(curr_thd_id, meta);
}

void Predictor::BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk, Inst *inst,
                                           address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorCondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForNotify(curr_thd_id, meta);
}

void Predictor::BeforePthreadCondWait(thread_id_t curr_thd_id,
                                      timestamp_t curr_thd_clk, Inst *inst,
                                      address_t cond_addr,
                                      address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  PredictorMutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, mutex_meta);
  // wait
  PredictorCondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  UpdateBeforeWait(curr_thd_id, cond_meta);
}

void Predictor::AfterPthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr,
                                     address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // wait
  PredictorCondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  UpdateAfterWait(curr_thd_id, cond_meta);
  // lock
  PredictorMutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, mutex_meta);
}

void Predictor::BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk, Inst *inst,
                                           address_t cond_addr,
                                           address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  PredictorMutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, mutex_meta);
  // wait
  PredictorCondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  UpdateBeforeWait(curr_thd_id, cond_meta);
}

void Predictor::AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // wait
  PredictorCondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  UpdateAfterWait(curr_thd_id, cond_meta);
  // lock
  PredictorMutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, mutex_meta);
}

void Predictor::BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorBarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateBeforeBarrier(curr_thd_id, meta);
}

void Predictor::AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  PredictorBarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateAfterBarrier(curr_thd_id, meta);
}

void Predictor::AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

void Predictor::AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t nmemb, size_t size,
                            address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void Predictor::BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t ori_addr, size_t size) {
  FreeAddrRegion(ori_addr);
}

void Predictor::AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size,
                             address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void Predictor::BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, address_t addr) {
  FreeAddrRegion(addr);
}

void Predictor::AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

PredictorMemMeta *Predictor::GetMemMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    PredictorMemMeta *meta = new PredictorMemMeta(iaddr);
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    PredictorMemMeta *meta = dynamic_cast<PredictorMemMeta *>(it->second);
    return meta; // could be NULL
  }
}

PredictorMutexMeta *Predictor::GetMutexMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    PredictorMutexMeta *meta = new PredictorMutexMeta(iaddr);
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    PredictorMutexMeta *meta = dynamic_cast<PredictorMutexMeta *>(it->second);
    if (meta) {
      return meta;
    } else {
      delete it->second;
      meta = new PredictorMutexMeta(iaddr);
      it->second = meta;
      return meta;
    }
  }
}

PredictorCondMeta *Predictor::GetCondMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    PredictorCondMeta *meta = new PredictorCondMeta(iaddr);
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    PredictorCondMeta *meta = dynamic_cast<PredictorCondMeta *>(it->second);
    if (meta) {
      return meta;
    } else {
      delete it->second;
      meta = new PredictorCondMeta(iaddr);
      it->second = meta;
      return meta;
    }
  }
}

PredictorBarrierMeta *Predictor::GetBarrierMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    PredictorBarrierMeta *meta = new PredictorBarrierMeta(iaddr);
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    PredictorBarrierMeta *meta =
        dynamic_cast<PredictorBarrierMeta *>(it->second);
    if (meta) {
      return meta;
    } else {
      delete it->second;
      meta = new PredictorBarrierMeta(iaddr);
      it->second = meta;
      return meta;
    }
  }
}

void Predictor::AllocAddrRegion(address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(addr && size);
  filter_->AddRegion(addr, size, false);
}

void Predictor::FreeAddrRegion(address_t addr) {
  ScopedLock locker(internal_lock_);

  if (!addr) return;
  size_t size = filter_->RemoveRegion(addr, false);
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    MetaMap::iterator it = meta_map_.find(iaddr);
    if (it != meta_map_.end()) {
      UpdateOnFree(it->second);
      delete it->second;
      meta_map_.erase(it);
    }
  }
}

bool Predictor::FilterAccess(address_t addr) {
  return filter_->Filter(addr, false);
}

bool Predictor::CheckLockSet(PredictorMemAccess *curr,
                             PredictorMemAccess *curr_prev,
                             PredictorMemAccess *rmt,
                             PredictorMemAccess *rmt_next) {
  DEBUG_ASSERT(curr && rmt);

  if (racy_only_ && !CheckRace(curr, rmt))
    return false;

  if (curr->ls_.Empty() || rmt->ls_.Empty())
    return true;

  if (curr->ls_.Disjoint(&rmt->ls_))
    return true;

  if (!curr_prev && !rmt_next) {
    return true;

  } else if (curr_prev && !rmt_next) {
    if (rmt->ls_.Disjoint(&curr->ls_, &curr_prev->ls_))
      return true;

  } else if (!curr_prev && rmt_next) {
    if (curr->ls_.Disjoint(&rmt->ls_, &rmt_next->ls_))
      return true;

  } else {
    if (curr->ls_.Disjoint(&rmt->ls_, &rmt_next->ls_) &&
        rmt->ls_.Disjoint(&curr->ls_, &curr_prev->ls_))
      return true;

  }

  return false;
}

bool Predictor::CheckLockSet(PredictorMutexAccess *curr,
                             PredictorMutexAccess *curr_prev,
                             PredictorMutexAccess *rmt,
                             PredictorMutexAccess *rmt_next) {
  DEBUG_ASSERT(curr && rmt);

  if (racy_only_ && !CheckRace(curr, rmt))
    return false;

  if (curr->ls_.Empty() || rmt->ls_.Empty())
    return true;

  if (curr->ls_.Disjoint(&rmt->ls_))
    return true;

  if (!curr_prev && !rmt_next) {
    return true;

  } else if (curr_prev && !rmt_next) {
    if (rmt->ls_.Disjoint(&curr->ls_, &curr_prev->ls_))
      return true;

  } else if (!curr_prev && rmt_next) {
    if (curr->ls_.Disjoint(&rmt->ls_, &rmt_next->ls_))
      return true;

  } else {
    if (curr->ls_.Disjoint(&rmt->ls_, &rmt_next->ls_) &&
        rmt->ls_.Disjoint(&curr->ls_, &curr_prev->ls_))
      return true;

  }

  return false;
}

bool Predictor::CheckRace(PredictorMemAccess *curr, PredictorMemAccess *rmt) {
  // return true if curr and rmt are not racy
  return curr->ls_.Disjoint(&rmt->ls_);
}

bool Predictor::CheckRace(PredictorMutexAccess *curr,
                          PredictorMutexAccess *rmt) {
  // return true if curr and rmt are not racy
  return curr->ls_.Disjoint(&rmt->ls_);
}

bool Predictor::CheckAsync(thread_id_t thd_id) {
  // return true if we need to set async for the iroot
  if (async_map_[thd_id])
    return true;
  else
    return false;
}

bool Predictor::CheckAsync(thread_id_t thd_id, timestamp_t clk) {
  // return true if we need to set async for the iroot
  if (async_map_[thd_id]) {
    timestamp_t start_time = async_start_time_map_[thd_id];
    if (clk > start_time)
      return true;
    else
      return false;
  } else {
    return false;
  }
}

bool Predictor::ValidPair(iRootEventType prev_type, iRootEventType curr_type) {
  if ((curr_type == IROOT_EVENT_MUTEX_UNLOCK &&
          prev_type == IROOT_EVENT_MUTEX_LOCK) ||
      (curr_type == IROOT_EVENT_MUTEX_LOCK &&
          prev_type == IROOT_EVENT_MUTEX_LOCK) ||
      (curr_type == IROOT_EVENT_MUTEX_UNLOCK &&
          prev_type == IROOT_EVENT_MUTEX_UNLOCK))
    return false;
  return true;
}

void Predictor::UpdateOnThreadExit(thread_id_t thd_id) {
  for (MetaMap::iterator it = meta_map_.begin(); it != meta_map_.end(); ++it) {
    PredictorMemMeta *mem_meta
        = dynamic_cast<PredictorMemMeta *>(it->second);
    if (mem_meta) {
      UpdateOnThreadExit(thd_id, mem_meta);
    }

    PredictorMutexMeta *mutex_meta
        = dynamic_cast<PredictorMutexMeta *>(it->second);
    if (mutex_meta) {
      UpdateOnThreadExit(thd_id, mutex_meta);
    }
  }
}

void Predictor::UpdateOnFree(PredictorMeta *meta) {
  PredictorMemMeta *mem_meta = dynamic_cast<PredictorMemMeta *>(meta);
  if (mem_meta) {
    UpdateOnFree(mem_meta);
  }

  PredictorMutexMeta *mutex_meta = dynamic_cast<PredictorMutexMeta *>(meta);
  if (mutex_meta) {
    UpdateOnFree(mutex_meta);
  }
}

bool Predictor::CheckShared(thread_id_t curr_thd_id, Inst *inst,
                            PredictorMemMeta *meta) {
  if (meta->shared_)
    return true;

  if (sinst_db_->Shared(inst)) {
    meta->history_ = new PredictorMemMeta::AccessHistory;
    meta->shared_ = true;
    return true;
  }

  if (meta->last_access_thd_id_ == INVALID_THD_ID) {
    meta->last_access_thd_id_ = curr_thd_id;
    return false;
  }

  if (meta->last_access_thd_id_ == curr_thd_id)
    return false;

  meta->history_ = new PredictorMemMeta::AccessHistory;
  meta->shared_ = true;
  return true;
}

void Predictor::UpdateForRead(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk, Inst *inst,
                              PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc && curr_ls);

  // create current access
  PredictorMemAccess curr_access(curr_thd_clk, IROOT_EVENT_MEM_READ,
                                 inst, curr_ls);

  // check monitoring
  if (!monitored_thd_map_[curr_thd_id]) {
    UpdateMemAccess(curr_thd_id, curr_vc, &curr_access, meta);
    return;
  }

  // predict iRoots
  VectorClock *curr_last_vc = FindLastVC(curr_thd_id, meta);
  PredictorMemAccess *curr_last_access = FindLastAccess(curr_thd_id, meta);

  typedef std::pair<VectorClock *, PredictorMemAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap precedent_access_map;
  TimedAccessMap precedent_candidate_map;

  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id == curr_thd_id) {
      // current thread
      if (curr_last_access) {
        DEBUG_ASSERT(curr_last_vc);
        if (curr_last_access->IsRead()) {
          FindSuccForRead(curr_thd_id, curr_last_access, curr_last_vc,
                          &curr_access, meta);
        } else {
          FindSuccForWrite(curr_thd_id, curr_last_access, curr_last_vc,
                           &curr_access, meta);
        }
      }
    } else {
      // remote thread
      PredictorMemMeta::PerThreadAccesses &accesses = mit->second;
      PredictorMemAccess *recent_access = NULL;
      bool precedent_first_reached = false;

      // iterate each vector clock value
      for (PredictorMemMeta::PerThreadAccesses::reverse_iterator lit =
              accesses.rbegin(); lit != accesses.rend(); ++lit) {
        VectorClock &vc = lit->first;
        PredictorMemMeta::AccessVec &access_vec = lit->second;

        if (vc.HappensAfter(curr_vc)) {
          // successive accesses
          DEBUG_FMT_PRINT_SAFE("vc=%s\n", vc.ToString().c_str());
          DEBUG_FMT_PRINT_SAFE("curr_vc=%s\n", curr_vc->ToString().c_str());
          DEBUG_ASSERT(0); // impossible

        } else if (vc.HappensBefore(curr_vc)) {
          // precedent accesses
          bool search_stop = false;
          for (PredictorMemMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMemAccess &access = *vit;

            if (!precedent_first_reached) {
              if (access.IsWrite()) {
                if (monitored_thd_map_[thd_id]) {
                  if (recent_access) {
                    // check vector clock
                    if (!curr_last_access || !vc.HappensBefore(curr_last_vc)) {
                      // verify locksets
                      if (CheckLockSet(&curr_access, curr_last_access,
                                       &access, recent_access)) {
                        precedent_candidate_map[thd_id]
                            = TimedAccess(&vc, &access);
                      }
                    }
                  }
                }
                precedent_access_map[thd_id] = TimedAccess(&vc, &access);
                search_stop = true;
                break;
              }
              precedent_first_reached = true;
            } else {
              if (access.IsWrite()) {
                precedent_access_map[thd_id] = TimedAccess(&vc, &access);
                search_stop = true;
                break;
              }
            }
            recent_access = &access;
          } // end of for each precedent access

          if (search_stop)
            break;

        } else {
          // concurrent accesses
          for (PredictorMemMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMemAccess &access = *vit;

            if (monitored_thd_map_[thd_id]) {
              if (access.IsWrite()) {
                if (recent_access) {
                  if (CheckLockSet(&curr_access, curr_last_access,
                                   &access, recent_access)) {
                    UpdateMemo(thd_id, access, curr_thd_id, curr_access);
                  }
                }
              }
            }
            recent_access = &access;
          } // end of for each concurrent access

        } // end of concurrent accesses
      } // end of iterate vector clock values
    } // end of else remote thread
  } // end of for each thread

  // update memorization for precedent accesses
  for (TimedAccessMap::iterator it = precedent_candidate_map.begin();
       it != precedent_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMemAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensBefore(curr_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate precedent access map
    for (TimedAccessMap::iterator inner_it = precedent_access_map.begin();
         inner_it != precedent_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensBefore(curr_vc));
      if (vc->HappensBefore(inner_vc)) {
        // inner_vc is the in the middle of curr_vc and vc
        // therefore, access -> curr_access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(thd_id, *access, curr_thd_id, curr_access);
    }
  }

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(curr_thd_id, &curr_access, meta);

  // update meta data
  UpdateMemAccess(curr_thd_id, curr_vc, &curr_access, meta);
}

void Predictor::UpdateForWrite(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc && curr_ls);

  // create current access
  PredictorMemAccess curr_access(curr_thd_clk, IROOT_EVENT_MEM_WRITE,
                                 inst, curr_ls);

  // check monitoring
  if (!monitored_thd_map_[curr_thd_id]) {
    UpdateMemAccess(curr_thd_id, curr_vc, &curr_access, meta);
    return;
  }

  // predict iRoots
  VectorClock *curr_last_vc = FindLastVC(curr_thd_id, meta);
  PredictorMemAccess *curr_last_access = FindLastAccess(curr_thd_id, meta);

  typedef std::pair<VectorClock *, PredictorMemAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap precedent_access_map;
  TimedAccessMap precedent_candidate_map;

  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id == curr_thd_id) {
      // current thread
      if (curr_last_access) {
        DEBUG_ASSERT(curr_last_vc);
        if (curr_last_access->IsRead()) {
          FindSuccForRead(curr_thd_id, curr_last_access, curr_last_vc,
                          &curr_access, meta);
        } else {
          FindSuccForWrite(curr_thd_id, curr_last_access, curr_last_vc,
                           &curr_access, meta);
        }
      }

    } else {
      // remote thread
      PredictorMemMeta::PerThreadAccesses &accesses = mit->second;
      PredictorMemAccess *recent_access = NULL;
      bool precedent_first_reached = false;

      // iterate each vector clock value
      for (PredictorMemMeta::PerThreadAccesses::reverse_iterator lit =
              accesses.rbegin(); lit != accesses.rend(); ++lit) {
        VectorClock &vc = lit->first;
        PredictorMemMeta::AccessVec &access_vec = lit->second;

        if (vc.HappensAfter(curr_vc)) {
          // successive accesses
          DEBUG_ASSERT(0); // impossible

        } else if (vc.HappensBefore(curr_vc)) {
          // precedent accesses
          bool search_stop = false;
          for (PredictorMemMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMemAccess &access = *vit;

            if (!precedent_first_reached) {
              if (monitored_thd_map_[thd_id]) {
                if (recent_access) {
                  // check vector clock
                  if (!curr_last_access || !vc.HappensBefore(curr_last_vc)) {
                    // check locksets
                    if (CheckLockSet(&curr_access, curr_last_access,
                                     &access, recent_access)) {
                      precedent_candidate_map[thd_id]
                        = TimedAccess(&vc, &access);
                    }
                  }
                }
              }
              if (access.IsWrite()) {
                precedent_access_map[thd_id] = TimedAccess(&vc, &access);
                search_stop = true;
                break;
              }
              precedent_first_reached = true;
            } else {
              if (access.IsWrite()) {
                precedent_access_map[thd_id] = TimedAccess(&vc, &access);
                search_stop = true;
                break;
              }
            }
            recent_access = &access;
          } // end of for each precedent access

          if (search_stop)
            break;

        } else {
          // concurrent accesses
          for (PredictorMemMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMemAccess &access = *vit;

            if (monitored_thd_map_[thd_id]) {
              if (recent_access) {
                // check lock set
                if (CheckLockSet(&curr_access, curr_last_access,
                                 &access, recent_access)) {
                  UpdateMemo(thd_id, access, curr_thd_id, curr_access);
                }
              }
            }
            recent_access = &access;
          }

        } // end of else concurrent accesses
      } // end of for each vector clock value
    } // end of else remote thread
  } // end of for each thread

  // update memorization for precedent accesses
  for (TimedAccessMap::iterator it = precedent_candidate_map.begin();
       it != precedent_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMemAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensBefore(curr_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate precedent access map
    for (TimedAccessMap::iterator inner_it = precedent_access_map.begin();
         inner_it != precedent_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      DEBUG_ASSERT(inner_it->first != curr_thd_id);

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensBefore(curr_vc));
      if (vc->HappensBefore(inner_vc)) {
        // inner_vc is the intermediate between curr_vc and vc
        // therefore, access -> curr_access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(thd_id, *access, curr_thd_id, curr_access);
    }
  }

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(curr_thd_id, &curr_access, meta);

  // update meta data
  UpdateMemAccess(curr_thd_id, curr_vc, &curr_access, meta);
}

void Predictor::UpdateOnThreadExit(thread_id_t exit_thd_id,
                                   PredictorMemMeta *meta) {
  if (!meta->history_)
    return;

  // for the last access, find its successors
  VectorClock *last_vc = FindLastVC(exit_thd_id, meta);
  PredictorMemAccess *last_access = FindLastAccess(exit_thd_id, meta);

  if (last_access) {
    if (last_access->IsRead()) {
      FindSuccForRead(exit_thd_id, last_access, last_vc, NULL, meta);
    } else {
      FindSuccForWrite(exit_thd_id, last_access, last_vc, NULL, meta);
    }
  }
}

void Predictor::UpdateOnFree(PredictorMemMeta *meta) {
  if (!meta->history_)
    return;

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    VectorClock *last_vc = FindLastVC(thd_id, meta);
    PredictorMemAccess *last_access = FindLastAccess(thd_id, meta);
    if (last_access) {
      if (last_access->IsRead()) {
        FindSuccForRead(thd_id, last_access, last_vc, NULL, meta);
      } else {
        FindSuccForWrite(thd_id, last_access, last_vc, NULL, meta);
      }
    }
  }
}

void Predictor::FindSuccForRead(thread_id_t curr_thd_id,
                                PredictorMemAccess *curr_reader,
                                VectorClock *curr_reader_vc,
                                PredictorMemAccess *curr_next,
                                PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);
  DEBUG_ASSERT(curr_reader && curr_reader_vc);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;

  typedef std::pair<VectorClock *, PredictorMemAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap successive_access_map;
  TimedAccessMap successive_candidate_map;

  // check monitoring
  if (!monitored_thd_map_[curr_thd_id])
    return;

  // iterate all remote threads
  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id == curr_thd_id)
      continue;

    // remote thread
    PredictorMemMeta::PerThreadAccesses &accesses = mit->second;
    PredictorMemAccess *recent_access = NULL;
    bool successive_first_reached = false;

    // iterate all the accesses in this thread
    for (PredictorMemMeta::PerThreadAccesses::iterator lit =
            accesses.begin(); lit != accesses.end(); ++lit) {
      VectorClock &vc = lit->first;
      PredictorMemMeta::AccessVec &access_vec = lit->second;

      if (vc.HappensBefore(curr_reader_vc)) {
        // precedent accesses
        // impossible to be the successor of curr reader, ignore
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;
          recent_access = &access;
        }

      } else if (vc.HappensAfter(curr_reader_vc)) {
        // successive accesses
        bool search_stop = false;
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;

          if (!successive_first_reached) {
            if (access.IsWrite()) {
              if (monitored_thd_map_[thd_id]) {
                // check lock set
                if (CheckLockSet(&access, recent_access, curr_reader,
                                 curr_next)) {
                  successive_candidate_map[thd_id] = TimedAccess(&vc, &access);
                }
              }
              successive_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            }
            successive_first_reached = true;
          } else {
            if (access.IsWrite()) {
              successive_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            }
          }
          recent_access = &access;
        }

        if (search_stop)
          break;

      } else {
        // concurrent accesses
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;

          if (monitored_thd_map_[thd_id]) {
            if (access.IsWrite()) {
              // check lock sets
              if (CheckLockSet(&access, recent_access, curr_reader,
                               curr_next)) {
                UpdateMemo(curr_thd_id, *curr_reader, thd_id, access);
              }
            }
          }
          recent_access = &access;
          // recent_access_vc = vc;
        }

      }
    } // end of for each vector clock value
  } // end of for each thread

  // update memorization for precedent accesses
  for (TimedAccessMap::iterator it = successive_candidate_map.begin();
       it != successive_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMemAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensAfter(curr_reader_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate successive access map
    for (TimedAccessMap::iterator inner_it = successive_access_map.begin();
         inner_it != successive_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensAfter(curr_reader_vc));
      if (vc->HappensAfter(inner_vc)) {
        // inner_vc is in the middle of curr_vc and vc
        // therefore, access -> curr_access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(curr_thd_id, *curr_reader, thd_id, *access);
    }
  }
}

void Predictor::FindSuccForWrite(thread_id_t curr_thd_id,
                                 PredictorMemAccess *curr_writer,
                                 VectorClock *curr_writer_vc,
                                 PredictorMemAccess *curr_next,
                                 PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);
  DEBUG_ASSERT(curr_writer && curr_writer_vc);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;

  typedef std::pair<VectorClock *, PredictorMemAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap successive_access_map;
  TimedAccessMap successive_candidate_map;

  if (!monitored_thd_map_[curr_thd_id])
    return;

  // iterate all remote threads
  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id == curr_thd_id)
      continue;

    // remote thread
    PredictorMemMeta::PerThreadAccesses &accesses = mit->second;
    PredictorMemAccess *recent_access = NULL;
    bool successive_first_reached = false;

    // iterate all the accesses in this thread
    for (PredictorMemMeta::PerThreadAccesses::iterator lit = accesses.begin();
         lit != accesses.end(); ++lit) {
      VectorClock &vc = lit->first;
      PredictorMemMeta::AccessVec &access_vec = lit->second;

      if (vc.HappensBefore(curr_writer_vc)) {
        // precedent accesses
        // impossible to be the successor of curr writer, ignore
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;
          recent_access = &access;
        }

      } else if (vc.HappensAfter(curr_writer_vc)) {
        // successive accesses
        bool search_stop = false;
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;

          if (!successive_first_reached) {
            if (monitored_thd_map_[thd_id]) {
              // check lock set
              if (CheckLockSet(&access, recent_access, curr_writer,
                               curr_next)) {
                successive_candidate_map[thd_id] = TimedAccess(&vc, &access);
              }
            }
            if (access.IsWrite()) {
              successive_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            }
            successive_first_reached = true;
          } else {
            if (access.IsWrite()) {
              successive_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            }
          }
          recent_access = &access;
        }

        if (search_stop)
          break;

      } else {
        // concurrent accesses
        for (PredictorMemMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMemAccess &access = *vit;

          if (monitored_thd_map_[thd_id]) {
            // check lock set
            if (CheckLockSet(&access, recent_access, curr_writer, curr_next)) {
              UpdateMemo(curr_thd_id, *curr_writer, thd_id, access);
            }
          }
          recent_access = &access;
        }

      } // end of else concurrent accesses
    } // end of for each vector clock value
  } // end of for each thread

  // update memorization for precedent accesses
  for (TimedAccessMap::iterator it = successive_candidate_map.begin();
       it != successive_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMemAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensAfter(curr_writer_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate successive access map
    for (TimedAccessMap::iterator inner_it = successive_access_map.begin();
         inner_it != successive_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensAfter(curr_writer_vc));
      if (vc->HappensAfter(inner_vc)) {
        // inner_vc is in the middle of curr_vc and vc
        // therefore, curr_writer -> access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(curr_thd_id, *curr_writer, thd_id, *access);
    }
  }
}

void Predictor::UpdateMemAccess(thread_id_t thd_id, VectorClock *vc,
                                PredictorMemAccess *access,
                                PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  PredictorMemMeta::PerThreadAccesses &per_thd_accesses = access_map[thd_id];

  if (per_thd_accesses.empty()) {
    PredictorMemMeta::AccessVec access_vec;
    access_vec.push_back(*access);
    per_thd_accesses.push_back(
        PredictorMemMeta::TimedAccessVec(*vc, access_vec));
    meta->history_->last_gc_vec_size[thd_id] = 0;
  } else {
    // obtain the last vector clock and the associated access vector
    VectorClock &last_vc = per_thd_accesses.back().first;
    PredictorMemMeta::AccessVec &last_access_vec
        = per_thd_accesses.back().second;
    if (last_vc.Equal(vc)) {
      // no need to create a new vector clock value
      last_access_vec.push_back(*access);
      // selectively compress the last_access_vec depending on gc status
      CheckCompress(thd_id, &last_access_vec, meta);
    } else {
      DEBUG_ASSERT(last_vc.HappensBefore(vc));
      // compress last_access_vec
      Compress(&last_access_vec, meta);
      // create a new access_vec
      PredictorMemMeta::AccessVec access_vec;
      access_vec.push_back(*access);
      per_thd_accesses.push_back(
          PredictorMemMeta::TimedAccessVec(*vc, access_vec));
      meta->history_->last_gc_vec_size[thd_id] = 0;
      CheckGC(meta);
    }
  }
}

VectorClock *Predictor::FindLastVC(thread_id_t thd_id, PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  PredictorMemMeta::PerThreadAccesses &accesses = access_map[thd_id];
  if (accesses.empty()) {
    return NULL;
  } else {
    return &accesses.back().first;
  }
}

PredictorMemAccess *Predictor::FindLastAccess(thread_id_t thd_id,
                                              PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;
  PredictorMemMeta::PerThreadAccesses &accesses = access_map[thd_id];
  if (accesses.empty()) {
    return NULL;
  } else {
    DEBUG_ASSERT(!accesses.back().second.empty());
    return &accesses.back().second.back();
  }
}

bool Predictor::CheckGC(PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  GC(meta);

  return true;
}

bool Predictor::CheckCompress(thread_id_t thd_id,
                              PredictorMemMeta::AccessVec *access_vec,
                              PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  size_t last_gc_size = meta->history_->last_gc_vec_size[thd_id];
  size_t curr_size = access_vec->size();

  if (curr_size < last_gc_size)
    return false;

  if (curr_size - last_gc_size < 70)
    return false;

  Compress(access_vec, meta);
  meta->history_->last_gc_vec_size[thd_id] = access_vec->size();
  return true;
}

void Predictor::GC(PredictorMemMeta *meta) {
  // only do GC for shared meta
  if (!meta->history_)
    return;

  PredictorMemMeta::AccessMap &access_map = meta->history_->access_map;

  std::map<thread_id_t, VectorClock *> last_access_vc_table;
  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    VectorClock *last_vc = FindLastVC(thd_id, meta);
    if (last_vc) {
      last_access_vc_table[thd_id] = last_vc;
    }
  }

  // iterate each thread access history
  for (PredictorMemMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;
    PredictorMemMeta::PerThreadAccesses &accesses = mit->second;

    // reverse iterate each vector clock value
    PredictorMemMeta::PerThreadAccesses::iterator lit;
    for (lit = accesses.end(); lit != accesses.begin(); ) {
      --lit;

      VectorClock &vc = lit->first;

      // check whether vc is earlier than all curr_vc && last_access_vc
      // if yes, clear the remaining access vectors
      bool collect = true;
      for (std::map<thread_id_t, VectorClock *>::iterator vcit =
              curr_vc_map_.begin(); vcit != curr_vc_map_.end(); ++vcit) {
        // skip the same thread
        if (vcit->first == thd_id)
          continue;

        if (!vc.HappensBefore(vcit->second)) {
          collect = false;
          break;
        }

        std::map<thread_id_t, VectorClock *>::iterator luit =
            last_access_vc_table.find(vcit->first);
        if (luit != last_access_vc_table.end()) {
          if (!vc.HappensBefore(luit->second)) {
            collect = false;
            break;
          }
        }
      }

      if (collect) {
        break;
      }
    }

    // delete [begin(), lit)
    for (PredictorMemMeta::PerThreadAccesses::iterator it = lit;
         it != accesses.begin(); ) {
      --it;
      //DEBUG_FMT_PRINT_SAFE("<GC> %lu mem accesses in T%lx are collected\n",
      //                     (*it).second.size(), thd_id);
    }

    accesses.erase(accesses.begin(), lit);
  } // end for each thread
}

void Predictor::Compress(PredictorMemMeta::AccessVec *access_vec,
                         PredictorMemMeta *meta) {
  DEBUG_ASSERT(meta->history_);

  // resultant list
  std::list<PredictorMemAccess> results;

  // reverse iterate access_vec
  for (PredictorMemMeta::AccessVec::reverse_iterator vit =
          access_vec->rbegin(); vit != access_vec->rend(); ++vit) {
    PredictorMemAccess &access = *vit;

    // iterate the results to find duplicates
    bool duplicate = false;
    for (std::list<PredictorMemAccess>::iterator it = results.begin();
         it != results.end(); ++it) {
      if (it->type_ == access.type_ &&
          it->inst_ == access.inst_ &&
          it->ls_.Match(&access.ls_)) {
        // duplicate
        duplicate = true;
        break;
      }
    }

    if (!duplicate) {
      results.push_front(access);
    }
  }

  // recreate access_vec
  access_vec->clear();
  for (std::list<PredictorMemAccess>::iterator it = results.begin();
       it != results.end(); ++it) {
    access_vec->push_back(*it);
  }
}

void Predictor::UpdateForLock(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk, Inst *inst,
                              PredictorMutexMeta *meta) {
  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc && curr_ls);
  address_t lock_addr = meta->addr_;

  // create current access
  PredictorMutexAccess curr_access(curr_thd_clk, IROOT_EVENT_MUTEX_LOCK,
                                   inst, curr_ls);

  // check monitoring
  if (!monitored_thd_map_[curr_thd_id]) {
    curr_ls->Add(lock_addr);
    UpdateMutexAccess(curr_thd_id, curr_vc, &curr_access, meta);
    return;
  }

  VectorClock *curr_last_vc = FindLastVC(curr_thd_id, meta);
  PredictorMutexAccess *curr_last_access = FindLastAccess(curr_thd_id, meta);
  DEBUG_ASSERT(!curr_last_access || curr_last_access->IsUnlock());

  typedef std::pair<VectorClock *, PredictorMutexAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap precedent_access_map;
  TimedAccessMap precedent_candidate_map;

  for (PredictorMutexMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id == curr_thd_id) {
      // current thread
      if (curr_last_access) {
        DEBUG_ASSERT(curr_last_vc);
        FindSuccForUnlock(curr_thd_id, curr_last_access, curr_last_vc,
                          &curr_access, meta);
      }

    } else {
      // remote thread
      PredictorMutexMeta::PerThreadAccesses &accesses = mit->second;
      PredictorMutexAccess *recent_lock = NULL;

      // reverse iterate all the accesses in the remote thread
      for (PredictorMutexMeta::PerThreadAccesses::reverse_iterator lit =
              accesses.rbegin(); lit != accesses.rend(); ++lit) {
        VectorClock &vc = lit->first;
        PredictorMutexMeta::AccessVec &access_vec = lit->second;

        if (vc.HappensAfter(curr_vc)) {
          // successive accesses
          DEBUG_ASSERT(0); // impossible

        } else if (vc.HappensBefore(curr_vc)) {
          // precedent accesses
          bool search_stop = false;
          for (PredictorMutexMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMutexAccess &access = *vit;

            if (access.IsLock()) {
              precedent_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            } else {
              if (monitored_thd_map_[thd_id]) {
                if (recent_lock) {
                  // check vector clock
                  if (!curr_last_access || !vc.HappensBefore(curr_last_vc)) {
                    // check lock set
                    if (CheckLockSet(&curr_access, curr_last_access,
                                     &access, recent_lock)) {
                      precedent_candidate_map[thd_id]
                          = TimedAccess(&vc, &access);
                    }
                  }
                }
              }
              precedent_access_map[thd_id] = TimedAccess(&vc, &access);
              search_stop = true;
              break;
            }
          }

          if (search_stop)
            break;

        } else {
          // concurrent accesses
          for (PredictorMutexMeta::AccessVec::reverse_iterator vit =
                  access_vec.rbegin(); vit != access_vec.rend(); ++vit) {
            PredictorMutexAccess &access = *vit;
            // if last concurrent lock of this thread is not hit, no need
            // to proceed since we cannot decide the lock set of its immediate
            // successive unlock
            if (access.IsLock()) {
              recent_lock = &access;
            } else {
              if (monitored_thd_map_[thd_id]) {
                // check lock set
                if (recent_lock) {
                  if (CheckLockSet(&curr_access, curr_last_access,
                                   &access, recent_lock)) {
                    UpdateMemo(thd_id, access, curr_thd_id, curr_access);
                  }
                }
              }
            }
          } // end of for each access in the access_vec

        } // end of else concurrent accesses
      } // end of for each vector clock value
    } // end of else remote thread
  } // end of for each thread

  // update memorization for precedent accesses
  for (TimedAccessMap::iterator it = precedent_candidate_map.begin();
       it != precedent_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMutexAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensBefore(curr_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate precedent access map
    for (TimedAccessMap::iterator inner_it = precedent_access_map.begin();
         inner_it != precedent_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensBefore(curr_vc));
      if (vc->HappensBefore(inner_vc)) {
        // inner_vc is in the middle of curr_vc and vc
        // therefore, access -> curr_access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(thd_id, *access, curr_thd_id, curr_access);
    }
  }

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(curr_thd_id, &curr_access, meta);

  // update meta data
  curr_ls->Add(lock_addr);
  UpdateMutexAccess(curr_thd_id, curr_vc, &curr_access, meta);
}

void Predictor::UpdateForUnlock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                PredictorMutexMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  LockSet *curr_ls = curr_ls_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc && curr_ls);
  address_t lock_addr = meta->addr_;

  // update lock set
  curr_ls->Remove(lock_addr);
  // create current access
  PredictorMutexAccess curr_access(curr_thd_clk, IROOT_EVENT_MUTEX_UNLOCK,
                                   inst, curr_ls);

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(curr_thd_id, &curr_access, meta);

  // update meta data
  UpdateMutexAccess(curr_thd_id, curr_vc, &curr_access, meta);
}

void Predictor::UpdateOnThreadExit(thread_id_t exit_thd_id,
                                   PredictorMutexMeta *meta) {
  VectorClock *last_vc = FindLastVC(exit_thd_id, meta);
  PredictorMutexAccess *last_access = FindLastAccess(exit_thd_id, meta);

  if (last_access && last_access->IsUnlock()) {
    FindSuccForUnlock(exit_thd_id, last_access, last_vc, NULL, meta);
  }
}

void Predictor::UpdateOnFree(PredictorMutexMeta *meta) {
  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;
  for (PredictorMutexMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    VectorClock *last_vc = FindLastVC(thd_id, meta);
    PredictorMutexAccess *last_access = FindLastAccess(thd_id, meta);
    if (last_access && last_access->IsUnlock()) {
      FindSuccForUnlock(thd_id, last_access, last_vc, NULL, meta);
    }
  }
}

void Predictor::FindSuccForUnlock(thread_id_t curr_thd_id,
                                  PredictorMutexAccess *curr_unlock,
                                  VectorClock *curr_unlock_vc,
                                  PredictorMutexAccess *curr_next,
                                  PredictorMutexMeta *meta) {
  DEBUG_ASSERT(curr_unlock && curr_unlock_vc);

  if (!monitored_thd_map_[curr_thd_id])
    return;

  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;

  typedef std::pair<VectorClock *, PredictorMutexAccess *> TimedAccess;
  typedef std::map<thread_id_t, TimedAccess> TimedAccessMap;
  TimedAccessMap successive_access_map;
  TimedAccessMap successive_candidate_map;

  // iterate all remote threads
  for (PredictorMutexMeta::AccessMap::iterator mit = access_map.begin();
       mit != access_map.end(); ++mit) {
    thread_id_t thd_id = mit->first;

    if (thd_id  == curr_thd_id)
      continue;

    // remote threads
    PredictorMutexMeta::PerThreadAccesses &accesses = mit->second;
    PredictorMutexAccess *recent_unlock = NULL;

    // iterate all the accesses in remote thread
    for (PredictorMutexMeta::PerThreadAccesses::iterator lit = accesses.begin();
         lit != accesses.end(); ++lit) {
      VectorClock &vc = lit->first;
      PredictorMutexMeta::AccessVec &access_vec = lit->second;

      if (vc.HappensBefore(curr_unlock_vc)) {
        // impossible to be the successor or curr_unlock, ignore
        for (PredictorMutexMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMutexAccess &access = *vit;

          if (access.IsUnlock()) {
            recent_unlock = &access;
          }
        }

      } else if (vc.HappensAfter(curr_unlock_vc)) {
        // successive accesses
        bool search_stop = false;
        for (PredictorMutexMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMutexAccess &access = *vit;

          if (access.IsUnlock()) {
            successive_access_map[thd_id] = TimedAccess(&vc, &access);
            search_stop = true;
            break; // no need to proceed
          } else {
            if (monitored_thd_map_[thd_id]) {
              // check lock set
              if (CheckLockSet(&access, recent_unlock, curr_unlock,
                               curr_next)) {
                successive_candidate_map[thd_id] = TimedAccess(&vc, &access);
              }
            }
            successive_access_map[thd_id] = TimedAccess(&vc, &access);
            search_stop = true;
            break; // no need to proceed
          }
        }

        if (search_stop)
          break;

      } else {
        // concurrent accesses
        for (PredictorMutexMeta::AccessVec::iterator vit = access_vec.begin();
             vit != access_vec.end(); ++vit) {
          PredictorMutexAccess &access = *vit;

          if (access.IsUnlock()) {
            recent_unlock = &access;
            // recent_unlock_vc = vc;
          } else {
            if (monitored_thd_map_[thd_id]) {
              // check lock set
              if (CheckLockSet(&access, recent_unlock, curr_unlock,
                               curr_next)) {
                UpdateMemo(curr_thd_id, *curr_unlock, thd_id, access);
              }
            }
          }
        }

      } // end of else concurrent accesses
    } // end of for each vector clock value
  } // end of for each thread

  // update memorization for successive accesses
  for (TimedAccessMap::iterator it = successive_candidate_map.begin();
       it != successive_candidate_map.end(); ++it) {
    thread_id_t thd_id = it->first;
    VectorClock *vc = it->second.first;
    PredictorMutexAccess *access = it->second.second;

    DEBUG_ASSERT(vc->HappensAfter(curr_unlock_vc));
    DEBUG_ASSERT(access && vc && thd_id != curr_thd_id);

    bool feasible = true;
    // iterate successive access map
    for (TimedAccessMap::iterator inner_it = successive_access_map.begin();
         inner_it != successive_access_map.end(); ++inner_it) {
      // skip the same thread
      if (inner_it->first == thd_id)
        continue;

      VectorClock *inner_vc = inner_it->second.first;
      DEBUG_ASSERT(inner_vc->HappensAfter(curr_unlock_vc));
      if (vc->HappensAfter(inner_vc)) {
        // inner_vc is in the middle of curr_unlock_vc and vc
        // therefore, access -> curr_access is infeasible
        feasible = false;
        break;
      }
    }

    if (feasible) {
      DEBUG_ASSERT(monitored_thd_map_[thd_id]);
      UpdateMemo(curr_thd_id, *curr_unlock, thd_id, *access);
    }
  }
}

void Predictor::UpdateMutexAccess(thread_id_t thd_id, VectorClock *vc,
                                  PredictorMutexAccess *access,
                                  PredictorMutexMeta *meta) {
  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;
  PredictorMutexMeta::PerThreadAccesses &per_thd_accesses = access_map[thd_id];

  if (per_thd_accesses.empty()) {
    PredictorMutexMeta::AccessVec access_vec;
    access_vec.push_back(*access);
    per_thd_accesses.push_back(
        PredictorMutexMeta::TimedAccessVec(*vc, access_vec));
  } else {
    // obtain the last vector clock and the associated access vector
    VectorClock &last_vc = per_thd_accesses.back().first;
    if (last_vc.Equal(vc)) {
      // no need to create a new vector clock value
      PredictorMutexMeta::AccessVec &access_vec
          = per_thd_accesses.back().second;
      access_vec.push_back(*access);
    } else {
      DEBUG_ASSERT(last_vc.HappensBefore(vc));
      PredictorMutexMeta::AccessVec access_vec;
      access_vec.push_back(*access);
      per_thd_accesses.push_back(
          PredictorMutexMeta::TimedAccessVec(*vc, access_vec));
    }
  }
}

VectorClock *Predictor::FindLastVC(thread_id_t thd_id,
                                   PredictorMutexMeta *meta) {
  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;
  PredictorMutexMeta::PerThreadAccesses &accesses = access_map[thd_id];
  if (accesses.empty()) {
    return NULL;
  } else {
    return &accesses.back().first;
  }
}

PredictorMutexAccess *Predictor::FindLastAccess(thread_id_t thd_id,
                                                PredictorMutexMeta *meta) {
  PredictorMutexMeta::AccessMap &access_map = meta->history_.access_map;
  PredictorMutexMeta::PerThreadAccesses &accesses = access_map[thd_id];
  if (accesses.empty()) {
    return NULL;
  } else {
    DEBUG_ASSERT(!accesses.back().second.empty());
    return &accesses.back().second.back();
  }
}

void Predictor::UpdateBeforeWait(thread_id_t curr_thd_id,
                                 PredictorCondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  meta->wait_table_[curr_thd_id] = *curr_vc;
  curr_vc->Increment(curr_thd_id);
}

void Predictor::UpdateAfterWait(thread_id_t curr_thd_id,
                                PredictorCondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];

  // it is possible that waitpost does not depend on a signal or broadcast
  // this is because there exist wait_timeout sync functions
  PredictorCondMeta::VectorClockMap::iterator wit
      = meta->wait_table_.find(curr_thd_id);
  DEBUG_ASSERT(wit != meta->wait_table_.end());
  meta->wait_table_.erase(wit);

  PredictorCondMeta::VectorClockMap::iterator sit
      = meta->signal_table_.find(curr_thd_id);
  if (sit != meta->signal_table_.end()) {
    // join vector clock
    curr_vc->Join(&sit->second);
    meta->signal_table_.erase(sit);
  }
}

void Predictor::UpdateForNotify(thread_id_t curr_thd_id,
                                PredictorCondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);

  // iterate the wait table, join vector clock
  for (PredictorCondMeta::VectorClockMap::iterator it =
          meta->wait_table_.begin(); it != meta->wait_table_.end(); ++it) {
    curr_vc->Join(&it->second);
  }

  // update signal table
  for (PredictorCondMeta::VectorClockMap::iterator it =
          meta->wait_table_.begin(); it != meta->wait_table_.end(); ++it) {
    meta->signal_table_[it->first] = *curr_vc;
  }

  curr_vc->Increment(curr_thd_id);
}

void Predictor::UpdateBeforeBarrier(thread_id_t curr_thd_id,
                                    PredictorBarrierMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);

  // choose which table to use
  PredictorBarrierMeta::VectorClockMap *wait_table = NULL;
  if (meta->pre_using_table1_)
    wait_table = &meta->barrier_wait_table1_;
  else
    wait_table = &meta->barrier_wait_table2_;

  (*wait_table)[curr_thd_id] = std::pair<VectorClock, bool>(*curr_vc, false);
}

void Predictor::UpdateAfterBarrier(thread_id_t curr_thd_id,
                                   PredictorBarrierMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);

  // choose which table to use
  PredictorBarrierMeta::VectorClockMap *wait_table = NULL;
  if (meta->post_using_table1_)
    wait_table = &meta->barrier_wait_table1_;
  else
    wait_table = &meta->barrier_wait_table2_;

  bool all_flagged_ = true;
  bool all_not_flagged_ = true;
  for (PredictorBarrierMeta::VectorClockMap::iterator it = wait_table->begin();
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
    meta->pre_using_table1_ = !meta->pre_using_table1_;
  }

  if (all_flagged_) {
    // clear table
    wait_table->clear();
    meta->post_using_table1_ = !meta->post_using_table1_;
  }
}

void Predictor::UpdateMemo(thread_id_t src_thd_id, PredictorAccess &src_access,
                           thread_id_t dst_thd_id, PredictorAccess &dst_access){
  iRootEvent *e0 = iroot_db_->GetiRootEvent(src_access.inst_,
                                            src_access.type_,
                                            false);
  iRootEvent *e1 = iroot_db_->GetiRootEvent(dst_access.inst_,
                                            dst_access.type_,
                                            false);
  iRoot *iroot = iroot_db_->GetiRoot(IDIOM_1, false, e0, e1);
  memo_->Predicted(iroot, false);
  if (CheckAsync(src_thd_id, src_access.clk_) ||
      CheckAsync(dst_thd_id, dst_access.clk_)) {
    memo_->SetAsync(iroot, false);
  }
  if (complex_idioms_)
    UpdateDynEventMap(src_thd_id, src_access, dst_thd_id, dst_access);
}

void Predictor::UpdateDynEventMap(thread_id_t src_thd_id,
                                  PredictorAccess &src_access,
                                  thread_id_t dst_thd_id,
                                  PredictorAccess &dst_access) {
  // create src and dst dynamic event
  PredictorLocalInfo::DynEvent src;
  src.thd_id = src_thd_id;
  src.type = src_access.type_;
  src.inst = src_access.inst_;
  PredictorLocalInfo::DynEvent dst;
  dst.thd_id = dst_thd_id;
  dst.type = dst_access.type_;
  dst.inst = dst_access.inst_;
  // update dynamic event map and reverse dynamic event map
  PredictorLocalInfo::DynEventRangeMap &range_map
      = local_info_.dyn_event_map_[src];
  PredictorLocalInfo::DynEventRangeMap::iterator it = range_map.find(dst);
  if (it == range_map.end()) {
    PredictorLocalInfo::DynRange range;
    range.start = dst_access.clk_;
    range.end = dst_access.clk_;
    range_map[dst] = range;
  } else {
    it->second.end = dst_access.clk_;
  }

  PredictorLocalInfo::DynEventRangeMap &r_range_map
      = local_info_.r_dyn_event_map_[dst];
  PredictorLocalInfo::DynEventRangeMap::iterator r_it = r_range_map.find(src);
  if (r_it == r_range_map.end()) {
    PredictorLocalInfo::DynRange range;
    range.start = src_access.clk_;
    range.end = src_access.clk_;
    r_range_map[src] = range;
  } else {
    it->second.end = src_access.clk_;
  }
}

void Predictor::UpdateLocalInfo(thread_id_t curr_thd_id,
                                PredictorAccess *curr_access,
                                PredictorMeta *meta) {
  // do not update local info for local accesses
  if (!curr_access->IsSync()) {
    if (sinst_db_ && !sinst_db_->Shared(curr_access->inst_))
      return; // non shared instruction
  }

  timestamp_t curr_thd_clk = curr_access->clk_;
  address_t addr = meta->addr_;

  // get access list
  PredictorLocalInfo::EntryList &access_list
      = local_info_.access_map_[curr_thd_id];

  // iterate recent events, calculate distance
  std::tr1::unordered_set<address_t> touched_addr_set;
  for (PredictorLocalInfo::EntryList::reverse_iterator it
          = access_list.rbegin(); it != access_list.rend(); ++it) {
    if (TIME_DISTANCE((*it).clk, curr_thd_clk) < vw_) {
      if (touched_addr_set.find((*it).addr) != touched_addr_set.end())
        continue;
      // add to the touched address set
      touched_addr_set.insert((*it).addr);
      if ((*it).clk != curr_thd_clk) {
        // update pair db
        if (ValidPair((*it).type, curr_access->type_)) {
          PredictorLocalInfo::PairType local_pair;
          local_pair.curr_type = curr_access->type_;
          local_pair.curr_inst = curr_access->inst_;
          local_pair.prev_type = (*it).type;
          local_pair.prev_inst = (*it).inst;
          local_pair.same_addr = (addr == (*it).addr);
          local_pair.thd_id = curr_thd_id;
          local_info_.pair_db_.insert(local_pair);
        }
        // predict deadlock
        if (predict_deadlock_) {
          if (curr_access->type_ == IROOT_EVENT_MUTEX_LOCK &&
              (*it).type == IROOT_EVENT_MUTEX_LOCK &&
              addr != (*it).addr) {
            PredictorDeadlockInfo::PairType deadlock_pair;
            deadlock_pair.curr_type = curr_access->type_;
            deadlock_pair.curr_inst = curr_access->inst_;
            deadlock_pair.curr_addr = addr;
            deadlock_pair.prev_type = (*it).type;
            deadlock_pair.prev_inst = (*it).inst;
            deadlock_pair.prev_addr = (*it).addr;
            deadlock_pair.thd_id = curr_thd_id;
            deadlock_info_.pair_db_.insert(deadlock_pair);
          }
        }
      }
      // break if same addr access is found
      if ((*it).addr == addr) {
        break;
      }
    } else {
      break;
    }
  }

  // remove stale events
  while (!access_list.empty()) {
    PredictorLocalInfo::EntryList::iterator it = access_list.begin();
    if (TIME_DISTANCE((*it).clk, curr_thd_clk) >= vw_) {
      access_list.erase(it);
    } else {
      break;
    }
  }

  // push the current event
  PredictorLocalInfo::EntryType access;
  access.clk = curr_thd_clk;
  access.addr = addr;
  access.type = curr_access->type_;
  access.inst = curr_access->inst_;
  access_list.push_back(access);
}

void Predictor::UpdateComplexiRoots() {
  DEBUG_FMT_PRINT_SAFE("Num local pairs = %lu\n", local_info_.pair_db_.size());
  DEBUG_FMT_PRINT_SAFE("Dyn event map size = %lu\n",
                       local_info_.dyn_event_map_.size());
  DEBUG_FMT_PRINT_SAFE("Reverse dyn event map size = %lu\n",
                       local_info_.r_dyn_event_map_.size());
  for (PredictorLocalInfo::PairSet::iterator it = local_info_.pair_db_.begin();
       it != local_info_.pair_db_.end(); ++it) {
    bool same_addr = (*it).same_addr;
    thread_id_t thd_id = (*it).thd_id;
    PredictorLocalInfo::DynEvent curr_event, prev_event;
    curr_event.thd_id = thd_id;
    curr_event.type = (*it).curr_type;
    curr_event.inst = (*it).curr_inst;
    prev_event.thd_id = thd_id;
    prev_event.type = (*it).prev_type;
    prev_event.inst = (*it).prev_inst;

    PredictorLocalInfo::DynEventMap::iterator succs_it
        = local_info_.dyn_event_map_.find(prev_event);
    PredictorLocalInfo::DynEventMap::iterator preds_it
        = local_info_.r_dyn_event_map_.find(curr_event);

    if (succs_it == local_info_.dyn_event_map_.end() ||
        preds_it == local_info_.r_dyn_event_map_.end())
      continue;

    // whether async happens in thd_id
    bool curr_async = CheckAsync(thd_id);

    PredictorLocalInfo::DynEventRangeMap &succs = succs_it->second;
    PredictorLocalInfo::DynEventRangeMap &preds = preds_it->second;
    if (same_addr) {
      // for idiom-2, idiom-3
      for (PredictorLocalInfo::DynEventRangeMap::iterator sit = succs.begin();
           sit != succs.end(); ++sit) {
        PredictorLocalInfo::DynEvent se = sit->first;
        PredictorLocalInfo::DynRange sr = sit->second;
        bool idiom2_exists = false;
        for (PredictorLocalInfo::DynEventRangeMap::iterator pit = preds.begin();
             pit != preds.end(); ++pit) {
          PredictorLocalInfo::DynEvent pe = pit->first;
          PredictorLocalInfo::DynRange pr = pit->second;

          if (pe.thd_id == se.thd_id) {
            // check range
            if (sr.start <= pr.end) {
              iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_event.inst,
                                                        prev_event.type,
                                                        false);
              iRootEvent *e1 = iroot_db_->GetiRootEvent(se.inst,
                                                        se.type,
                                                        false);
              iRootEvent *e2 = iroot_db_->GetiRootEvent(pe.inst,
                                                        pe.type,
                                                        false);
              iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_event.inst,
                                                        curr_event.type,
                                                        false);
              iRoot *iroot = iroot_db_->GetiRoot(IDIOM_3,false, e0, e1, e2, e3);
              memo_->Predicted(iroot, false);
              if (curr_async ||
                  CheckAsync(se.thd_id, sr.end) ||
                  CheckAsync(pe.thd_id, pr.end)) {
                memo_->SetAsync(iroot, false);
              }
            }
          }

          if (!idiom2_exists &&
              pe.thd_id == se.thd_id &&
              pe.type == se.type &&
              pe.inst == se.inst) {
            // check range
            if (sr.start <= pr.end && pr.start <= sr.end) {
              idiom2_exists = true;
            }
          }
        } // end of for

        if (idiom2_exists) {
          iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_event.inst,
                                                    prev_event.type,
                                                    false);
          iRootEvent *e1 = iroot_db_->GetiRootEvent(se.inst,
                                                    se.type,
                                                    false);
          iRootEvent *e2 = iroot_db_->GetiRootEvent(curr_event.inst,
                                                    curr_event.type,
                                                    false);
          iRoot *iroot = iroot_db_->GetiRoot(IDIOM_2, false, e0, e1, e2);
          memo_->Predicted(iroot, false);
          if (curr_async || CheckAsync(se.thd_id, sr.end)) {
            memo_->SetAsync(iroot, false);
          }
        }
      }

    } else {
      // for idiom-4, idiom-5
      for (PredictorLocalInfo::DynEventRangeMap::iterator sit = succs.begin();
           sit != succs.end(); ++sit) {
        PredictorLocalInfo::DynEvent se = sit->first;
        PredictorLocalInfo::DynRange sr = sit->second;
        for (PredictorLocalInfo::DynEventRangeMap::iterator pit = preds.begin();
             pit != preds.end(); ++pit) {
          PredictorLocalInfo::DynEvent pe = pit->first;
          PredictorLocalInfo::DynRange pr = pit->second;

          if (pe.thd_id == se.thd_id) {
            // check range for idiom-4
            if (sr.start <= pr.end) {
              iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_event.inst,
                                                        prev_event.type,
                                                        false);
              iRootEvent *e1 = iroot_db_->GetiRootEvent(se.inst,
                                                        se.type,
                                                        false);
              iRootEvent *e2 = iroot_db_->GetiRootEvent(pe.inst,
                                                        pe.type,
                                                        false);
              iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_event.inst,
                                                        curr_event.type,
                                                        false);
              iRoot *iroot = iroot_db_->GetiRoot(IDIOM_4,false, e0, e1, e2, e3);
              memo_->Predicted(iroot, false);
              if (curr_async ||
                  CheckAsync(se.thd_id, sr.end) ||
                  CheckAsync(pe.thd_id, pr.end)) {
                memo_->SetAsync(iroot, false);
              }
            }

            // check range for idiom-5
            if (pr.start <= sr.end) {
              // check <pe, se> is a valid pair in pair db
              PredictorLocalInfo::PairType remote_pair;
              remote_pair.curr_inst = se.inst;
              remote_pair.curr_type = se.type;
              remote_pair.prev_inst = pe.inst;
              remote_pair.prev_type = pe.type;
              remote_pair.same_addr = false;
              remote_pair.thd_id = se.thd_id;
              if (local_info_.pair_db_.find(remote_pair) !=
                  local_info_.pair_db_.end()) {
                iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_event.inst,
                                                          prev_event.type,
                                                          false);
                iRootEvent *e1 = iroot_db_->GetiRootEvent(se.inst,
                                                          se.type,
                                                          false);
                iRootEvent *e2 = iroot_db_->GetiRootEvent(pe.inst,
                                                          pe.type,
                                                          false);
                iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_event.inst,
                                                          curr_event.type,
                                                          false);
                iRoot *iroot = iroot_db_->GetiRoot(IDIOM_5, false, e0,e1,e2,e3);
                memo_->Predicted(iroot, false);
                if (curr_async ||
                    CheckAsync(se.thd_id, sr.end) ||
                    CheckAsync(pe.thd_id, pr.end)) {
                  memo_->SetAsync(iroot, false);
                }
              }
            }
          }
        }
      }
    }
  }

  if (predict_deadlock_) {
    for (PredictorDeadlockInfo::PairSet::iterator outer_it =
            deadlock_info_.pair_db_.begin();
            outer_it != deadlock_info_.pair_db_.end(); ++outer_it) {
      iRootEventType outer_curr_type = (*outer_it).curr_type;
      Inst *outer_curr_inst = (*outer_it).curr_inst;
      address_t outer_curr_addr = (*outer_it).curr_addr;
      iRootEventType outer_prev_type = (*outer_it).prev_type;
      Inst *outer_prev_inst = (*outer_it).prev_inst;
      address_t outer_prev_addr = (*outer_it).prev_addr;
      thread_id_t outer_thd_id = (*outer_it).thd_id;

      for (PredictorDeadlockInfo::PairSet::iterator inner_it =
              deadlock_info_.pair_db_.begin();
              inner_it != deadlock_info_.pair_db_.end(); ++inner_it) {
        iRootEventType inner_curr_type = (*inner_it).curr_type;
        Inst *inner_curr_inst = (*inner_it).curr_inst;
        address_t inner_curr_addr = (*inner_it).curr_addr;
        iRootEventType inner_prev_type = (*inner_it).prev_type;
        Inst *inner_prev_inst = (*inner_it).prev_inst;
        address_t inner_prev_addr = (*inner_it).prev_addr;
        thread_id_t inner_thd_id = (*inner_it).thd_id;

        if (outer_thd_id != inner_thd_id &&
            outer_curr_addr == inner_prev_addr &&
            outer_prev_addr == inner_curr_addr) {
          iRootEvent *e0 = iroot_db_->GetiRootEvent(outer_prev_inst,
                                                    outer_prev_type,
                                                    false);
          iRootEvent *e1 = iroot_db_->GetiRootEvent(inner_curr_inst,
                                                    inner_curr_type,
                                                    false);
          iRootEvent *e2 = iroot_db_->GetiRootEvent(inner_prev_inst,
                                                    inner_prev_type,
                                                    false);
          iRootEvent *e3 = iroot_db_->GetiRootEvent(outer_curr_inst,
                                                    outer_curr_type,
                                                    false);
          iRoot *iroot = iroot_db_->GetiRoot(IDIOM_5, false, e0, e1, e2, e3);
          memo_->Predicted(iroot, false);
        }
      }
    }
  }
}

} // namespace idiom

