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

// File: idiom/observer_new.cc - Implementation of the bew iRoot
// observer analyzer.

#include "idiom/observer_new.h"

#include "core/logging.h"
#include "core/stat.h"

namespace idiom {

// each thread has a buffer of about 10M bytes
#define ENTRY_QUEUE_LIMIT (1024 * 10)

ObserverNew::ObserverNew()
    : internal_lock_(NULL),
      sinfo_(NULL),
      iroot_db_(NULL),
      memo_(NULL),
      sinst_db_(NULL),
      shadow_(false),
      sync_only_(false),
      complex_idioms_(false),
      single_var_idioms_(false),
      unit_size_(4),
      vw_(1000),
      filter_(NULL),
      curr_acc_uid_(0) {
  // empty
}

ObserverNew::~ObserverNew() {
  // empty
}

void ObserverNew::Register() {
  knob_->RegisterBool("enable_observer_new", "whether enable iroot observer (NEW)", "0");
  knob_->RegisterBool("shadow_observer", "whether the observer is shadow", "0");
  knob_->RegisterBool("sync_only", "whether only monitor synchronization accesses", "0");
  knob_->RegisterBool("complex_idioms", "whether target complex idioms", "0");
  knob_->RegisterBool("single_var_idioms", "whether only consider single variable idioms", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("vw", "the vulnerability window (# dynamic inst)", "1000");
}

bool ObserverNew::Enabled() {
  return knob_->ValueBool("enable_observer_new");
}

void ObserverNew::Setup(Mutex *lock,
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
  shadow_ = knob_->ValueBool("shadow_observer");
  sync_only_ = knob_->ValueBool("sync_only");
  complex_idioms_ = knob_->ValueBool("complex_idioms");
  single_var_idioms_ = knob_->ValueBool("single_var_idioms");
  unit_size_ = knob_->ValueInt("unit_size");
  vw_ = knob_->ValueInt("vw");

  // init global analysis state
  filter_ = new RegionFilter(internal_lock_->Clone());
  InitLpValidTable();

  // setup analysis descriptor
  if (!sync_only_)
    desc_.SetHookBeforeMem();
  desc_.SetHookPthreadFunc();
  desc_.SetHookMallocFunc();
  desc_.SetTrackInstCount();
}

void ObserverNew::ImageLoad(Image *image,
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

void ObserverNew::ImageUnload(Image *image,
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

void ObserverNew::ThreadStart(thread_id_t curr_thd_id,
                              thread_id_t parent_thd_id) {
  // empty
}

void ObserverNew::ThreadExit(thread_id_t curr_thd_id,
                             timestamp_t curr_thd_clk) {
  // empty
}

void ObserverNew::BeforeMemRead(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk,
                                Inst *inst,
                                address_t addr,
                                size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize addresses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue; // acecss to sync variable, ignore
    ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                      IROOT_EVENT_MEM_READ, inst, meta);
  }
}

void ObserverNew::BeforeMemWrite(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk,
                                 Inst *inst,
                                 address_t addr,
                                 size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize addresses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue; // acecss to sync variable, ignore
    ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                      IROOT_EVENT_MEM_WRITE, inst, meta);
  }
}

void ObserverNew::AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk,
                                        Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  Meta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, meta);
}

void ObserverNew::BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                           timestamp_t curr_thd_clk,
                                           Inst *inst,
                                           address_t addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  Meta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, meta);
}

void ObserverNew::BeforePthreadCondWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk,
                                        Inst *inst,
                                        address_t cond_addr,
                                        address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  Meta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, meta);
}

void ObserverNew::AfterPthreadCondWait(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk,
                                       Inst *inst,
                                       address_t cond_addr,
                                       address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  Meta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, meta);
}

void ObserverNew::BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                             timestamp_t curr_thd_clk,
                                             Inst *inst,
                                             address_t cond_addr,
                                             address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  Meta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_UNLOCK, inst, meta);
}

void ObserverNew::AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                            timestamp_t curr_thd_clk,
                                            Inst *inst,
                                            address_t cond_addr,
                                            address_t mutex_addr) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  Meta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  ProcessiRootEvent(curr_thd_id, curr_thd_clk,
                    IROOT_EVENT_MUTEX_LOCK, inst, meta);
}

void ObserverNew::AfterMalloc(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk,
                              Inst *inst,
                              size_t size,
                              address_t addr) {
  AllocAddrRegion(addr, size);
}

void ObserverNew::AfterCalloc(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk,
                              Inst *inst,
                              size_t nmemb,
                              size_t size,
                              address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void ObserverNew::BeforeRealloc(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk,
                                Inst *inst,
                                address_t ori_addr,
                                size_t size) {
  FreeAddrRegion(ori_addr);
}

void ObserverNew::AfterRealloc(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst,
                               address_t ori_addr,
                               size_t size,
                               address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void ObserverNew::BeforeFree(thread_id_t curr_thd_id,
                             timestamp_t curr_thd_clk,
                             Inst *inst,
                             address_t addr) {
  FreeAddrRegion(addr);
}

void ObserverNew::AfterValloc(thread_id_t curr_thd_id,
                              timestamp_t curr_thd_clk,
                              Inst *inst,
                              size_t size,
                              address_t addr) {
  AllocAddrRegion(addr, size);
}

void ObserverNew::RecentInfoGC(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               size_t threshold) {
  RecentInfo &ri = ri_table_[curr_thd_id];
  if (ri.entry_queue.size() < threshold)
    return;
  // perform garbage collection
  RecentInfo::Entry::Vec new_entry_queue;
  for (RecentInfo::Entry::Vec::reverse_iterator it = ri.entry_queue.rbegin();
       it != ri.entry_queue.rend(); ++it) {
    RecentInfo::Entry &ri_entry = *it;
    if (TIME_DISTANCE(ri_entry.acc.thd_clk, curr_thd_clk) >= vw_)
      break;
    new_entry_queue.push_back(ri_entry);
  }
  // clear the original entry queue
  ri.entry_queue.clear();
  // setup the new entry queue
  for (RecentInfo::Entry::Vec::reverse_iterator it = new_entry_queue.rbegin();
       it != new_entry_queue.rend(); ++it) {
    ri.entry_queue.push_back(*it);
  }
  DEBUG_STAT_INC("ob_recent_info_gc", 1);
}

bool ObserverNew::CheckLocalPair(iRootEventType prev, iRootEventType curr) {
  // return true if <prev,curr> is a valid local pair
  return lp_valid_table_[prev][curr];
}

void ObserverNew::InitLpValidTable() {
  for (int i = 0; i < IROOT_EVENT_TYPE_ARRAYSIZE; i++) {
    for (int j = 0; j < IROOT_EVENT_TYPE_ARRAYSIZE; j++) {
      lp_valid_table_[i][j] = false;
    }
  }
  lp_valid_table_[IROOT_EVENT_MEM_READ][IROOT_EVENT_MEM_READ] = true;
  lp_valid_table_[IROOT_EVENT_MEM_READ][IROOT_EVENT_MEM_WRITE] = true;
  lp_valid_table_[IROOT_EVENT_MEM_WRITE][IROOT_EVENT_MEM_READ] = true;
  lp_valid_table_[IROOT_EVENT_MEM_WRITE][IROOT_EVENT_MEM_WRITE] = true;
  lp_valid_table_[IROOT_EVENT_MUTEX_UNLOCK][IROOT_EVENT_MUTEX_LOCK] = true;
}


void ObserverNew::AllocAddrRegion(address_t addr, size_t size) {
  DEBUG_ASSERT(addr && size);
  ScopedLock locker(internal_lock_);
  filter_->AddRegion(addr, size, false);
}

void ObserverNew::FreeAddrRegion(address_t addr) {
  if (!addr) return;
  ScopedLock locker(internal_lock_);
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
}

ObserverNew::Meta *ObserverNew::GetMemMeta(address_t iaddr) {
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

ObserverNew::Meta *ObserverNew::GetMutexMeta(address_t iaddr) {
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

void ObserverNew::UpdateiRoot(Acc *curr_acc, Acc::Vec *preds) {
  for (Acc::Vec::iterator it = preds->begin(); it != preds->end(); ++it) {
    iRootEvent *pred = iroot_db_->GetiRootEvent((*it).inst,
                                                (*it).type,
                                                false);
    iRootEvent *curr = iroot_db_->GetiRootEvent(curr_acc->inst,
                                                curr_acc->type,
                                                false);
    iRoot *iroot = iroot_db_->GetiRoot(IDIOM_1, false, pred, curr);
    memo_->Observed(iroot, shadow_, false);
    DEBUG_STAT_INC("ob_dynamic_deps", 1);
  }
}

void ObserverNew::UpdateComplexiRoot(Acc *curr_acc,
                                     Meta *curr_meta,
                                     Acc::Vec *preds,
                                     RecentInfo::Entry *prev_ri_entry) {
  Acc::Vec *succs = &prev_ri_entry->succs;
  // no need to continue if either preds or succs is empty
  if (preds->empty() || succs->empty())
    return;
  Acc &prev_acc = prev_ri_entry->acc;
  Meta *prev_meta = prev_ri_entry->meta;
  // iterate over two vectors (using index)
  size_t succs_size = succs->size();
  size_t preds_size = preds->size();
  for (size_t succ_idx = 0; succ_idx < succs_size; succ_idx++) {
    Acc &succ = (*succs)[succ_idx];
    Acc::Vec &succ_prevs = prev_ri_entry->succ_prevs[succ_idx];
    // whether exists a access that is both pred and succ
    bool same_acc_exist = false;
    for (size_t pred_idx = 0; pred_idx < preds_size; pred_idx++) {
      Acc &pred = (*preds)[pred_idx];
      // make sure that succ and pred are in the same thread
      if (succ.thd_id == pred.thd_id) {
        DEBUG_ASSERT(succ.thd_id != curr_acc->thd_id);
        if (succ.thd_clk < pred.thd_clk) {
          // for idiom3/idiom4
          iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_acc.inst,
                                                    prev_acc.type,
                                                    false);
          iRootEvent *e1 = iroot_db_->GetiRootEvent(succ.inst,
                                                    succ.type,
                                                    false);
          iRootEvent *e2 = iroot_db_->GetiRootEvent(pred.inst,
                                                    pred.type,
                                                    false);
          iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_acc->inst,
                                                    curr_acc->type,
                                                    false);
          iRoot *iroot = NULL;
          if (prev_meta == curr_meta) {
            iroot = iroot_db_->GetiRoot(IDIOM_3, false, e0, e1, e2, e3);
          } else {
            iroot = iroot_db_->GetiRoot(IDIOM_4, false, e0, e1, e2, e3);
          }
          memo_->Observed(iroot, shadow_, false);
        } else if (succ.thd_clk > pred.thd_clk) {
          // for idiom5
          if (TIME_DISTANCE(pred.thd_clk, succ.thd_clk) < vw_) {
            // make sure they involve different meta
            if (prev_meta != curr_meta) {
              // need to check whether there exists access between succ
              // and pred that accesses the same location as succ or pred
              // check whether pred is in succ_prevs
              for (Acc::Vec::iterator it = succ_prevs.begin();
                   it != succ_prevs.end(); ++it) {
                if ((*it).uid == pred.uid) {
                  iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_acc.inst,
                                                            prev_acc.type,
                                                            false);
                  iRootEvent *e1 = iroot_db_->GetiRootEvent(succ.inst,
                                                            succ.type,
                                                            false);
                  iRootEvent *e2 = iroot_db_->GetiRootEvent(pred.inst,
                                                            pred.type,
                                                            false);
                  iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_acc->inst,
                                                            curr_acc->type,
                                                            false);
                  iRoot *iroot = iroot_db_->GetiRoot(IDIOM_5, false,
                                                     e0, e1, e2, e3);
                  iRoot *irootx = iroot_db_->GetiRoot(IDIOM_5, false,
                                                      e2, e3, e0, e1);
                  memo_->Observed(iroot, shadow_, false);
                  memo_->Observed(irootx, shadow_, false);
                  break;
                }
              }
            }
          }
        } // end of else succ.thd_clk < pred.thd_clk
        // for idiom2
        if (succ.uid == pred.uid) {
          same_acc_exist = true;
        }
      } // end of if succ and pred are in the same thread
    } // end of for each pred

    // for idiom2
    if (same_acc_exist) {
      iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_acc.inst,
                                                prev_acc.type,
                                                false);
      iRootEvent *e1 = iroot_db_->GetiRootEvent(succ.inst,
                                                succ.type,
                                                false);
      iRootEvent *e2 = iroot_db_->GetiRootEvent(curr_acc->inst,
                                                curr_acc->type,
                                                false);
      iRoot *iroot = iroot_db_->GetiRoot(IDIOM_2, false, e0, e1, e2);
      memo_->Observed(iroot, shadow_, false);
    }
  } // end of for each succ
  DEBUG_STAT_INC("ob_upd_comp_iroot", 1);
}

void ObserverNew::ProcessiRootEvent(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk,
                                    iRootEventType type,
                                    Inst *inst,
                                    Meta *meta) {
  // create the current access
  Acc curr_acc;
  curr_acc.uid = GetNextAccUid();
  curr_acc.thd_id = curr_thd_id;
  curr_acc.thd_clk = curr_thd_clk;
  curr_acc.type = type;
  curr_acc.inst = inst;

  // observe idiom1 iroots for the current access
  Acc::Vec preds; // the observed predecessors of the current access
  if (IsRead(type)) {
    // the current access is a read
    // detect read-after-write (RAW) dependency
    if (meta->last_writer.valid &&
        meta->last_writer.acc.thd_id != curr_thd_id) {
      // check whether a local precedent read exists
      Meta::LastAcc::Table::iterator it
          = meta->last_acc_table.find(curr_thd_id);
      if (it == meta->last_acc_table.end() || !it->second.valid) {
        // a RAW dependency is discovered
        preds.push_back(meta->last_writer.acc);
      }
    }
  } else {
    // the current access is a write
    // detect write-after-read (WAR) dependency
    bool war_exist = false;
    for (Meta::LastAcc::Table::iterator it = meta->last_acc_table.begin();
         it != meta->last_acc_table.end(); ++it) {
      if (it->second.valid) {
        if (it->first != curr_thd_id) {
          // a WAR dependency is discovered
          preds.push_back(it->second.acc);
        }
        war_exist = true;
      }
    }
    // detect write-after-write (WAW) dependency
    if (!war_exist) {
      if (meta->last_writer.valid &&
          meta->last_writer.acc.thd_id != curr_thd_id) {
        // a WAW dependency is discovered
        preds.push_back(meta->last_writer.acc);
      }
    }
  }
  // update iroots
  UpdateiRoot(&curr_acc, &preds);

  // handle complex iroots
  if (complex_idioms_) {
    ProcessRecentInfo(&curr_acc, meta, &preds);
  }

  // update meta data
  if (IsRead(type)) {
    // the current access is a read
    Meta::LastAcc &last = meta->last_acc_table[curr_thd_id];
    last.valid = true;
    last.acc = curr_acc;
  } else {
    // the current access is a write
    for (Meta::LastAcc::Table::iterator it = meta->last_acc_table.begin();
         it != meta->last_acc_table.end(); ++it) {
      it->second.valid = false;
    }
    Meta::LastAcc &last = meta->last_acc_table[curr_thd_id];
    last.valid = false;
    last.acc = curr_acc;
    meta->last_writer.valid = true;
    meta->last_writer.acc = curr_acc;
  }
}

void ObserverNew::ProcessFree(Meta *meta) {
  meta->last_acc_table.clear();
}

void ObserverNew::ProcessRecentInfo(Acc *curr_acc,
                                    Meta *curr_meta,
                                    Acc::Vec *preds) {
  DEBUG_ASSERT(complex_idioms_);
  // get the current recent info
  RecentInfo &curr_ri = ri_table_[curr_acc->thd_id];
  // only do a search when preds is not empty
  if (!preds->empty()) {
    // recorded local prevs
    Acc::Vec prevs;
    // search the recent accesses of the current thread
    if (single_var_idioms_) {
      for (RecentInfo::Entry::Vec::reverse_iterator it =
              curr_ri.entry_queue.rbegin();
           it != curr_ri.entry_queue.rend(); ++it) {
        RecentInfo::Entry &prev_ri_entry = *it;
        // stop the search if go beyond the window
        if (TIME_DISTANCE(prev_ri_entry.acc.thd_clk, curr_acc->thd_clk) >= vw_)
          break;
        // only check the latest entry with a matching meta
        if (prev_ri_entry.meta == curr_meta) {
          // make sure the potential local pair is valid
          if (CheckLocalPair(prev_ri_entry.acc.type, curr_acc->type)) {
            UpdateComplexiRoot(curr_acc, curr_meta, preds, &prev_ri_entry);
          }
          // store into local prevs
          prevs.push_back(prev_ri_entry.acc);
          // no need to continue
          break;
        }
      }
    } else {
      Meta::HashSet visited_meta;
      for (RecentInfo::Entry::Vec::reverse_iterator it =
              curr_ri.entry_queue.rbegin();
           it != curr_ri.entry_queue.rend(); ++it) {
        RecentInfo::Entry &prev_ri_entry = *it;
        // stop the search if go beyond the window
        if (TIME_DISTANCE(prev_ri_entry.acc.thd_clk, curr_acc->thd_clk) >= vw_)
          break;
        // skip if a later access to this meta is found
        if (visited_meta.find(prev_ri_entry.meta) != visited_meta.end())
          continue;
        // make sure the potential local pair is valid
        if (CheckLocalPair(prev_ri_entry.acc.type, curr_acc->type)) {
          UpdateComplexiRoot(curr_acc, curr_meta, preds, &prev_ri_entry);
        }
        // store into local prevs
        prevs.push_back(prev_ri_entry.acc);
        // mark this meta as visited
        visited_meta.insert(prev_ri_entry.meta);
        // no need to continue if hit the current meta
        if (prev_ri_entry.meta == curr_meta)
          break;
      } // end of for each recent access
    }

    // add curr_acc to the succs of each pred access
    for (Acc::Vec::iterator pred_it = preds->begin();
         pred_it != preds->end(); ++pred_it) {
      Acc &pred_acc = *pred_it;
      // search the recent info of thread pred_acc->thd_id
      // but, no need to search beyond the time window
      RecentInfo &rmt_ri = ri_table_[pred_acc.thd_id];
      for (RecentInfo::Entry::Vec::reverse_iterator rmt_it =
              rmt_ri.entry_queue.rbegin();
           rmt_it != rmt_ri.entry_queue.rend(); ++rmt_it) {
        RecentInfo::Entry &rmt_ri_entry = *rmt_it;
        // no need to search beyond the time window
        if (TIME_DISTANCE(rmt_ri_entry.acc.thd_clk, rmt_ri.curr_thd_clk) >= vw_)
          break;
        // check whether acc uid matches
        if (pred_acc.uid == rmt_ri_entry.acc.uid) {
          rmt_ri_entry.succs.push_back(*curr_acc);
          rmt_ri_entry.succ_prevs.push_back(prevs);
          break; // no need to continue
        }
      } // end of for each access in rmt_ri
    } // end of for each access in preds
  } // end of if preds is not empty

  // update the current recent info
  RecentInfo::Entry new_entry;
  new_entry.meta = curr_meta;
  new_entry.acc = *curr_acc;
  curr_ri.entry_queue.push_back(new_entry);
  curr_ri.curr_thd_clk = curr_acc->thd_clk;
  RecentInfoGC(curr_acc->thd_id, curr_acc->thd_clk, ENTRY_QUEUE_LIMIT);
}

} // namespace idiom

