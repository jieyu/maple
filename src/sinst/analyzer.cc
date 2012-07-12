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

// File: sinst/analyzer.cc - Implementation of the analyzer for finding
// shared instructions.

#include "sinst/analyzer.h"

#include "core/logging.h"

namespace sinst {

SharedInstAnalyzer::SharedInstAnalyzer()
    : internal_lock_(NULL),
      sinst_db_(NULL),
      unit_size_(4),
      filter_(NULL) {
  // do nothing
}

SharedInstAnalyzer::~SharedInstAnalyzer() {
  delete internal_lock_;
  delete filter_;
}

void SharedInstAnalyzer::Register() {
  knob_->RegisterBool("enable_sinst", "whether enable the shared inst analyzer", "0");
  knob_->RegisterInt("unit_size", "the monitoring granularity in bytes", "4");
}

bool SharedInstAnalyzer::Enabled() {
  return knob_->ValueBool("enable_sinst");
}

void SharedInstAnalyzer::Setup(Mutex *lock, SharedInstDB *sinst_db) {
  internal_lock_ = lock;
  sinst_db_ = sinst_db;
  unit_size_ = knob_->ValueInt("unit_size");
  filter_ = new RegionFilter(internal_lock_->Clone());
  // set analyzer descriptor
  desc_.SetHookBeforeMem();
  desc_.SetHookMallocFunc();
}

void SharedInstAnalyzer::ImageLoad(Image *image, address_t low_addr,
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

void SharedInstAnalyzer::ImageUnload(Image *image, address_t low_addr,
                                     address_t high_addr, address_t data_start,
                                     size_t data_size, address_t bss_start,
                                     size_t bss_size) {
  DEBUG_ASSERT(low_addr);
  if (data_start)
    FreeAddrRegion(data_start);
  if (bss_start)
    FreeAddrRegion(bss_start);
}

void SharedInstAnalyzer::BeforeMemRead(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize accesses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // check shared for iaddr
    Meta::Table::iterator mit = meta_table_.find(iaddr);
    if (mit == meta_table_.end()) {
      Meta &meta = meta_table_[iaddr];
      meta.last_thd_id = curr_thd_id;
      meta.inst_set.insert(inst);
    } else {
      // shared info exists
      Meta &meta = mit->second;
      if (meta.shared) {
        // meta is shared
        sinst_db_->SetShared(inst);
      } else {
        // meta is not currently shared
        meta.inst_set.insert(inst);
        if (curr_thd_id != meta.last_thd_id) {
          if (meta.has_write) {
            // mark as shared
            meta.shared = true;
            for (Meta::InstSet::iterator it = meta.inst_set.begin();
                 it != meta.inst_set.end(); ++it) {
              sinst_db_->SetShared(*it);
            }
            meta.inst_set.clear();
          } else {
            meta.multi_read = true;
            meta.last_thd_id = curr_thd_id;
          }
        }
      } // end of else meta.shared
    } // end of else meta not exist
  } // end of for each iaddr
}

void SharedInstAnalyzer::BeforeMemWrite(thread_id_t curr_thd_id,
                                        timestamp_t curr_thd_clk, Inst *inst,
                                        address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  if (FilterAccess(addr))
    return;
  // normalize accesses
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    // check shared for iaddr
    Meta::Table::iterator mit = meta_table_.find(iaddr);
    if (mit == meta_table_.end()) {
      Meta &meta = meta_table_[iaddr];
      meta.has_write = true;
      meta.last_thd_id = curr_thd_id;
      meta.inst_set.insert(inst);
    } else {
      // shared info exists
      Meta &meta = mit->second;
      if (meta.shared) {
        // meta is shared
        sinst_db_->SetShared(inst);
      } else {
        // meta is not currently shared
        meta.has_write = true;
        meta.inst_set.insert(inst);
        if (curr_thd_id != meta.last_thd_id || meta.multi_read) {
          // mark as shared
          meta.shared = true;
          for (Meta::InstSet::iterator it = meta.inst_set.begin();
               it != meta.inst_set.end(); ++it) {
            sinst_db_->SetShared(*it);
          }
          meta.inst_set.clear();
        }
      }
    }
  }
}

void SharedInstAnalyzer::AfterMalloc(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

void SharedInstAnalyzer::AfterCalloc(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     size_t nmemb, size_t size,
                                     address_t addr) {
  AllocAddrRegion(addr, size * nmemb);
}

void SharedInstAnalyzer::BeforeRealloc(thread_id_t curr_thd_id,
                                       timestamp_t curr_thd_clk, Inst *inst,
                                       address_t ori_addr, size_t size) {
  FreeAddrRegion(ori_addr);
}

void SharedInstAnalyzer::AfterRealloc(thread_id_t curr_thd_id,
                                      timestamp_t curr_thd_clk, Inst *inst,
                                      address_t ori_addr, size_t size,
                                      address_t new_addr) {
  AllocAddrRegion(new_addr, size);
}

void SharedInstAnalyzer::BeforeFree(thread_id_t curr_thd_id,
                                    timestamp_t curr_thd_clk, Inst *inst,
                                    address_t addr) {
  FreeAddrRegion(addr);
}

void SharedInstAnalyzer::AfterValloc(thread_id_t curr_thd_id,
                                     timestamp_t curr_thd_clk, Inst *inst,
                                     size_t size, address_t addr) {
  AllocAddrRegion(addr, size);
}

void SharedInstAnalyzer::AllocAddrRegion(address_t addr, size_t size) {
  ScopedLock locker(internal_lock_);
  DEBUG_ASSERT(addr && size);
  filter_->AddRegion(addr, size, false);
}

void SharedInstAnalyzer::FreeAddrRegion(address_t addr) {
  ScopedLock locker(internal_lock_);
  if (!addr) return;
  size_t size = filter_->RemoveRegion(addr, false);
  address_t start_addr = UNIT_DOWN_ALIGN(addr, unit_size_);
  address_t end_addr = UNIT_UP_ALIGN(addr + size, unit_size_);
  for (address_t iaddr = start_addr; iaddr < end_addr; iaddr += unit_size_) {
    Meta::Table::iterator it = meta_table_.find(iaddr);
    if (it != meta_table_.end()) {
      meta_table_.erase(it);
    }
  }
}

} // namespace sinst

