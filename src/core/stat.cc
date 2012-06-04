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

// File: core/stat.cc - Implementation of statistics utilities.

#include "core/stat.h"

#include <fstream>
#include <iomanip>
#include <algorithm>

void Stat::Inc(std::string var, Stat::Int i, bool locking) {
  ScopedLock locker(internal_lock_, locking);
  IntTable::iterator it = int_table_.find(var);
  if (it == int_table_.end())
    int_table_[var] = i;
  else
    it->second += i;
}

void Stat::Max(std::string var, Stat::Int i, bool locking) {
  ScopedLock locker(internal_lock_, locking);
  IntTable::iterator it = int_table_.find(var);
  if (it == int_table_.end())
    int_table_[var] = i;
  else
    it->second = MAX(it->second, i);
}

void Stat::Min(std::string var, Stat::Int i, bool locking) {
  ScopedLock locker(internal_lock_, locking);
  IntTable::iterator it = int_table_.find(var);
  if (it == int_table_.end())
    int_table_[var] = i;
  else
    it->second = MIN(it->second, i);
}

void Stat::Rec(std::string var, Stat::Int i, bool locking) {
  ScopedLock locker(internal_lock_, locking);
  int_vec_table_[var].push_back(i);
}

void Stat::Display(const std::string &fname) {
  std::fstream out(fname.c_str(), std::ios::out | std::ios::trunc);
  // display title
  out << std::left;
  out << "Statistics" << std::endl;
  out << "---------------------------" << std::endl;
  // display int table
  for (IntTable::iterator tit = int_table_.begin();
       tit != int_table_.end(); ++tit) {
    out << std::setw(20) << tit->first;
    out << tit->second << std::endl;
  }
  // display int vec table
  for (IntVecTable::iterator tit = int_vec_table_.begin();
       tit != int_vec_table_.end(); ++tit) {
    IntVec &vec = tit->second;
    sort(vec.begin(), vec.end());
    out << std::setw(20) << tit->first;
    out << vec.size() << std::endl;
    size_t detail_level = 10;
    for (size_t i = 0; i < detail_level; i++) {
      double ratio = (double)(i + 1) / (double)detail_level;
      size_t idx = (size_t)((double)(vec.size() - 1) * ratio);
      out << "  " << std::setw(18) << idx;
      out << vec[idx] << std::endl;
    }
  }
  out.close();
}

// global variables and definitions
Stat *g_stat = NULL;

void stat_init(Mutex *lock) {
  g_stat = new Stat(lock);
}

void stat_display(const std::string &fname) {
  g_stat->Display(fname);
}

