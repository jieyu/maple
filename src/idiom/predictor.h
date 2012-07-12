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

// File: idiom/predictor.h - Define iRoot predictor analyzer.

#ifndef IDIOM_PREDICTOR_H_
#define IDIOM_PREDICTOR_H_

#include <list>
#include <vector>
#include <map>
#include <tr1/unordered_map>
#include <tr1/unordered_set>

#include "core/basictypes.h"
#include "core/sync.h"
#include "core/knob.h"
#include "core/static_info.h"
#include "core/analyzer.h"
#include "core/vector_clock.h"
#include "core/lock_set.h"
#include "core/filter.h"
#include "idiom/iroot.h"
#include "idiom/memo.h"
#include "sinst/sinst.h"

namespace idiom {

class Predictor;

// Abstract access.
class PredictorAccess {
 public:
  PredictorAccess(timestamp_t clk, iRootEventType type, Inst *inst)
      : clk_(clk), type_(type), inst_(inst) {}
  virtual ~PredictorAccess() {}

  bool IsRead() { return type_ == IROOT_EVENT_MEM_READ; }
  bool IsWrite() { return type_ == IROOT_EVENT_MEM_WRITE; }

  bool IsSync() {
    return type_ == IROOT_EVENT_MUTEX_LOCK ||
           type_ == IROOT_EVENT_MUTEX_UNLOCK;
  }

  bool IsMem() {
    return type_ == IROOT_EVENT_MEM_READ ||
           type_ == IROOT_EVENT_MEM_WRITE;
  }

 protected:
  timestamp_t clk_;
  iRootEventType type_;
  Inst *inst_;

 private:
  friend class Predictor;

  // using default copy constructor and assignment operator
};

// Memory access.
class PredictorMemAccess : public PredictorAccess {
 public:
  PredictorMemAccess(timestamp_t clk, iRootEventType type, Inst *inst,
                     LockSet *ls)
      : PredictorAccess(clk, type, inst), ls_(*ls) {}
  ~PredictorMemAccess() {}

 private:
  LockSet ls_;

  friend class Predictor;

  // using default copy constructor and assignment operator
};

// Mutex access.
class PredictorMutexAccess : public PredictorAccess {
 public:
  PredictorMutexAccess(timestamp_t clk, iRootEventType type, Inst *inst,
                       LockSet *ls)
      : PredictorAccess(clk, type, inst), ls_(*ls) {}
  ~PredictorMutexAccess() {}

  bool IsLock() { return type_ == IROOT_EVENT_MUTEX_LOCK; }
  bool IsUnlock() { return type_ == IROOT_EVENT_MUTEX_UNLOCK; }

 private:
  LockSet ls_;

  friend class Predictor;

  // using default copy constructor and assignment operator
};

// Abstract meta data for each address.
class PredictorMeta {
 public:
  explicit PredictorMeta(address_t addr) : addr_(addr) {}
  virtual ~PredictorMeta() {}

 protected:
  address_t addr_;

 private:
  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorMeta);
};

// Meta data for memory access locations.
class PredictorMemMeta : public PredictorMeta {
 public:
  explicit PredictorMemMeta(address_t addr)
      : PredictorMeta(addr),
        shared_(false),
        last_access_thd_id_(INVALID_THD_ID),
        history_(NULL) {}
  ~PredictorMemMeta() {}

 private:
  typedef std::vector<PredictorMemAccess> AccessVec;
  typedef std::pair<VectorClock, AccessVec> TimedAccessVec;
  typedef std::list<TimedAccessVec> PerThreadAccesses;
  typedef std::map<thread_id_t, PerThreadAccesses> AccessMap;
  typedef struct {
    AccessMap access_map;
    std::map<thread_id_t, size_t> last_gc_vec_size;
  } AccessHistory;

  bool shared_;
  thread_id_t last_access_thd_id_;
  AccessHistory *history_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorMemMeta);
};

// Meta data for lock variables.
class PredictorMutexMeta : public PredictorMeta {
 public:
  explicit PredictorMutexMeta(address_t addr) : PredictorMeta(addr) {}
  ~PredictorMutexMeta() {}

 private:
  typedef std::vector<PredictorMutexAccess> AccessVec;
  typedef std::pair<VectorClock, AccessVec> TimedAccessVec;
  typedef std::list<TimedAccessVec> PerThreadAccesses;
  typedef std::map<thread_id_t, PerThreadAccesses> AccessMap;
  typedef struct {
    AccessMap access_map;
  } AccessHistory;

  AccessHistory history_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorMutexMeta);
};

// Meta data for conditional variables.
class PredictorCondMeta : public PredictorMeta {
 public:
  explicit PredictorCondMeta(address_t addr) : PredictorMeta(addr) {}
  ~PredictorCondMeta() {}

 private:
  typedef std::map<thread_id_t, VectorClock> VectorClockMap;

  // data
  VectorClockMap wait_table_;
  VectorClockMap signal_table_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorCondMeta);
};

// Meta data for barrier variables.
class PredictorBarrierMeta : public PredictorMeta {
 public:
  explicit PredictorBarrierMeta(address_t addr)
      : PredictorMeta(addr),
        pre_using_table1_(true),
        post_using_table1_(true) {}
  ~PredictorBarrierMeta() {}

 private:
  typedef std::map<thread_id_t, std::pair<VectorClock, bool> > VectorClockMap;

  // data
  bool pre_using_table1_;
  bool post_using_table1_;
  VectorClockMap barrier_wait_table1_;
  VectorClockMap barrier_wait_table2_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorBarrierMeta);
};

// Local information.
class PredictorLocalInfo {
 public:
  PredictorLocalInfo() {}
  ~PredictorLocalInfo() {}

 private:
  typedef struct {
    timestamp_t clk;
    address_t addr;
    iRootEventType type;
    Inst *inst;
  } EntryType;

  typedef struct {
    iRootEventType curr_type;
    Inst *curr_inst;
    iRootEventType prev_type;
    Inst *prev_inst;
    bool same_addr;
    thread_id_t thd_id;
  } PairType;

  typedef struct {
    thread_id_t thd_id;
    iRootEventType type;
    Inst *inst;
  } DynEvent;

  typedef struct {
    timestamp_t start;
    timestamp_t end;
  } DynRange;

  struct PairTypeHash {
    size_t operator()(const PairType &p) const {
      return (size_t)p.curr_type + (size_t)p.curr_inst +
             (size_t)p.prev_type + (size_t)p.prev_inst +
             (size_t)p.same_addr + (size_t)p.thd_id;
    }
  };

  struct PairTypeEqual {
    bool operator()(const PairType &p1, const PairType &p2) const {
      return p1.curr_type == p2.curr_type &&
             p1.curr_inst == p2.curr_inst &&
             p1.prev_type == p2.prev_type &&
             p1.prev_inst == p2.prev_inst &&
             p1.same_addr == p2.same_addr &&
             p1.thd_id == p2.thd_id;
    }
  };

  struct DynEventHash {
    size_t operator()(const DynEvent &e) const {
      return (size_t)e.thd_id + (size_t)e.type + (size_t)e.inst;
    }
  };

  struct DynEventEqual {
    bool operator()(const DynEvent &e1, const DynEvent &e2) const {
      return e1.thd_id == e2.thd_id &&
             e1.type == e2.type &&
             e1.inst == e2.inst;
    }
  };

  typedef std::list<EntryType> EntryList;
  typedef std::map<thread_id_t, EntryList> EntryMap;
  typedef std::tr1::unordered_set<PairType,
                                  PairTypeHash,
                                  PairTypeEqual> PairSet;
  typedef std::tr1::unordered_map<DynEvent,
                                  DynRange,
                                  DynEventHash,
                                  DynEventEqual> DynEventRangeMap;
  typedef std::tr1::unordered_map<DynEvent,
                                  DynEventRangeMap,
                                  DynEventHash,
                                  DynEventEqual> DynEventMap;

  EntryMap access_map_;
  PairSet pair_db_;
  DynEventMap dyn_event_map_;
  DynEventMap r_dyn_event_map_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorLocalInfo);
};

class PredictorDeadlockInfo {
 public:
  PredictorDeadlockInfo() {}
  ~PredictorDeadlockInfo() {}

 private:
  typedef struct {
    iRootEventType curr_type;
    Inst *curr_inst;
    address_t curr_addr;
    iRootEventType prev_type;
    Inst *prev_inst;
    address_t prev_addr;
    thread_id_t thd_id;
  } PairType;

  struct PairTypeHash {
    size_t operator()(const PairType &p) const {
      return (size_t)p.curr_type + (size_t)p.curr_inst + (size_t)p.curr_addr +
             (size_t)p.prev_type + (size_t)p.prev_inst + (size_t)p.prev_addr +
             (size_t)p.thd_id;
    }
  };

  struct PairTypeEqual {
    bool operator()(const PairType &p1, const PairType &p2) const {
      return p1.curr_type == p2.curr_type &&
             p1.curr_inst == p2.curr_inst &&
             p1.curr_addr == p2.curr_addr &&
             p1.prev_type == p2.prev_type &&
             p1.prev_inst == p2.prev_inst &&
             p1.prev_addr == p2.prev_addr &&
             p1.thd_id == p2.thd_id;
    }
  };

  typedef std::tr1::unordered_set<PairType,PairTypeHash,PairTypeEqual> PairSet;

  PairSet pair_db_;

  friend class Predictor;

  DISALLOW_COPY_CONSTRUCTORS(PredictorDeadlockInfo);
};

// iRoot predictor which predicts iRoots to test.
class Predictor : public Analyzer {
 public:
  Predictor();
  ~Predictor();

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
  void SyscallExit(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
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
  void AfterPthreadCreate(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, thread_id_t child_thd_id);
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

 private:
  typedef std::tr1::unordered_map<address_t, PredictorMeta *> MetaMap;

  PredictorMemMeta *GetMemMeta(address_t iaddr);
  PredictorMutexMeta *GetMutexMeta(address_t iaddr);
  PredictorCondMeta *GetCondMeta(address_t iaddr);
  PredictorBarrierMeta *GetBarrierMeta(address_t iaddr);
  bool FilterAccess(address_t addr);
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool CheckLockSet(PredictorMemAccess *curr, PredictorMemAccess *curr_prev,
                    PredictorMemAccess *rmt, PredictorMemAccess *rmt_next);
  bool CheckLockSet(PredictorMutexAccess *curr, PredictorMutexAccess *curr_prev,
                    PredictorMutexAccess *rmt, PredictorMutexAccess *rmt_next);
  bool CheckRace(PredictorMemAccess *curr, PredictorMemAccess *rmt);
  bool CheckRace(PredictorMutexAccess *curr, PredictorMutexAccess *rmt);
  bool CheckAsync(thread_id_t thd_id);
  bool CheckAsync(thread_id_t thd_id, timestamp_t clk);
  bool ValidPair(iRootEventType prev_type, iRootEventType curr_type);
  void UpdateOnThreadExit(thread_id_t thd_id);
  void UpdateOnFree(PredictorMeta *meta);

  // for memory access meta
  bool CheckShared(thread_id_t curr_thd_id, Inst *inst, PredictorMemMeta *meta);
  void UpdateForRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, PredictorMemMeta *meta);
  void UpdateForWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, PredictorMemMeta *meta);
  void UpdateOnThreadExit(thread_id_t exit_thd_id, PredictorMemMeta *meta);
  void UpdateOnFree(PredictorMemMeta *meta);
  void FindSuccForRead(thread_id_t curr_thd_id,
                       PredictorMemAccess *curr_reader,
                       VectorClock *curr_reader_vc,
                       PredictorMemAccess *curr_next,
                       PredictorMemMeta *meta);
  void FindSuccForWrite(thread_id_t curr_thd_id,
                        PredictorMemAccess *curr_writer,
                        VectorClock *curr_writer_vc,
                        PredictorMemAccess *curr_next,
                        PredictorMemMeta *meta);
  void UpdateMemAccess(thread_id_t thd_id, VectorClock *vc,
                       PredictorMemAccess *access, PredictorMemMeta *meta);
  VectorClock *FindLastVC(thread_id_t thd_id, PredictorMemMeta *meta);
  PredictorMemAccess *FindLastAccess(thread_id_t thd_id,PredictorMemMeta *meta);
  bool CheckGC(PredictorMemMeta *meta);
  bool CheckCompress(thread_id_t thd_id,
                     PredictorMemMeta::AccessVec *access_vec,
                     PredictorMemMeta *meta);
  void GC(PredictorMemMeta *meta);
  void Compress(PredictorMemMeta::AccessVec *access_vec,
                PredictorMemMeta *meta);

  // for mutex meta
  void UpdateForLock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, PredictorMutexMeta *meta);
  void UpdateForUnlock(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                       Inst *inst, PredictorMutexMeta *meta);
  void UpdateOnThreadExit(thread_id_t exit_thd_id, PredictorMutexMeta *meta);
  void UpdateOnFree(PredictorMutexMeta *meta);
  void FindSuccForUnlock(thread_id_t curr_thd_id,
                         PredictorMutexAccess *curr_unlock,
                         VectorClock *curr_unlock_vc,
                         PredictorMutexAccess *curr_next,
                         PredictorMutexMeta *meta);
  void UpdateMutexAccess(thread_id_t thd_id, VectorClock *vc,
                         PredictorMutexAccess *access,
                         PredictorMutexMeta *meta);
  VectorClock *FindLastVC(thread_id_t thd_id, PredictorMutexMeta *meta);
  PredictorMutexAccess *FindLastAccess(thread_id_t thd_id,
                                       PredictorMutexMeta *meta);

  // for cond meta
  void UpdateBeforeWait(thread_id_t curr_thd_id, PredictorCondMeta *meta);
  void UpdateAfterWait(thread_id_t curr_thd_id, PredictorCondMeta *meta);
  void UpdateForNotify(thread_id_t curr_thd_id, PredictorCondMeta *meta);

  // for barrier meta
  void UpdateBeforeBarrier(thread_id_t curr_thd_id, PredictorBarrierMeta *meta);
  void UpdateAfterBarrier(thread_id_t curr_thd_id, PredictorBarrierMeta *meta);

  // for local info
  void UpdateMemo(thread_id_t src_thd_id, PredictorAccess &src_access,
                  thread_id_t dst_thd_id, PredictorAccess &dst_access);
  void UpdateDynEventMap(thread_id_t src_thd_id, PredictorAccess &src_access,
                         thread_id_t dst_thd_id, PredictorAccess &dst_access);
  void UpdateLocalInfo(thread_id_t curr_thd_id, PredictorAccess *curr_access,
                       PredictorMeta *meta);
  void UpdateLocalPairDB(PredictorLocalInfo::PairType *local_pair);
  void UpdateComplexiRoots();

  Mutex *internal_lock_;
  StaticInfo *sinfo_;
  iRootDB *iroot_db_;
  Memo *memo_;
  sinst::SharedInstDB *sinst_db_;
  bool sync_only_;
  address_t unit_size_;
  bool complex_idioms_;
  timestamp_t vw_; // vulnerability window
  bool racy_only_; // whether ignore non racy mem and mutex dependencies
  bool predict_deadlock_; // whether predict deadlock
  RegionFilter *filter_;
  std::map<thread_id_t, VectorClock *> curr_vc_map_;
  std::map<thread_id_t, LockSet *> curr_ls_map_;
  std::map<thread_id_t, VectorClock *> exit_vc_map_;
  std::map<thread_id_t, bool> monitored_thd_map_;
  std::map<thread_id_t, bool> async_map_;
  std::map<thread_id_t, timestamp_t> async_start_time_map_;
  std::map<address_t, size_t> addr_region_map_;
  MetaMap meta_map_;
  PredictorLocalInfo local_info_;
  PredictorDeadlockInfo deadlock_info_;

  DISALLOW_COPY_CONSTRUCTORS(Predictor);
};

} // namespace idiom

#endif

