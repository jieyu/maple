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

// File: idiom/observer.h - Define iRoot observer analyzer.

#ifndef IDIOM_OBSERVER_H_
#define IDIOM_OBSERVER_H_

#include <list>
#include <vector>
#include <map>
#include <set>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/knob.h"
#include "core/analyzer.h"
#include "core/static_info.h"
#include "core/filter.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"
#include "sinst/sinst.h"

namespace idiom {

class Observer;

// Access.
class ObserverAccess {
 public:
  ObserverAccess()
      : thd_id_(INVALID_THD_ID),
        clk_(0),
        type_(IROOT_EVENT_INVALID),
        inst_(NULL) {}
  ObserverAccess(thread_id_t thd_id, timestamp_t clk,
                 iRootEventType type, Inst *inst)
      : thd_id_(thd_id),
        clk_(clk),
        type_(type),
        inst_(inst) {}
  ~ObserverAccess() {}

  bool IsMem() {
    if (type_ == IROOT_EVENT_MEM_READ ||
        type_ == IROOT_EVENT_MEM_WRITE)
      return true;
    else
      return false;
  }

  bool IsSync() {
    if (type_ == IROOT_EVENT_MUTEX_LOCK ||
        type_ == IROOT_EVENT_MUTEX_UNLOCK)
      return true;
    else
      return false;
  }

 private:
  thread_id_t thd_id_;
  timestamp_t clk_;
  iRootEventType type_;
  Inst *inst_;

  friend class Observer;

  // using default copy constructor and assignment operator
};

// Meta data.
class ObserverMeta {
 public:
  ObserverMeta() {}
  virtual ~ObserverMeta() {}

 private:
  DISALLOW_COPY_CONSTRUCTORS(ObserverMeta);
};

// Memory access meta data.
class ObserverMemMeta : public ObserverMeta {
 public:
  ObserverMemMeta() { last_writer_.first = false; }
  ~ObserverMemMeta() {}

 private:
  typedef std::pair<bool, ObserverAccess> Accessor;
  typedef std::map<thread_id_t, Accessor> ReaderMap;

  Accessor last_writer_;
  ReaderMap last_readers_;

  friend class Observer;

  DISALLOW_COPY_CONSTRUCTORS(ObserverMemMeta);
};

// Mutex variable meta data.
class ObserverMutexMeta : public ObserverMeta {
 public:
  ObserverMutexMeta() { last_unlocker_.first = false; }
  ~ObserverMutexMeta() {}

 private:
  typedef std::pair<bool, ObserverAccess> Accessor;

  Accessor last_unlocker_;

  friend class Observer;

  DISALLOW_COPY_CONSTRUCTORS(ObserverMutexMeta);
};

// Local information.
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

  // using default copy constructor and assignment operator
};

// iRoot observer which analyzes which iRoots are tested.
class Observer : public Analyzer {
 public:
  Observer();
  ~Observer();

  void Register();
  bool Enabled();
  void Setup(Mutex *lock, StaticInfo *sinfo, iRootDB *iroot_db, Memo *memo,
             sinst::SharedInstDB *sinst_db);
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

 private:
  typedef std::tr1::unordered_map<address_t, ObserverMeta *> MetaMap;

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
  iRootDB *iroot_db_;
  Memo *memo_;
  sinst::SharedInstDB *sinst_db_; // could be null
  bool shadow_; // shadow observer
  bool sync_only_;
  address_t unit_size_;
  bool complex_idioms_;
  timestamp_t vw_; // vulnerability window
  RegionFilter *filter_;
  std::map<thread_id_t, ObserverLocalInfo> local_info_map_;
  MetaMap meta_map_;

  DISALLOW_COPY_CONSTRUCTORS(Observer);
};

} // namespace idiom

#endif

