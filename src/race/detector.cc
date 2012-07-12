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

// File: race/detector.cc - Implementation of the abstract data race
// detector.

#include "race/detector.h"

#include "core/logging.h"

namespace race {

Detector::Detector()
    : internal_lock_(NULL),
      race_db_(NULL),
      unit_size_(4),
      filter_(NULL) {
  // do nothing
}

Detector::~Detector() {
  delete internal_lock_;
  delete filter_;
}

void Detector::Register() {
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
}

void Detector::Setup(Mutex *lock, RaceDB *race_db) {
  internal_lock_ = lock;
  race_db_ = race_db;
  unit_size_ = knob_->ValueInt("unit_size");
  filter_ = new RegionFilter(internal_lock_->Clone());

  // set analyzer descriptor
  desc_.SetHookBeforeMem();
  desc_.SetHookPthreadFunc();
  desc_.SetHookMallocFunc();
  desc_.SetHookAtomicInst();
}

void Detector::ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                         address_t data_start, size_t data_size,
                         address_t bss_start, size_t bss_size) {
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

void Detector::ImageUnload(Image *image,address_t low_addr, address_t high_addr,
                           address_t data_start, size_t data_size,
                           address_t bss_start, size_t bss_size) {
  DEBUG_ASSERT(low_addr);
  if (data_start)
    FreeAddrRegion(data_start);
  if (bss_start)
    FreeAddrRegion(bss_start);
}

void Detector::ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id) {
  // create thread local vector clock and lockset
  VectorClock *curr_vc = new VectorClock;

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
  // init atomic map
  atomic_map_[curr_thd_id] = false;
}

void Detector::BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  if (atomic_map_[curr_thd_id])
    return;
  // normalize accesses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta *meta = GetMeta(iaddr);
    DEBUG_ASSERT(meta);
    ProcessRead(curr_thd_id, meta, inst);
  } // end of for each iaddr
}

void Detector::BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  if (atomic_map_[curr_thd_id])
    return;
  // normalize accesses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta *meta = GetMeta(iaddr);
    DEBUG_ASSERT(meta);
    ProcessWrite(curr_thd_id, meta, inst);
  } // end of for each iaddr
}

void Detector::BeforeAtomicInst(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                std::string type, address_t addr) {
  ScopedLock locker(internal_lock_);
  // set atomic map
  atomic_map_[curr_thd_id] = true;
  // special care for lock prefix instructions
  //address_t aligned_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  //MutexMeta *guard_meta = GetMutexMeta(aligned_addr);
  // guard lock/unlock
  //ProcessLock(curr_thd_id, guard_meta);
  //ProcessUnlock(curr_thd_id, guard_meta);
}

void Detector::AfterAtomicInst(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               std::string type, address_t addr) {
  ScopedLock locker(internal_lock_);
  // clear atomic map
  atomic_map_[curr_thd_id] = false;
}

void Detector::AfterPthreadJoin(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                thread_id_t child_thd_id) {
  ScopedLock locker(internal_lock_);
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  VectorClock *child_vc = curr_vc_map_[child_thd_id];
  curr_vc->Join(child_vc);
}

void Detector::AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  MutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessLock(curr_thd_id, meta);
}

void Detector::BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  MutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessUnlock(curr_thd_id, meta);
}

void Detector::BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  CondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessNotify(curr_thd_id, meta);
}

void Detector::BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  CondMeta *meta = GetCondMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessNotify(curr_thd_id, meta);
}

void Detector::BeforePthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr,
                                     address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  MutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessUnlock(curr_thd_id, mutex_meta);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPreWait(curr_thd_id, cond_meta);
}

void Detector::AfterPthreadCondWait(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk, Inst *inst,
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
  MutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessLock(curr_thd_id, mutex_meta);
}

void Detector::BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(cond_addr, unit_size_) == cond_addr);
  // unlock
  MutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessUnlock(curr_thd_id, mutex_meta);
  // wait
  CondMeta *cond_meta = GetCondMeta(cond_addr);
  DEBUG_ASSERT(cond_meta);
  ProcessPreWait(curr_thd_id, cond_meta);
}

void Detector::AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
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
  MutexMeta *mutex_meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(mutex_meta);
  ProcessLock(curr_thd_id, mutex_meta);
}

void Detector::BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  BarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessPreBarrier(curr_thd_id, meta);
}

void Detector::AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  BarrierMeta *meta = GetBarrierMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessPostBarrier(curr_thd_id, meta);
}

void Detector::AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

void Detector::AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t nmemb, size_t size,
                           address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void Detector::BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size) {
  FreeAddrRegion(ori_addr);
}

void Detector::AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t ori_addr, size_t size,
                            address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void Detector::BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t addr) {
  FreeAddrRegion(addr);
}

void Detector::AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

// helper functions
void Detector::AllocAddrRegion(address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(addr && size);
  filter_->AddRegion(addr, size, false);
}

void Detector::FreeAddrRegion(address_t addr) {
  ScopedLock locker(internal_lock_);
  if (!addr) return;
  size_t size = filter_->RemoveRegion(addr, false);
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta::Table::iterator it = meta_table_.find(iaddr);
    if (it != meta_table_.end()) {
      ProcessFree(it->second);
      meta_table_.erase(it);
    }
  }
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    MutexMeta::Table::iterator it = mutex_meta_table_.find(iaddr);
    if (it != mutex_meta_table_.end()) {
      ProcessFree(it->second);
      mutex_meta_table_.erase(it);
    }
  }
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    CondMeta::Table::iterator it = cond_meta_table_.find(iaddr);
    if (it != cond_meta_table_.end()) {
      ProcessFree(it->second);
      cond_meta_table_.erase(it);
    }
  }
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    BarrierMeta::Table::iterator it = barrier_meta_table_.find(iaddr);
    if (it != barrier_meta_table_.end()) {
      ProcessFree(it->second);
      barrier_meta_table_.erase(it);
    }
  }
}

Detector::MutexMeta *Detector::GetMutexMeta(address_t iaddr) {
  MutexMeta::Table::iterator it = mutex_meta_table_.find(iaddr);
  if (it == mutex_meta_table_.end()) {
    MutexMeta *meta = new MutexMeta;
    mutex_meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

Detector::CondMeta *Detector::GetCondMeta(address_t iaddr) {
  CondMeta::Table::iterator it = cond_meta_table_.find(iaddr);
  if (it == cond_meta_table_.end()) {
    CondMeta *meta = new CondMeta;
    cond_meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

Detector::BarrierMeta *Detector::GetBarrierMeta(address_t iaddr) {
  BarrierMeta::Table::iterator it = barrier_meta_table_.find(iaddr);
  if (it == barrier_meta_table_.end()) {
    BarrierMeta *meta = new BarrierMeta;
    barrier_meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void Detector::ReportRace(Meta *meta, thread_id_t t0, Inst *i0,
                          RaceEventType p0, thread_id_t t1, Inst *i1,
                          RaceEventType p1) {
  race_db_->CreateRace(meta->addr, t0, i0, p0, t1, i1, p1, false);
}

// main processing functions
void Detector::ProcessLock(thread_id_t curr_thd_id, MutexMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  // join the vector clock
  curr_vc->Join(&meta->vc);
}

void Detector::ProcessUnlock(thread_id_t curr_thd_id, MutexMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  meta->vc = *curr_vc;
  // increment the vector clock
  curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessNotify(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  // iterate the wait table, join vector clock
  for (CondMeta::VectorClockMap::iterator it = meta->wait_table.begin();
       it != meta->wait_table.end(); ++it) {
    curr_vc->Join(&it->second);
  }
  // update signal table
  for (CondMeta::VectorClockMap::iterator it = meta->wait_table.begin();
       it != meta->wait_table.end(); ++it) {
    meta->signal_table[it->first] = *curr_vc;
  }
  curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessPreWait(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  DEBUG_ASSERT(curr_vc);
  meta->wait_table[curr_thd_id] = *curr_vc;
  curr_vc->Increment(curr_thd_id);
}

void Detector::ProcessPostWait(thread_id_t curr_thd_id, CondMeta *meta) {
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  // it is possible that wait_post does not depend on a signal
  // or broadcast. this is because there exist wait_timeout
  // sync functions
  CondMeta::VectorClockMap::iterator wit = meta->wait_table.find(curr_thd_id);
  DEBUG_ASSERT(wit != meta->wait_table.end());
  meta->wait_table.erase(wit);
  CondMeta::VectorClockMap::iterator sit = meta->signal_table.find(curr_thd_id);
  if (sit != meta->signal_table.end()) {
    // join vector clock
    curr_vc->Join(&sit->second);
    meta->signal_table.erase(sit);
  }
}

void Detector::ProcessPreBarrier(thread_id_t curr_thd_id, BarrierMeta *meta) {
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

void Detector::ProcessPostBarrier(thread_id_t curr_thd_id, BarrierMeta *meta) {
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

void Detector::ProcessFree(MutexMeta *meta) {
  delete meta;
}

void Detector::ProcessFree(CondMeta *meta) {
  delete meta;
}

void Detector::ProcessFree(BarrierMeta *meta) {
  delete meta;
}

} // namespace race


