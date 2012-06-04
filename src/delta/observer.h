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

// File: delta/observer.h - Define iRoot observer analyzer.

#ifndef DELTA_OBSERVER_H_
#define DELTA_OBSERVER_H_

#include <list>
#include <vector>
#include <map>
#include <set>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include "core/basictypes.h"
#include "core/sync.h"
#include "core/knob.h"
#include "core/analyzer.h"
#include "core/static_info.h"
#include "core/filter.h"
#include "idiom/iroot.h"
#include "ilist.h"

namespace delta {

class Observer;

class ObserverAccess {
 public:
  ObserverAccess() : thd_id_(INVALID_THD_ID), clk_(0) {}
  ObserverAccess(thread_id_t thd_id, timestamp_t clk,
                 idiom::iRootEventType type, Inst *inst)
      : thd_id_(thd_id), clk_(clk), type_(type), inst_(inst) {}
  ~ObserverAccess() {}

 private:
  thread_id_t thd_id_;
  timestamp_t clk_;
  idiom::iRootEventType type_;
  Inst *inst_;

  friend class Observer;
};


class ObserverMeta {
 public:
  ObserverMeta() {}
  virtual ~ObserverMeta() {}

 private:
  DISALLOW_COPY_CONSTRUCTORS(ObserverMeta);
};

class ObserverMemMeta : public ObserverMeta {
 public:
  ObserverMemMeta() {last_writer_.first = false;}
  ~ObserverMemMeta() {}

 private:
  typedef std::pair<bool, ObserverAccess> Accessor;
  typedef std::map<thread_id_t, Accessor> ReaderMap;
  //typedef std::vector<Accessor> AccessorVec;
  //typedef std::map<thread_id_t, AccessorVec> Readermap;

  Accessor last_writer_;
  ReaderMap last_readers_;

  friend class Observer;

  DISALLOW_COPY_CONSTRUCTORS(ObserverMemMeta);
};

class ObserverMutexMeta : public ObserverMeta {
 public:
  ObserverMutexMeta() {last_unlocker_.first = false;}
  ~ObserverMutexMeta() {}

 private:
  typedef std::pair<bool, ObserverAccess> Accessor;

  Accessor last_unlocker_;

  friend class Observer;

  DISALLOW_COPY_CONSTRUCTORS(ObserverMutexMeta);
};

class ObserverLocalInfo {
 public:
  ObserverLocalInfo() {}
  ~ObserverLocalInfo() {}

  void Clear() { entries_.clear(); }

 private:
  typedef struct {
    ObserverAccess succ;
    std::vector<ObserverAccess> local_prev_vec; // for idiom-5
  } SuccEntry;
  typedef std::vector<SuccEntry> SuccVec;
  typedef struct {
    address_t addr;
    ObserverAccess access;
    SuccVec succs;
  } EntryType;
  typedef std::vector<EntryType> EntryVec;
  typedef std::map<timestamp_t, EntryVec> EntryMap;

  EntryMap entries_;

  friend class Observer;

};

class Observer : public Analyzer {
 public:
  explicit Observer(Knob *knob);
  ~Observer();

  void Register();
  bool Enabled();  
  void Setup(Mutex *lock, StaticInfo *sinfo, idiom::iRootDB *iroot_db, iList *ilist);  
  
  void ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                 address_t data_start, size_t data_size, address_t bss_start,
                 size_t bss_size);
  void ImageUnload(Image *image, address_t low_addr, address_t high_addr,
                   address_t data_start, size_t data_size, address_t bss_start,
                   size_t bss_size);
  void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id);
  void ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk);
  void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size);
  void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, size_t size);
  void AfterPthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr);
  void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr);
  void BeforePthreadCondWait(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t cond_addr,
                             address_t mutex_addr);
  void AfterPthreadCondWait(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t cond_addr,
                            address_t mutex_addr);
  void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                  timestamp_t curr_thd_clk, Inst *inst,
                                  address_t cond_addr, address_t mutex_addr);
  void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                 timestamp_t curr_thd_clk, Inst *inst,
                                 address_t cond_addr, address_t mutex_addr);
  void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr);
  void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t nmemb, size_t size, address_t addr);
  void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t ori_addr, size_t size);
  void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    Inst *inst, address_t ori_addr, size_t size,
                    address_t new_addr);
  void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                  Inst *inst, address_t addr);
  void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                   Inst *inst, size_t size, address_t addr);

  static void RegisterKnobs(Knob *knob);

 private:
  typedef std::tr1::unordered_map<address_t, ObserverMeta *> MetaMap;
  typedef std::map<thread_id_t, ObserverLocalInfo> LocalInfoMap;

  ObserverMemMeta *GetMemMeta(address_t iaddr);
  ObserverMutexMeta *GetMutexMeta(address_t iaddr);
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr);
  void UpdateOnFree(ObserverMeta *meta);
  void UpdateForRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, ObserverMemMeta *meta);
  void UpdateForWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, ObserverMemMeta *meta);
  void UpdateForLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, ObserverMutexMeta *meta);
  void UpdateForUnlock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                       Inst *inst, address_t addr, ObserverMutexMeta *meta);
  void UpdateLocalInfo(ObserverAccess *curr_access, address_t addr,
                       std::vector<ObserverAccess> *preds);  
  void UpdateiRoots(ObserverAccess *curr_access,
                    std::vector<ObserverAccess> *preds);
  void UpdateComplexiRoots(ObserverAccess *curr_access,
                           std::vector<ObserverAccess> *preds,
                           ObserverAccess *prev_access,
                           ObserverLocalInfo::SuccVec *succs,
                           bool same_addr);  


  Mutex *internal_lock_;
  StaticInfo *sinfo_;
  idiom::iRootDB *iroot_db_;
  iList *ilist_;
  address_t unit_size_;
  bool sync_only_;  
  bool complex_idioms_;
  bool type1_;
  bool type2_;
  bool type3_;
  bool type4_;
  bool type5_;
  timestamp_t vw_;
  RegionFilter *filter_;
  MetaMap meta_map_;
  LocalInfoMap local_info_map_;

  DISALLOW_COPY_CONSTRUCTORS(Observer);
};

} // namespace idiom

#endif

