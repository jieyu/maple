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

// File: idiom/predictor_new.h - Define new iRoot predictor analyzer.

#ifndef IDIOM_PREDICTOR_NEW_H_
#define IDIOM_PREDICTOR_NEW_H_

#include <map>
#include <set>
#include <list>
#include <tr1/unordered_map>
#include <stack>

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/vector_clock.h"
#include "core/lock_set.h"
#include "core/filter.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"
#include "sinst/sinst.h"

namespace idiom {

class PredictorNew : public Analyzer {
 public:
  PredictorNew();
  ~PredictorNew();

  void Register();
  bool Enabled();
  void Setup(Mutex *lock, StaticInfo *sinfo, iRootDB *iroot_db, Memo *memo,
             sinst::SharedInstDB *sinst_db);
  void ProgramExit();
  void ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                 address_t data_start, size_t data_size, address_t bss_start,
                 size_t bss_size);
  void ImageUnload(Image *image, address_t low_addr, address_t high_addr,
                   address_t data_start, size_t data_size, address_t bss_start,
                   size_t bss_size);
  void SyscallEntry(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                    int syscall_num);
  void SignalReceived(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      int signal_num);
  void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id);
  void ThreadExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk);
  void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size);
  void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, size_t size);
  void BeforeAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, std::string type, address_t addr);
  void AfterAtomicInst(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                       Inst *inst, std::string type, address_t addr);
  void AfterPthreadJoin(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                        Inst *inst, thread_id_t child_thd_id);
  void AfterPthreadMutexLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr);
  void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr);
  void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr);
  void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
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
  void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                address_t addr);
  void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               address_t addr);
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
  // forward declaration
  class Meta;

  // the flagged lock set
  class FLockSet {
   public:
    typedef std::pair<bool, bool> Flag;
    typedef std::map<address_t, Flag> LockFlagTable;

    FLockSet() {}
    ~FLockSet() {}

    LockFlagTable lock_flag_table;
  };

  // the container for thread local clock values
  class ThdClkInfo {
   public:
    ThdClkInfo() : start(INVALID_TIMESTAMP), end(INVALID_TIMESTAMP) {}
    ThdClkInfo(timestamp_t s) : start(s), end(s) {}
    ~ThdClkInfo() {}

    timestamp_t start;
    timestamp_t end;
  };

  // the access summary in the access history
  class AccSum {
   public:
    typedef std::vector<AccSum *> Vec;
    typedef std::pair<AccSum *, AccSum *> Pair;
    typedef std::pair<VectorClock, ThdClkInfo> TimeInfoEntry;
    typedef std::vector<TimeInfoEntry> TimeInfo;
    typedef std::tr1::unordered_map<size_t, Vec> HashIndex;
    typedef std::tr1::unordered_map<AccSum *, Vec> PairIndex;

    AccSum()
        : meta(NULL),
          thd_id(INVALID_THD_ID),
          type(IROOT_EVENT_INVALID),
          inst(NULL) {}

    ~AccSum() {}

    Meta *meta;
    thread_id_t thd_id;
    iRootEventType type;
    Inst *inst;
    FLockSet fls;
    TimeInfo tinfo;

  };

 public : 
  std::map <std::pair<AccSum*, AccSum*>*, std::pair<int, int>*> iroot_inst_count_map ;

 protected :
  // the dynamic access
  class DynAcc {
   public:
    DynAcc()
        : meta(NULL),
          thd_id(INVALID_THD_ID),
          thd_clk(INVALID_TIMESTAMP),
          type(IROOT_EVENT_INVALID),
          inst(NULL) {}

    ~DynAcc() {}

    Meta *meta;
    thread_id_t thd_id;
    timestamp_t thd_clk;
    iRootEventType type;
    Inst *inst;
    VectorClock vc;
    LockSet ls;
    FLockSet fls;
  };

  // the access history
  class AccHisto {
   public:
    typedef std::map<thread_id_t, AccSum::Vec> AccSumTable;
    typedef std::map<thread_id_t, DynAcc> DynAccTable;

    AccHisto() {}
    ~AccHisto() {}

    AccSumTable acc_sum_table;
    DynAccTable last_dyn_acc_table; // the last dynamic access for each thread
  };

  // the information about recent accesses
  class RecentInfo {
   public:
    class Entry {
     public:
      typedef std::vector<Entry> Vec;

      Entry() : thd_clk(INVALID_THD_ID), acc_sum(NULL) {}
      ~Entry() {}

      timestamp_t thd_clk;
      AccSum *acc_sum;
      VectorClock vc;
      LockSet ls;
      Meta *meta;
    };
    typedef std::map<thread_id_t, RecentInfo> Table;
    typedef std::tr1::unordered_map<Meta *, size_t> RawEntryIndex;

    RecentInfo() {}
    ~RecentInfo() {}

    Entry::Vec entry_vec;
    RawEntryIndex raw_entry_index;
  };

  // the local pair (only used at exit)
  class LocalPair {
   public:
    typedef std::vector<LocalPair> Vec;
    typedef std::map<thread_id_t, Vec> Table;
    typedef std::tr1::unordered_map<AccSum *, Vec> Index;
    typedef std::tr1::unordered_map<AccSum *, Index> PairIndex;

    LocalPair()
        : prev_entry(NULL),
          curr_entry(NULL),
          succ_acc_sum(NULL),
          pred_acc_sum(NULL) {}

    ~LocalPair() {}

    RecentInfo::Entry *prev_entry;
    RecentInfo::Entry *curr_entry;
    AccSum *succ_acc_sum;
    AccSum *pred_acc_sum;
  };

  // the meta data for iroot events
  class Meta {
   public:
    typedef enum {
      TYPE_INVALID = 0,
      TYPE_MEM,
      TYPE_MUTEX,
    } Type;
    typedef std::tr1::unordered_set<Meta *> HashSet;
    typedef std::tr1::unordered_map<address_t, Meta *> Table;

    explicit Meta(Type t) : type(t) { acc_histo = new AccHisto; }
    ~Meta() { if (acc_histo) delete acc_histo; }


    Type type;
    AccHisto *acc_histo;
  };

  // the meta data for conditional variables to track vector clock
  class CondMeta {
   public:
    typedef uint32 signal_id_t;
    typedef std::map<signal_id_t, VectorClock> SignalMap;
    typedef struct {
      bool timed;
      bool broadcasted;
      VectorClock broadcast_vc;
      SignalMap signal_map;
    } WaitInfo;
    typedef std::map<thread_id_t, WaitInfo> WaitMap;
    typedef std::tr1::unordered_map<address_t, CondMeta *> Table;

    CondMeta() : curr_signal_id(0) {}
    ~CondMeta() {}

    signal_id_t curr_signal_id;
    WaitMap wait_map;
  };

  // the meta data for barrier variables to track vector clock
  class BarrierMeta {
   public:
    typedef std::map<thread_id_t, std::pair<VectorClock, bool> > VectorClockMap;
    typedef std::tr1::unordered_map<address_t, BarrierMeta *> Table;

    BarrierMeta()
        : pre_using_table1(true),
          post_using_table1(true) {}

    ~BarrierMeta() {}

    bool pre_using_table1;
    bool post_using_table1;
    VectorClockMap barrier_wait_table1;
    VectorClockMap barrier_wait_table2;
  };

  // the meta data for shared memory accesses
  class SharedMeta {
   public:
    typedef std::tr1::unordered_map<address_t, SharedMeta> Table;

    SharedMeta()
        : shared(false),
          has_write(false),
          multi_read(false),
          last_thd_id(INVALID_THD_ID),
          first_inst(NULL) {}

    ~SharedMeta() {}

    bool shared;
    bool has_write; // whether has write
    bool multi_read; // read from mutiple threads
    thread_id_t last_thd_id;
    Inst *first_inst;
  };

  // helper functions for simple idiom
  size_t Hash(Meta *meta) { return (size_t)meta; }
  size_t Hash(iRootEventType type) { return (size_t)type; }
  size_t Hash(Inst *inst) { return (size_t)inst; }
  size_t Hash(VectorClock *vc);
  size_t Hash(FLockSet *fls);
  size_t Hash(AccSum *acc_sum);
  size_t Hash(DynAcc *dyn_acc);
  bool Match(FLockSet *fls1, FLockSet *fls2);
  void AddThdClk(ThdClkInfo *info, timestamp_t thd_clk);
  timestamp_t GetSmallestThdClk(ThdClkInfo *info) { return info->start; }
  timestamp_t GetLargestThdClk(ThdClkInfo *info) { return info->end; }
  void ClearFlag(FLockSet *fls);
  void UpdateFirstFlag(FLockSet *fls, LockSet *last_ls, LockSet *curr_ls);
  void UpdateLastFlag(FLockSet *fls, LockSet *last_ls, LockSet *curr_ls);
  void CommonLockSet(FLockSet *fls, LockSet *prev_ls, LockSet *curr_ls);
  bool ExistAccSumPair(AccSum *src, AccSum *dst);
  void AddAccSumPair(AccSum *src, AccSum *dst);
  AccSum *MatchAccSum(DynAcc *dyn_acc);
  bool CheckConflict(iRootEventType src, iRootEventType dst);
  bool CheckMutexExclution(FLockSet *src_fls, FLockSet *dst_fls);
  bool CheckConcurrent(VectorClock *vc, AccSum *rmt);
  bool CheckAsync(AccSum *acc_sum);
  bool CheckAtomic(AccSum *src, AccSum *dst);
  void InitConflictTable();
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr) { return filter_->Filter(addr, false); }
  Meta *GetMemMeta(address_t iaddr);
  Meta *GetMutexMeta(address_t iaddr);
  CondMeta *GetCondMeta(address_t iaddr);
  BarrierMeta *GetBarrierMeta(address_t iaddr);
  void PredictiRoot();
  AccSum *ProcessAccSumUpdate(DynAcc *dyn_acc);
  void ProcessAccSumPairUpdate(Meta *meta);

  // helper functions for complex idioms
  void UpdateLocalPair(thread_id_t thd_id,
                       RecentInfo::Entry *prev_entry,
                       RecentInfo::Entry *curr_entry,
                       AccSum *succ_acc_sum,
                       AccSum *pred_acc_sum);
  void UpdateDeadlockPair(thread_id_t thd_id,
                          RecentInfo::Entry *prev_entry,
                          RecentInfo::Entry *curr_entry);
  bool CheckCompound(RecentInfo::Entry *prev_entry,
                     RecentInfo::Entry *curr_entry,
                     AccSum *succ_acc_sum,
                     AccSum *pred_acc_sum);
  bool CheckCompound2(RecentInfo::Entry *prev_entry,
                      RecentInfo::Entry *curr_entry,
                      AccSum *succ_acc_sum,
                      AccSum *pred_acc_sum);
  bool CheckDeadlock(LocalPair *dl, LocalPair *rmt_dl);
  iRoot *Predict(IdiomType idiom, ...);
  void PredictComplexiRoot();
  void ProcessRecentInfoUpdate(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk,
                               VectorClock *curr_vc,
                               LockSet *curr_ls,
                               Meta *meta);
  void ProcessRecentInfoMaturize(AccSum *acc_sum);

  // main entries
  void ProcessiRootEvent(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                         iRootEventType type, Inst *inst, Meta *meta);
  void ProcessFree(Meta *meta);
  void ProcessThreadExit(thread_id_t curr_thd_id);
  void ProcessSignal(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessBroadcast(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreWait(thread_id_t curr_thd_id, CondMeta *meta, bool timedwait);
  void ProcessPostWait(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);
  void ProcessPostBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);

  int getNumAcc(AccSum*);

  std::pair<int, int>* searchForAccSumPair(AccSum* src, AccSum* dst);

  // common databases
  Mutex *internal_lock_;
  StaticInfo *sinfo_;
  iRootDB *iroot_db_;
  Memo *memo_;
  sinst::SharedInstDB *sinst_db_;

  // settings and flags
  bool sync_only_;
  bool complex_idioms_;
  bool single_var_idioms_;
  bool racy_only_;
  bool predict_deadlock_;
  address_t unit_size_;
  timestamp_t vw_;

  // meta data
  CondMeta::Table cond_meta_table_;
  BarrierMeta::Table barrier_meta_table_;
  SharedMeta::Table shared_meta_table_;
  Meta::Table meta_table_;

  // global analysis state
  RegionFilter *filter_;
  bool conflict_table_[IROOT_EVENT_TYPE_ARRAYSIZE][IROOT_EVENT_TYPE_ARRAYSIZE];
  std::map<thread_id_t, VectorClock *> curr_vc_map_;
  std::map<thread_id_t, LockSet *> curr_ls_map_;
  std::map<thread_id_t, timestamp_t> async_start_time_map_;
  std::set<Inst *> atomic_inst_set_;

  // access summary related
  AccSum::HashIndex acc_sum_hash_index_;
  AccSum::PairIndex acc_sum_succ_index_;
  AccSum::PairIndex acc_sum_pred_index_;

  // complex idioms related
  RecentInfo::Table ri_table_;
  LocalPair::Table lp_table_;
  LocalPair::PairIndex lp_pair_index_;
  LocalPair::Table dl_table_;
  LocalPair::PairIndex dl_pair_index_;
};

} // namespace idiom

#endif

