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

// File: core/vector_clock.h - Define vector clock.

#ifndef CORE_VECTOR_CLOCK_H_
#define CORE_VECTOR_CLOCK_H_

#include <map>

#include "core/basictypes.h"

// Vector clock.
class VectorClock {
 public:
  VectorClock() {}
  ~VectorClock() {}

  bool HappensBefore(VectorClock *vc);
  bool HappensAfter(VectorClock *vc);
  void Join(VectorClock *vc);
  void Increment(thread_id_t thd_id);
  timestamp_t GetClock(thread_id_t thd_id);
  void SetClock(thread_id_t thd_id, timestamp_t clk);
  bool Equal(VectorClock *vc);
  std::string ToString();
  void IterBegin() { it_ = map_.begin(); }
  bool IterEnd() { return it_ == map_.end(); }
  void IterNext() { ++it_; }
  thread_id_t IterCurrThd() { return it_->first; }
  timestamp_t IterCurrClk() { return it_->second; }

 private:
  typedef std::map<thread_id_t, timestamp_t> ThreadClockMap;

  ThreadClockMap map_;
  ThreadClockMap::iterator it_;

  // using default copy constructor and assignment operator
};

#endif

