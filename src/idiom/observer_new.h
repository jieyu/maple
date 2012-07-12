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

// File: idiom/observer_new.h - Define new iRoot observer analyzer.

#ifndef IDIOM_OBSERVER_NEW_H_
#define IDIOM_OBSERVER_NEW_H_

#include <vector>
#include <map>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/knob.h"
#include "core/static_info.h"
#include "core/analyzer.h"
#include "core/filter.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"
#include "sinst/sinst.h"

namespace idiom {

// The new iRoot observer which analyzes which iRoots are tested.
class ObserverNew : public Analyzer {
 public:
  ObserverNew();
  ~ObserverNew();

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

 protected:
  // unique id for each access
  typedef uint64 acc_uid_t;

  // the dynamic access
  class Acc {
   public:
    typedef std::vector<Acc> Vec;

    Acc()
        : uid(0),
          thd_id(INVALID_THD_ID),
          thd_clk(INVALID_TIMESTAMP),
          type(IROOT_EVENT_INVALID),
          inst(NULL) {}

    ~Acc() {}

    acc_uid_t uid;
    thread_id_t thd_id;
    timestamp_t thd_clk;
    iRootEventType type;
    Inst *inst;
  };

  // the meta data for iroot events
  class Meta {
   public:
    typedef enum {
      TYPE_INVALID = 0,
      TYPE_MEM,
      TYPE_MUTEX,
    } Type;
    class LastAcc {
     public:
      typedef std::map<thread_id_t, LastAcc> Table;

      LastAcc() : valid(false) {}
      ~LastAcc() {}

      bool valid;
      Acc acc;
    };
    typedef std::tr1::unordered_set<Meta *> HashSet;
    typedef std::tr1::unordered_map<address_t, Meta *> Table;

    explicit Meta(Type t) : type(t) {}
    ~Meta() {}

    Type type;
    LastAcc::Table last_acc_table;
    LastAcc last_writer;
  };

  // the information about recent accesses
  class RecentInfo {
   public:
    // recent access entry
    class Entry {
     public:
      typedef std::vector<Acc::Vec> SuccPrevs;
      typedef std::vector<Entry> Vec;

      Entry() : meta(NULL) {}
      ~Entry() {}

      Meta *meta;
      Acc acc;
      Acc::Vec succs;
      SuccPrevs succ_prevs; // the prevs of each succ
    };
    typedef std::map<thread_id_t, RecentInfo> Table;

    RecentInfo() : curr_thd_clk(INVALID_TIMESTAMP) {}
    ~RecentInfo() {}

    timestamp_t curr_thd_clk; // the latest known thread local clock
    Entry::Vec entry_queue;
  };

  // helper functions
  acc_uid_t GetNextAccUid() { return ATOMIC_ADD_AND_FETCH(&curr_acc_uid_, 1); }
  bool IsRead(iRootEventType type) { return type == IROOT_EVENT_MEM_READ; }
  void RecentInfoGC(thread_id_t curr_thd_id,
                    timestamp_t curr_thd_clk,
                    size_t threshold);
  bool CheckLocalPair(iRootEventType prev, iRootEventType curr);
  void InitLpValidTable();
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr) { return filter_->Filter(addr, false); }
  Meta *GetMemMeta(address_t iaddr);
  Meta *GetMutexMeta(address_t iaddr);

  // main processing functions
  void UpdateiRoot(Acc *curr_acc, Acc::Vec *preds);
  void UpdateComplexiRoot(Acc *curr_acc,
                          Meta *curr_meta,
                          Acc::Vec *preds,
                          RecentInfo::Entry *prev_ri_entry);
  void ProcessiRootEvent(thread_id_t curr_thd_id,
                         timestamp_t curr_thd_clk,
                         iRootEventType type,
                         Inst *inst,
                         Meta *meta);
  void ProcessFree(Meta *meta);
  void ProcessRecentInfo(Acc *curr_acc,
                         Meta *curr_meta,
                         Acc::Vec *preds);

  // common databases
  Mutex *internal_lock_;
  StaticInfo *sinfo_;
  iRootDB *iroot_db_;
  Memo *memo_;
  sinst::SharedInstDB *sinst_db_;

  // settings and flags
  bool shadow_;
  bool sync_only_;
  bool complex_idioms_;
  bool single_var_idioms_;
  address_t unit_size_;
  timestamp_t vw_;

  // meta data
  Meta::Table meta_table_;

  // global analysis state
  RegionFilter *filter_;
  acc_uid_t curr_acc_uid_;
  bool lp_valid_table_[IROOT_EVENT_TYPE_ARRAYSIZE][IROOT_EVENT_TYPE_ARRAYSIZE];

  // complex idioms related
  RecentInfo::Table ri_table_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(ObserverNew);
};

} // namespace idiom

#endif

