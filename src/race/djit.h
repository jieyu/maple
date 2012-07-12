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

// File: race/djit.h - Define the data race detector using the Djit
// algorithm.

#ifndef RACE_DJIT_H_
#define RACE_DJIT_H_

#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/vector_clock.h"
#include "core/filter.h"
#include "race/detector.h"
#include "race/race.h"

namespace race {

class Djit : public Detector {
 public:
  Djit();
  ~Djit();

  void Register();
  bool Enabled();
  void Setup(Mutex *lock, RaceDB *race_db);

 protected:
  // the meta data for the memory access
  class DjitMeta : public Meta {
   public:
    typedef std::map<thread_id_t, Inst *> InstMap;
    typedef std::set<Inst *> InstSet;

    explicit DjitMeta(address_t a) : Meta(a), racy(false) {}
    ~DjitMeta() {}

    bool racy; // whether this meta is involved in any race
    VectorClock writer_vc;
    InstMap writer_inst_table;
    VectorClock reader_vc;
    InstMap reader_inst_table;
    InstSet race_inst_set;
  };

  // overrided virtual functions
  Meta *GetMeta(address_t iaddr);
  void ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst);
  void ProcessFree(Meta *meta);

  // settings and flasg
  bool track_racy_inst_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Djit);
};

} // namespace race

#endif

