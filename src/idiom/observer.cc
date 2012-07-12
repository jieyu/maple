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

// File: idiom/observer.cc - Implementation of the iRoot observer analyzer.

#include "idiom/observer.h"

#include "core/logging.h"

namespace idiom {

Observer::Observer()
    : internal_lock_(NULL),
      sinfo_(NULL),
      iroot_db_(NULL),
      memo_(NULL),
      sinst_db_(NULL),
      shadow_(false),
      sync_only_(false),
      unit_size_(4),
      complex_idioms_(false),
      vw_(1000),
      filter_(NULL) {
  // empty
}

Observer::~Observer() {
  delete internal_lock_;
  delete filter_;
}

void Observer::Register() {
  knob_->RegisterBool("enable_observer", "whether enable the iroot observer", "0");
  knob_->RegisterBool("shadow_observer", "whether the observer is shadow", "0");
  knob_->RegisterBool("sync_only", "whether only monitor synchronization accesse", "0");
  knob_->RegisterBool("complex_idioms", "whether target complex idioms", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
  knob_->RegisterInt("vw", "the vulnerability window (# dynamic inst)", "1000");
}

bool Observer::Enabled() {
  return knob_->ValueBool("enable_observer");
}

void Observer::Setup(Mutex *lock, StaticInfo *sinfo, iRootDB *iroot_db,
                     Memo *memo, sinst::SharedInstDB *sinst_db) {
  internal_lock_ = lock;
  sinfo_ = sinfo;
  iroot_db_ = iroot_db;
  memo_ = memo;
  sinst_db_ = sinst_db;

  shadow_ = knob_->ValueBool("shadow_observer");
  sync_only_ = knob_->ValueBool("sync_only");
  unit_size_ = knob_->ValueInt("unit_size");
  complex_idioms_ = knob_->ValueBool("complex_idioms");
  vw_ = knob_->ValueInt("vw");
  filter_ = new RegionFilter(internal_lock_->Clone());

  if (!sync_only_)
    desc_.SetHookBeforeMem();
  desc_.SetHookPthreadFunc();
  desc_.SetHookMallocFunc();
  desc_.SetTrackInstCount();
}

void Observer::ImageLoad(Image *image, address_t low_addr,
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

void Observer::ImageUnload(Image *image, address_t low_addr,
                           address_t high_addr, address_t data_start,
                           size_t data_size, address_t bss_start,
                           size_t bss_size) {
  DEBUG_ASSERT(low_addr);
  if (data_start)
    FreeAddrRegion(data_start);
  if (bss_start)
    FreeAddrRegion(bss_start);
}

void Observer::ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id) {
  ScopedLock locker(internal_lock_);

  local_info_map_[curr_thd_id].Clear();
}

void Observer::ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk) {
  ScopedLock locker(internal_lock_);

  local_info_map_[curr_thd_id].Clear();
}

void Observer::BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);

  if (FilterAccess(addr))
    return;

  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    ObserverMemMeta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue; // acecss to sync variable, ignore
    UpdateForRead(curr_thd_id, curr_thd_clk, inst, iaddr, meta);
  }
}

void Observer::BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);

  if (FilterAccess(addr))
    return;

  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    ObserverMemMeta *meta = GetMemMeta(iaddr);
    if (!meta)
      continue;
    UpdateForWrite(curr_thd_id, curr_thd_clk, inst, iaddr, meta);
  }
}

void Observer::AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  ObserverMutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, addr, meta);
}

void Observer::BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(addr, unit_size_) == addr);
  ObserverMutexMeta *meta = GetMutexMeta(addr);
  DEBUG_ASSERT(meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, addr, meta);
}

void Observer::BeforePthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr,
                                     address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  ObserverMutexMeta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, mutex_addr, meta);
}

void Observer::AfterPthreadCondWait(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk,
                                    Inst *inst, address_t cond_addr,
                                    address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  ObserverMutexMeta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, mutex_addr, meta);
}

void Observer::BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  ObserverMutexMeta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  UpdateForUnlock(curr_thd_id, curr_thd_clk, inst, mutex_addr, meta);
}

void Observer::AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t cond_addr,
                                         address_t mutex_addr) {
  ScopedLock locker(internal_lock_);

  DEBUG_ASSERT(UNIT_DOWN_ALIGN(mutex_addr, unit_size_) == mutex_addr);
  ObserverMutexMeta *meta = GetMutexMeta(mutex_addr);
  DEBUG_ASSERT(meta);
  UpdateForLock(curr_thd_id, curr_thd_clk, inst, mutex_addr, meta);
}

void Observer::AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

void Observer::AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t nmemb, size_t size,
                           address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void Observer::BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size) {
  FreeAddrRegion(ori_addr);
}

void Observer::AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t ori_addr, size_t size,
                            address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void Observer::BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t addr) {
  FreeAddrRegion(addr);
}

void Observer::AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

ObserverMemMeta *Observer::GetMemMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    ObserverMemMeta *meta = new ObserverMemMeta;
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    ObserverMemMeta *meta = dynamic_cast<ObserverMemMeta *>(it->second);
    return meta; // could be NULL
  }
}

ObserverMutexMeta *Observer::GetMutexMeta(address_t iaddr) {
  MetaMap::iterator it = meta_map_.find(iaddr);
  if (it == meta_map_.end()) {
    ObserverMutexMeta *meta = new ObserverMutexMeta;
    meta_map_[iaddr] = meta;
    return meta;
  } else {
    // check the type of the existing meta for this address
    ObserverMutexMeta *meta = dynamic_cast<ObserverMutexMeta *>(it->second);
    if (meta) {
      return meta;
    } else {
      delete it->second;
      meta = new ObserverMutexMeta;
      it->second = meta;
      return meta;
    }
  }
}

void Observer::AllocAddrRegion(address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(addr && size);
  filter_->AddRegion(addr, size, false);
}

void Observer::FreeAddrRegion(address_t addr) {
  ScopedLock locker(internal_lock_);

  if (!addr) return;
  size_t size = filter_->RemoveRegion(addr, false);
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    MetaMap::iterator it = meta_map_.find(iaddr);
    if (it != meta_map_.end()) {
      delete it->second;
      meta_map_.erase(it);
    }
  }
}

bool Observer::FilterAccess(address_t addr) {
  return filter_->Filter(addr, false);
}

void Observer::UpdateForRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr,
                             ObserverMemMeta *meta) {
  ObserverAccess curr_access(curr_thd_id, curr_thd_clk,
                             IROOT_EVENT_MEM_READ, inst);
  std::vector<ObserverAccess> preds;

  // detect idiom-1 iroot (RAW)
  ObserverMemMeta::ReaderMap::iterator reader_it
      = meta->last_readers_.find(curr_thd_id);
  // see whether the local precedent reader is valid or not
  if (reader_it == meta->last_readers_.end() || !reader_it->second.first) {
    // no local precedent reader, find recent remote writer
    if (meta->last_writer_.first &&
        meta->last_writer_.second.thd_id_ != curr_thd_id) {
      // RAW dependency
      preds.push_back(meta->last_writer_.second);
    }
  }

  // update iroots
  UpdateiRoots(&curr_access, &preds);

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);

  // update meta data
  if (reader_it == meta->last_readers_.end()) {
    meta->last_readers_[curr_thd_id]
        = ObserverMemMeta::Accessor(true, curr_access);
  } else {
    reader_it->second.first = true;
    reader_it->second.second = curr_access;
  }
}

void Observer::UpdateForWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr,
                              ObserverMemMeta *meta) {
  ObserverAccess curr_access(curr_thd_id, curr_thd_clk,
                             IROOT_EVENT_MEM_WRITE, inst);
  std::vector<ObserverAccess> preds;

  // detect idiom-1 iroot (WAR)
  bool war_exist = false;
  for (ObserverMemMeta::ReaderMap::iterator it = meta->last_readers_.begin();
       it != meta->last_readers_.end(); ++it) {
    if (it->second.first) {
      if (it->second.second.thd_id_ != curr_thd_id) {
        // WAR dependency
        preds.push_back(it->second.second);
      }
      war_exist = true;
    }
  }

  // detect idiom-1 iroot (WAW)
  if (!war_exist) {
    if (meta->last_writer_.first &&
        meta->last_writer_.second.thd_id_ != curr_thd_id) {
      // WAW dependency
      preds.push_back(meta->last_writer_.second);
    }
  }

  // update iroots
  UpdateiRoots(&curr_access, &preds);

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);

  // update meta data
  meta->last_writer_.first = true;
  meta->last_writer_.second = curr_access;
  for (ObserverMemMeta::ReaderMap::iterator it = meta->last_readers_.begin();
      it != meta->last_readers_.end(); ++it) {
    it->second.first = false;
  }
}

void Observer::UpdateForLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr,
                             ObserverMutexMeta *meta) {
  ObserverAccess curr_access(curr_thd_id, curr_thd_clk,
                             IROOT_EVENT_MUTEX_LOCK, inst);
  std::vector<ObserverAccess> preds;

  if (meta->last_unlocker_.first &&
      meta->last_unlocker_.second.thd_id_ != curr_thd_id) {
    // U->L dependency
    preds.push_back(meta->last_unlocker_.second);
  }

  // update iroots
  UpdateiRoots(&curr_access, &preds);

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);
}

void Observer::UpdateForUnlock(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst, address_t addr,
                               ObserverMutexMeta *meta) {
  ObserverAccess curr_access(curr_thd_id, curr_thd_clk,
                             IROOT_EVENT_MUTEX_UNLOCK, inst);
  std::vector<ObserverAccess> preds;

  // update local info
  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);

  meta->last_unlocker_.first = true;
  meta->last_unlocker_.second = curr_access;
}

void Observer::UpdateLocalInfo(ObserverAccess *curr_access, address_t addr,
                               std::vector<ObserverAccess> *preds) {
  if (!curr_access->IsSync()) {
    if (sinst_db_ && !sinst_db_->Shared(curr_access->inst_))
      return; // non shared instruction
  }

  thread_id_t curr_thd_id = curr_access->thd_id_;
  timestamp_t curr_time = curr_access->clk_;
  ObserverLocalInfo &curr_li = local_info_map_[curr_thd_id];

  // iterator recent accesses, calculate distance, discover complex iroots
  std::tr1::unordered_set<address_t> touched_addr_set;
  std::vector<ObserverAccess> local_prev_vec;
  for (ObserverLocalInfo::EntryMap::reverse_iterator mit =
          curr_li.entries_.rbegin(); mit != curr_li.entries_.rend(); ++mit) {
    timestamp_t time = mit->first;

    if (TIME_DISTANCE(time, curr_time) < vw_) {
      bool need_break = false;
      for (ObserverLocalInfo::EntryVec::reverse_iterator vit =
              mit->second.rbegin(); vit != mit->second.rend(); ++vit) {
        ObserverLocalInfo::EntryType &entry = *vit;
        if (touched_addr_set.find(entry.addr) != touched_addr_set.end())
          continue;
        // add to the touched address set
        touched_addr_set.insert(entry.addr);
        if (time != curr_time) {
          local_prev_vec.push_back(entry.access);
          UpdateComplexiRoots(curr_access, preds, &entry.access, &entry.succs,
                              (entry.addr == addr));
        }
        if (entry.addr == addr) {
          need_break = true;
          break;
        }
      } // end of for
      if (need_break)
        break;
    } else {
      break;
    }
  }

  // add curr_access to the succ of all the pred entries
  for (std::vector<ObserverAccess>::iterator it = preds->begin();
       it != preds->end(); ++it) {
    ObserverAccess &access = *it;
    timestamp_t time = access.clk_;
    ObserverLocalInfo &li = local_info_map_[access.thd_id_];
    ObserverLocalInfo::EntryMap::iterator mit = li.entries_.find(time);
    if (mit != li.entries_.end()) {
      for (ObserverLocalInfo::EntryVec::iterator vit = mit->second.begin();
           vit != mit->second.end(); ++vit) {
        if (addr == vit->addr &&
            access.type_ == vit->access.type_ &&
            access.inst_ == vit->access.inst_) {
          ObserverLocalInfo::SuccEntry succ_entry;
          succ_entry.succ = *curr_access;
          succ_entry.local_prev_vec = local_prev_vec;
          vit->succs.push_back(succ_entry);
        }
      }
    }
  }

  // remove stale entries if local info is so large
  while (!curr_li.entries_.empty()) {
    ObserverLocalInfo::EntryMap::iterator it = curr_li.entries_.begin();
    if (TIME_DISTANCE(it->first, curr_time) >= vw_) {
      curr_li.entries_.erase(it);
    } else {
      break;
    }
  }

  // add entry
  ObserverLocalInfo::EntryType new_entry;
  new_entry.addr = addr;
  new_entry.access = *curr_access;
  curr_li.entries_[curr_time].push_back(new_entry);
}

void Observer::UpdateiRoots(ObserverAccess *curr_access,
                            std::vector<ObserverAccess> *preds) {
  // for each in the preds
  for (std::vector<ObserverAccess>::iterator it = preds->begin();
       it != preds->end(); ++it) {
    iRootEvent *pred = iroot_db_->GetiRootEvent(it->inst_,
                                                it->type_,
                                                false);
    iRootEvent *curr = iroot_db_->GetiRootEvent(curr_access->inst_,
                                                curr_access->type_,
                                                false);
    iRoot *iroot = iroot_db_->GetiRoot(IDIOM_1, false, pred, curr);
    memo_->Observed(iroot, shadow_, false);
  }
}

void Observer::UpdateComplexiRoots(ObserverAccess *curr_access,
                                   std::vector<ObserverAccess> *preds,
                                   ObserverAccess *prev_access,
                                   ObserverLocalInfo::SuccVec *succs,
                                   bool same_addr) {
  if (preds->empty() || succs->empty())
    return;

  if (same_addr) {
    // for idiom-2, idiom-3
    for (std::vector<ObserverAccess>::iterator pit = preds->begin();
         pit != preds->end(); ++pit) {
      ObserverAccess &pa = *pit;
      bool idiom2_exists = false;
      for (ObserverLocalInfo::SuccVec::iterator sit = succs->begin();
           sit != succs->end(); ++sit) {
        ObserverAccess &sa = (*sit).succ;

        if (sa.thd_id_ == pa.thd_id_ && sa.clk_ < pa.clk_) {
          iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_access->inst_,
                                                    prev_access->type_,
                                                    false);
          iRootEvent *e1 = iroot_db_->GetiRootEvent(sa.inst_, sa.type_, false);
          iRootEvent *e2 = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, false);
          iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_access->inst_,
                                                    curr_access->type_,
                                                    false);
          iRoot *iroot = iroot_db_->GetiRoot(IDIOM_3, false, e0, e1, e2, e3);
          memo_->Observed(iroot, shadow_, false);
        }

        if (!idiom2_exists &&
            sa.thd_id_ == pa.thd_id_ &&
            sa.clk_ == pa.clk_ &&
            sa.type_ == pa.type_ &&
            sa.inst_ == pa.inst_) {
          idiom2_exists = true;
        }
      }

      if (idiom2_exists) {
        iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_access->inst_,
                                                  prev_access->type_,
                                                  false);
        iRootEvent *e1 = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, false);
        iRootEvent *e2 = iroot_db_->GetiRootEvent(curr_access->inst_,
                                                  curr_access->type_,
                                                  false);
        iRoot *iroot = iroot_db_->GetiRoot(IDIOM_2, false, e0, e1, e2);
        memo_->Observed(iroot, shadow_, false);
      }
    }

  } else {
    // for idiom-4, idiom-5
    for (std::vector<ObserverAccess>::iterator pit = preds->begin();
         pit != preds->end(); ++pit) {
      ObserverAccess &pa = *pit;
      for (ObserverLocalInfo::SuccVec::iterator sit = succs->begin();
           sit != succs->end(); ++sit) {
        ObserverAccess &sa = (*sit).succ;

        if (sa.thd_id_ == pa.thd_id_) {
          if (sa.clk_ < pa.clk_) {
            iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_access->inst_,
                                                      prev_access->type_,
                                                      false);
            iRootEvent *e1 = iroot_db_->GetiRootEvent(sa.inst_,
                                                      sa.type_,
                                                      false);
            iRootEvent *e2 = iroot_db_->GetiRootEvent(pa.inst_,
                                                      pa.type_,
                                                      false);
            iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_access->inst_,
                                                      curr_access->type_,
                                                      false);
            iRoot *iroot = iroot_db_->GetiRoot(IDIOM_4, false, e0, e1, e2, e3);
            memo_->Observed(iroot, shadow_, false);
          } else if (sa.clk_ > pa.clk_) {
            if (TIME_DISTANCE(pa.clk_, sa.clk_) < vw_) {
              // need to check whether there exists access between sa
              // and pa that accesses the same location as sa or pa
              // check whether pa is in local_prev_vec
              for (std::vector<ObserverAccess>::iterator it =
                      (*sit).local_prev_vec.begin();
                      it != (*sit).local_prev_vec.end(); ++it) {
                if ((*it).clk_ == pa.clk_ &&
                    (*it).type_ == pa.type_ &&
                    (*it).inst_ == pa.inst_) {
                  iRootEvent *e0 = iroot_db_->GetiRootEvent(prev_access->inst_,
                                                            prev_access->type_,
                                                            false);
                  iRootEvent *e1 = iroot_db_->GetiRootEvent(sa.inst_,
                                                            sa.type_,
                                                            false);
                  iRootEvent *e2 = iroot_db_->GetiRootEvent(pa.inst_,
                                                            pa.type_,
                                                            false);
                  iRootEvent *e3 = iroot_db_->GetiRootEvent(curr_access->inst_,
                                                            curr_access->type_,
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
        }
      }
    }
  }
}

} // namespace idiom

