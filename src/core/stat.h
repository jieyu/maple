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

// File: core/stat.h - Define statistics utilities.

#ifndef CORE_STAT_H_
#define CORE_STAT_H_

#include <map>
#include <vector>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/sync.h"

#ifndef MAX
#define MAX(a, b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a)<(b)) ? (a) : (b))
#endif

// The class for statistics
class Stat {
 public:
  typedef uint64 Int;
  typedef double Float;

  Stat(Mutex *lock) : internal_lock_(lock) {}
  ~Stat() {}

  void Inc(std::string var, Int i, bool locking);
  void Max(std::string var, Int i, bool locking);
  void Min(std::string var, Int i, bool locking);
  void Rec(std::string var, Int i, bool locking);
  void Display(const std::string &fname);

 protected:
  typedef std::vector<Int> IntVec;
  typedef std::tr1::unordered_map<std::string, Int> IntTable;
  typedef std::tr1::unordered_map<std::string, IntVec> IntVecTable;

  Mutex *internal_lock_;
  IntTable int_table_;
  IntVecTable int_vec_table_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Stat);
};

// Global definitions
extern Stat *g_stat;
extern void stat_init(Mutex *lock);
extern void stat_display(const std::string &fname);

#define STAT_INC(var,i) do { g_stat->Inc(var, i, false); } while (0)
#define STAT_INC_SAFE(var,i) do { g_stat->Inc(var, i, true); } while (0)
#define STAT_MAX(var,i) do { g_stat->Max(var, i, false); } while (0)
#define STAT_MAX_SAFE(var,i) do { g_stat->Inc(var, i, true); } while (0)
#define STAT_MIN(var,i) do { g_stat->Min(var, i, false); } while (0)
#define STAT_MIN_SAFE(var,i) do { g_stat->Min(var, i, true); } while (0)
#define STAT_REC(var,i) do { g_stat->Rec(var, i, false); } while (0)
#define STAT_REC_SAFE(var,i) do ( g_stat->Rec(var, i, true); } while (0)

#ifdef _DEBUG
#define DEBUG_STAT_INC(var,i) STAT_INC(var, i)
#define DEBUG_STAT_INC_SAFE(var,i) STAT_INC_SAFE(var, i)
#define DEBUG_STAT_MAX(var,i) STAT_MAX(var, i)
#define DEBUG_STAT_MAX_SAFE(var,i) STAT_MAX_SAFE(var, i)
#define DEBUG_STAT_MIN(var,i) STAT_MIN(var, i)
#define DEBUG_STAT_MIN_SAFE(var,i) STAT_MIN_SAFE(var, i)
#define DEBUG_STAT_REC(var,i) STAT_REC(var, i)
#define DEBUG_STAT_REC_SAFE(var,i) STAT_REC_SAFE(var, i)
#else
#define DEBUG_STAT_INC(var,i) do {} while (0)
#define DEBUG_STAT_INC_SAFE(var,i) do {} while (0)
#define DEBUG_STAT_MAX(var,i) do {} while (0)
#define DEBUG_STAT_MAX_SAFE(var,i) do {} while (0)
#define DEBUG_STAT_MIN(var,i) do {} while (0)
#define DEBUG_STAT_MIN_SAFE(var,i) do {} while (0)
#define DEBUG_STAT_REC(var,i) do {} while (0)
#define DEBUG_STAT_REC_SAFE(var,i) do {} while (0)
#endif

#endif

