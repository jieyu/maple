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

// File: race/detector.h - Define the abstract data race dectector.

#ifndef RACE_DETECTOR_H_
#define RACE_DETECTOR_H_

#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/vector_clock.h"
#include "core/filter.h"
#include "race/race.h"

namespace race {

class Detector : public Analyzer {
 public:
  Detector();
  virtual ~Detector();

  virtual void Register();
  virtual bool Enabled() = 0;
  virtual void Setup(Mutex *lock, RaceDB *race_db);
  virtual void ImageLoad(Image *image,
                         address_t low_addr, address_t high_addr,
                         address_t data_start, size_t data_size,
                         address_t bss_start, size_t bss_size);
  virtual void ImageUnload(Image *image,
                           address_t low_addr, address_t high_addr,
                           address_t data_start, size_t data_size,
                           address_t bss_start, size_t bss_size);
  virtual void ThreadStart(thread_id_t curr_thd_id, thread_id_t parent_thd_id);
  virtual void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t addr, size_t size);
  virtual void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                              Inst *inst, address_t addr, size_t size);
  virtual void BeforeAtomicInst(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                std::string type, address_t addr);
  virtual void AfterAtomicInst(thread_id_t curr_thd_id,
                               timestamp_t curr_thd_clk, Inst *inst,
                               std::string type, address_t addr);
  virtual void AfterPthreadJoin(thread_id_t curr_thd_id,
                                timestamp_t curr_thd_clk, Inst *inst,
                                thread_id_t child_thd_id);
  virtual void AfterPthreadMutexLock(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk,
                                     Inst *inst, address_t addr);
  virtual void BeforePthreadMutexUnlock(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr);
  virtual void BeforePthreadCondSignal(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr);
  virtual void BeforePthreadCondBroadcast(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk,
                                          Inst *inst, address_t addr);
  virtual void BeforePthreadCondWait(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     address_t cond_addr, address_t mutex_addr);
  virtual void AfterPthreadCondWait(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk, Inst *inst,
                                    address_t cond_addr, address_t mutex_addr);
  virtual void BeforePthreadCondTimedwait(thread_id_t curr_thd_id,
                                          timestamp_t curr_thd_clk, Inst *inst,
                                          address_t cond_addr,
                                          address_t mutex_addr);
  virtual void AfterPthreadCondTimedwait(thread_id_t curr_thd_id,
                                         timestamp_t curr_thd_clk, Inst *inst,
                                         address_t cond_addr,
                                         address_t mutex_addr);
  virtual void BeforePthreadBarrierWait(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr);
  virtual void AfterPthreadBarrierWait(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr);
  virtual void AfterMalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr);
  virtual void AfterCalloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t nmemb, size_t size,
                           address_t addr);
  virtual void BeforeRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                             Inst *inst, address_t ori_addr, size_t size);
  virtual void AfterRealloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                            Inst *inst, address_t ori_addr, size_t size,
                            address_t new_addr);
  virtual void BeforeFree(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                          Inst *inst, address_t addr);
  virtual void AfterValloc(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                           Inst *inst, size_t size, address_t addr);

 protected:
  // the abstract meta data for the memory access
  class Meta {
   public:
    typedef std::tr1::unordered_map<address_t, Meta *> Table;

    explicit Meta(address_t a) : addr(a) {}
    virtual ~Meta() {}

    address_t addr;
  };

  // the meta data for mutex variables to track vector clock
  class MutexMeta {
   public:
    typedef std::tr1::unordered_map<address_t, MutexMeta *> Table;

    MutexMeta() {}
    ~MutexMeta() {}

    VectorClock vc;
  };

  // the meta data for conditional variables to track vector clock
  class CondMeta {
   public:
    typedef std::map<thread_id_t, VectorClock> VectorClockMap;
    typedef std::tr1::unordered_map<address_t, CondMeta *> Table;

    CondMeta() {}
    ~CondMeta() {}

    VectorClockMap wait_table;
    VectorClockMap signal_table;
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

  // helper functions
  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr) { return filter_->Filter(addr, false); }
  MutexMeta *GetMutexMeta(address_t iaddr);
  CondMeta *GetCondMeta(address_t iaddr);
  BarrierMeta *GetBarrierMeta(address_t iaddr);
  void ReportRace(Meta *meta, thread_id_t t0, Inst *i0, RaceEventType p0,
                  thread_id_t t1, Inst *i1, RaceEventType p1);

  // main processing functions
  void ProcessLock(thread_id_t curr_thd_id, MutexMeta *meta);
  void ProcessUnlock(thread_id_t curr_thd_id, MutexMeta *meta);
  void ProcessNotify(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreWait(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPostWait(thread_id_t curr_thd_id, CondMeta *meta);
  void ProcessPreBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);
  void ProcessPostBarrier(thread_id_t curr_thd_id, BarrierMeta *meta);
  void ProcessFree(MutexMeta *meta);
  void ProcessFree(CondMeta *meta);
  void ProcessFree(BarrierMeta *meta);

  // virtual functions to override
  virtual Meta *GetMeta(address_t iaddr) = 0;
  virtual void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) = 0;
  virtual void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst)= 0;
  virtual void ProcessFree(Meta *meta) = 0;

  // common databases
  Mutex *internal_lock_;
  RaceDB *race_db_;

  // settings and flasg
  address_t unit_size_;
  RegionFilter *filter_;

  // meta data
  MutexMeta::Table mutex_meta_table_;
  CondMeta::Table cond_meta_table_;
  BarrierMeta::Table barrier_meta_table_;
  Meta::Table meta_table_;

  // global analysis state
  std::map<thread_id_t, VectorClock *> curr_vc_map_;
  std::map<thread_id_t, bool> atomic_map_; // whether executing atomic inst.

 private:
  DISALLOW_COPY_CONSTRUCTORS(Detector);
};

} // namespace race

#endif

