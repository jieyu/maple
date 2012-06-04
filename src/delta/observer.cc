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

// File: delta/observer.cc - Implementation of the iRoot observer analyzer.

#include "delta/observer.h"

#include "core/logging.h"
#include "core/knob.h"

#define UNIT_MASK(unit_size) (~((unit_size) - 1))
#define UNIT_DOWN_ALIGN(addr,unit_size) ((addr) & UNIT_MASK(unit_size))
#define UNIT_UP_ALIGN(addr,unit_size) \
    (((addr) + (unit_size) - 1) & UNIT_MASK(unit_size))

#define TIME_DISTANCE(start,end) \
    ((end) >= (start) ? \
    (end) - (start) : \
    (((timestamp_t)0) - ((start) - (end))))

namespace delta {

Observer::Observer(Knob *knob)
    : Analyzer(knob),
      internal_lock_(NULL),
      sinfo_(NULL) {
}

Observer::~Observer() {
  delete internal_lock_;
  delete filter_;
}

void Observer::Register() {
  knob_->RegisterBool("enable_observer", "Enable iroot observer.", "1");
  knob_->RegisterBool("type1", "Record idiom type1", "0");
  knob_->RegisterBool("type2", "Record idiom type2", "0");
  knob_->RegisterBool("type3", "Record idiom type3", "0");
  knob_->RegisterBool("type4", "Record idiom type4", "0");
  knob_->RegisterBool("type5", "Record idiom type5", "0");
  knob_->RegisterInt("vw", "Vulnerability window.", "1000");
  knob_->RegisterInt("unit_size", "Granularity of conflict detection.", "4");
}

bool Observer::Enabled() {
  return knob_->ValueBool("enable_observer");
}

void Observer::Setup(Mutex *lock, StaticInfo *sinfo, idiom::iRootDB *iroot, iList *ilist) {
  internal_lock_ = lock;
  sinfo_ = sinfo;
  iroot_db_ = iroot;
  ilist_ = ilist;

  unit_size_ = knob_->ValueInt("unit_size");
  type1_ = knob_->ValueBool("type1");
  type2_ = knob_->ValueBool("type2");
  type3_ = knob_->ValueBool("type3");
  type4_ = knob_->ValueBool("type4");
  type5_ = knob_->ValueBool("type5");
  vw_ = knob_->ValueInt("vw");
  complex_idioms_ = type2_ || type3_ || type4_ || type5_;
  filter_ = new RegionFilter(internal_lock_->Clone());

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
      continue;
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
                             idiom::IROOT_EVENT_MEM_READ, inst);
  std::vector<ObserverAccess> preds;

  ObserverMemMeta::ReaderMap::iterator reader_it
      = meta->last_readers_.find(curr_thd_id);
  if (reader_it == meta->last_readers_.end() || !reader_it->second.first) {
    if (meta->last_writer_.first &&
        meta->last_writer_.second.thd_id_ != curr_thd_id) {
      // RAW dependency
      preds.push_back(meta->last_writer_.second);
    }
  }

  if (type1_)
    UpdateiRoots(&curr_access, &preds);

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
                             idiom::IROOT_EVENT_MEM_WRITE, inst);
  std::vector<ObserverAccess> preds;

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

  if (!war_exist) {
    if (meta->last_writer_.first &&
        meta->last_writer_.second.thd_id_ != curr_thd_id) {
      // WAW dependency
      preds.push_back(meta->last_writer_.second);
    }
  }

  if (type1_)
    UpdateiRoots(&curr_access, &preds);

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
                             idiom::IROOT_EVENT_MUTEX_LOCK, inst);
  std::vector<ObserverAccess> preds;

  if (meta->last_unlocker_.first &&
      meta->last_unlocker_.second.thd_id_ != curr_thd_id) {
    // U->L dependency
    preds.push_back(meta->last_unlocker_.second);
  }

  if (type1_)
    UpdateiRoots(&curr_access, &preds);

  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);
}

void Observer::UpdateForUnlock(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               Inst *inst, address_t addr,
                               ObserverMutexMeta *meta) {
  ObserverAccess curr_access(curr_thd_id, curr_thd_clk,
                             idiom::IROOT_EVENT_MUTEX_UNLOCK, inst);
  std::vector<ObserverAccess> preds;

  if (complex_idioms_)
    UpdateLocalInfo(&curr_access, addr, &preds);

  meta->last_unlocker_.first = true;
  meta->last_unlocker_.second = curr_access;
}

void Observer::UpdateLocalInfo(ObserverAccess *curr_access, address_t addr,
                               std::vector<ObserverAccess> *preds) {
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
              access.inst_ == vit->access.inst_ &&
              access.type_ == vit->access.type_) {
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
  for (std::vector<ObserverAccess>::iterator it = preds->begin();
       it != preds->end(); ++it) {
    idiom::iRootEvent *iroot_event_pred = iroot_db_->GetiRootEvent(it->inst_, it->type_, true);
    idiom::iRootEvent *iroot_event_curr = iroot_db_->GetiRootEvent(curr_access->inst_, curr_access->type_, true);
    idiom::iRoot *iroot = iroot_db_->GetiRoot(idiom::IDIOM_1, true, iroot_event_pred, iroot_event_curr);
    ilist_->Update(iroot, true);
  }
}

void Observer::UpdateComplexiRoots(ObserverAccess *curr_access,
                                   std::vector<ObserverAccess> *preds,
                                   ObserverAccess *prev_access,
                                   ObserverLocalInfo::SuccVec *succs,
                                   bool same_addr) {
  if (preds->empty() || succs->empty())
    return;

  if (same_addr && (type2_ || type3_)) {
    // for idiom-2, idiom-3
    for (std::vector<ObserverAccess>::iterator pit = preds->begin();
         pit != preds->end(); ++pit) {
      ObserverAccess &pa = *pit;
      bool idiom2_exists = false;
      for (ObserverLocalInfo::SuccVec::iterator sit = succs->begin();
           sit != succs->end(); ++sit) {
        ObserverAccess &sa = (*sit).succ;

        if (sa.thd_id_ == pa.thd_id_ && sa.clk_ < pa.clk_ && type3_) {
         idiom::iRootEvent *iroot_event_pred = iroot_db_->GetiRootEvent(prev_access->inst_, prev_access->type_, true);
         idiom::iRootEvent *iroot_event_first_remo = iroot_db_->GetiRootEvent(sa.inst_, sa.type_, true);
         idiom::iRootEvent *iroot_event_last_remo = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, true);
         idiom::iRootEvent *iroot_event_curr = iroot_db_->GetiRootEvent(curr_access->inst_, curr_access->type_, true);
         idiom::iRoot *iroot = iroot_db_->GetiRoot(idiom::IDIOM_3, true, iroot_event_pred,
                                                   iroot_event_first_remo,
                                                   iroot_event_last_remo,
                                                   iroot_event_curr);
         ilist_->Update(iroot, true);
        }

        if (!idiom2_exists && sa.thd_id_ == pa.thd_id_ && sa.clk_ == pa.clk_ &&
            sa.inst_ == pa.inst_ && sa.type_ == pa.type_) {
          idiom2_exists = true;
        }
      }

      if (idiom2_exists && type2_) {
         idiom::iRootEvent *iroot_event_pred = iroot_db_->GetiRootEvent(prev_access->inst_, prev_access->type_, true);
         idiom::iRootEvent *iroot_event_remo = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, true);
         idiom::iRootEvent *iroot_event_curr = iroot_db_->GetiRootEvent(curr_access->inst_, curr_access->type_, true);
         idiom::iRoot *iroot = iroot_db_->GetiRoot(idiom::IDIOM_2, true, iroot_event_pred,
                                                   iroot_event_remo,
                                                   iroot_event_curr);
         ilist_->Update(iroot, true);
      }
    }

  }
  else if (same_addr == 0 && (type4_ || type5_)) {    // for idiom-4, idiom-5
    for (std::vector<ObserverAccess>::iterator pit = preds->begin();
         pit != preds->end(); ++pit) {
      ObserverAccess &pa = *pit;
      for (ObserverLocalInfo::SuccVec::iterator sit = succs->begin();
           sit != succs->end(); ++sit) {
        ObserverAccess &sa = (*sit).succ;

        if (sa.thd_id_ == pa.thd_id_) {
          if ((sa.clk_ < pa.clk_) && type4_) {
            idiom::iRootEvent *iroot_event_pred = iroot_db_->GetiRootEvent(prev_access->inst_, prev_access->type_, true);
            idiom::iRootEvent *iroot_event_first_remo = iroot_db_->GetiRootEvent(sa.inst_, sa.type_, true);
            idiom::iRootEvent *iroot_event_last_remo = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, true);
            idiom::iRootEvent *iroot_event_curr = iroot_db_->GetiRootEvent(curr_access->inst_, curr_access->type_, true);
            idiom::iRoot *iroot = iroot_db_->GetiRoot(idiom::IDIOM_4, true, iroot_event_pred,
                                                      iroot_event_first_remo,
                                                      iroot_event_last_remo,
                                                      iroot_event_curr);
            ilist_->Update(iroot, true);
          } else if ((sa.clk_ > pa.clk_) && type5_) {
            if (TIME_DISTANCE(pa.clk_, sa.clk_) < vw_) {
              // need to check whether there exists access between sa
              // and pa that accesses the same location as sa or pa
              // check whether pa is in local_prev_vec
              for (std::vector<ObserverAccess>::iterator it =
                      (*sit).local_prev_vec.begin();
                      it != (*sit).local_prev_vec.end(); ++it) {
                if ((*it).clk_ == pa.clk_ &&
                    (*it).inst_ == pa.inst_ &&
                    (*it).type_ == pa.type_) {
                  idiom::iRootEvent *iroot_event_pred = iroot_db_->GetiRootEvent(prev_access->inst_, prev_access->type_, true);
                  idiom::iRootEvent *iroot_event_first_remo = iroot_db_->GetiRootEvent(sa.inst_, sa.type_, true);
                  idiom::iRootEvent *iroot_event_last_remo = iroot_db_->GetiRootEvent(pa.inst_, pa.type_, true);
                  idiom::iRootEvent *iroot_event_curr = iroot_db_->GetiRootEvent(curr_access->inst_, curr_access->type_, true);
                  idiom::iRoot *iroot = iroot_db_->GetiRoot(idiom::IDIOM_5, true, iroot_event_pred,
                                                            iroot_event_first_remo,
                                                            iroot_event_last_remo,
                                                            iroot_event_curr);
                  ilist_->Update(iroot, true);
                  idiom::iRoot *irootx = iroot_db_->GetiRoot(idiom::IDIOM_5, true, iroot_event_last_remo,
                                                             iroot_event_curr,
                                                             iroot_event_pred,
                                                             iroot_event_first_remo);
                  ilist_->Update(irootx, true);
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

