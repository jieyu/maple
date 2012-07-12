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

// File: sinst/analyzer.h - Define the analyzer for finding shared
// instructions.

#ifndef SINST_ANALYZER_H_
#define SINST_ANALYZER_H_

#include <set>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/analyzer.h"
#include "core/sync.h"
#include "core/filter.h"
#include "sinst/sinst.h"

namespace sinst {

// Analyzer for finding shared instructions.
class SharedInstAnalyzer : public Analyzer {
 public:
  SharedInstAnalyzer();
  ~SharedInstAnalyzer();

  void Register();
  bool Enabled();
  void Setup(Mutex *lock, SharedInstDB *sinst_db);
  void ImageLoad(Image *image, address_t low_addr, address_t high_addr,
                 address_t data_start, size_t data_size, address_t bss_start,
                 size_t bss_size);
  void ImageUnload(Image *image, address_t low_addr, address_t high_addr,
                   address_t data_start, size_t data_size, address_t bss_start,
                   size_t bss_size);
  void BeforeMemRead(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                     Inst *inst, address_t addr, size_t size);
  void BeforeMemWrite(thread_id_t curr_thd_id, timestamp_t curr_thd_clk,
                      Inst *inst, address_t addr, size_t size);
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
  class Meta {
   public:
    typedef std::set<Inst *> InstSet;
    typedef std::tr1::unordered_map<address_t, Meta> Table;

    Meta()
        : shared(false),
          has_write(false),
          multi_read(false),
          last_thd_id(INVALID_THD_ID) {}

    ~Meta() {}

    bool shared;
    bool has_write;
    bool multi_read;
    thread_id_t last_thd_id;
    InstSet inst_set;
  };

  void AllocAddrRegion(address_t addr, size_t size);
  void FreeAddrRegion(address_t addr);
  bool FilterAccess(address_t addr) { return filter_->Filter(addr, false); }

  Mutex *internal_lock_;
  SharedInstDB *sinst_db_;
  address_t unit_size_;
  RegionFilter *filter_;
  Meta::Table meta_table_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(SharedInstAnalyzer);
};

} // namespace sinst

#endif

