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

// File: core/filter.h - Define address filters.

#ifndef CORE_FILTER_H_
#define CORE_FILTER_H_

#include <map>

#include "core/basictypes.h"
#include "core/sync.h"

class RegionFilter {
 public:
  explicit RegionFilter(Mutex *lock) : internal_lock_(lock) {}
  ~RegionFilter() { delete internal_lock_; }

  void AddRegion(address_t addr, size_t size) { AddRegion(addr, size, true); }
  size_t RemoveRegion(address_t addr) { return RemoveRegion(addr, true); }
  bool Filter(address_t addr) { return Filter(addr, true); }

  void AddRegion(address_t addr, size_t size, bool locking);
  size_t RemoveRegion(address_t addr, bool locking);
  bool Filter(address_t addr, bool locking);

 private:
  Mutex *internal_lock_;
  std::map<address_t, size_t> addr_region_map_;

  DISALLOW_COPY_CONSTRUCTORS(RegionFilter);
};

#endif

