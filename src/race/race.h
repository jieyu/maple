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

// File: race/race.h - Define the representation of each race and the
// race database.

#ifndef RACE_RACE_H_
#define RACE_RACE_H_

#include <vector>
#include <tr1/unordered_set>

#include "core/basictypes.h"
#include "core/static_info.h"
#include "race/race.pb.h"

namespace race {

// forward declarations
class RaceDB;

// represents a static race event
class StaticRaceEvent {
 public:
  typedef uint32 id_t;
  typedef std::vector<StaticRaceEvent *> Vec;
  typedef std::tr1::unordered_map<id_t, StaticRaceEvent *> Map;
  typedef std::tr1::unordered_map<size_t, Vec> HashIndex;

  size_t Hash();
  bool Match(StaticRaceEvent *e);

  id_t id() { return id_; }
  Inst *inst() { return inst_; }
  RaceEventType type() { return type_; }

 protected:
  StaticRaceEvent() : id_(0), inst_(NULL), type_(RACE_EVENT_INVALID) {}
  ~StaticRaceEvent() {}

  id_t id_;
  Inst *inst_;
  RaceEventType type_;

 private:
  friend class RaceDB;

  DISALLOW_COPY_CONSTRUCTORS(StaticRaceEvent);
};

// represents a static race
class StaticRace {
 public:
  typedef uint32 id_t;
  typedef std::vector<StaticRace *> Vec;
  typedef std::tr1::unordered_map<id_t, StaticRace *> Map;
  typedef std::tr1::unordered_map<size_t, Vec> HashIndex;

  size_t Hash();
  bool Match(StaticRace *r);

  id_t id() { return id_; }

 protected:
  StaticRace() : id_(0) {}
  ~StaticRace() {}

  id_t id_;
  StaticRaceEvent::Vec event_vec_;

 private:
  friend class RaceDB;

  DISALLOW_COPY_CONSTRUCTORS(StaticRace);
};

// represents a dynamic race event
class RaceEvent {
 public:
  typedef std::vector<RaceEvent *> Vec;

  thread_id_t thd_id() { return thd_id_; }
  Inst *inst() { return static_event_->inst(); }
  RaceEventType type() { return static_event_->type(); }

 protected:
  RaceEvent() : thd_id_(INVALID_THD_ID), static_event_(NULL) {}
  ~RaceEvent() {}

  thread_id_t thd_id_;
  StaticRaceEvent *static_event_;

 private:
  friend class RaceDB;

  DISALLOW_COPY_CONSTRUCTORS(RaceEvent);
};

// represents a dynamic race
class Race {
 public:
  typedef std::vector<Race *> Vec;

  int exec_id() { return exec_id_; }
  address_t addr() { return addr_; }

 protected:
  Race() : exec_id_(-1), addr_(INVALID_ADDRESS), static_race_(NULL) {}
  ~Race() {}

  int exec_id_;
  address_t addr_;
  RaceEvent::Vec event_vec_;
  StaticRace *static_race_;

 private:
  friend class RaceDB;

  DISALLOW_COPY_CONSTRUCTORS(Race);
};

// the race database
class RaceDB {
 public:
  explicit RaceDB(Mutex *lock);
  ~RaceDB();

  Race *CreateRace(address_t addr, thread_id_t t0, Inst *i0, RaceEventType p0,
                   thread_id_t t1, Inst *i1, RaceEventType p1, bool locking);
  void SetRacyInst(Inst *inst, bool locking);
  bool RacyInst(Inst *inst, bool locking);
  void Load(const std::string &db_name, StaticInfo *sinfo);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 protected:
  typedef std::tr1::unordered_set<Inst *> RacyInstSet;

  StaticRaceEvent *CreateStaticRaceEvent(Inst *inst,
                                         RaceEventType type,
                                         bool locking);
  StaticRaceEvent *FindStaticRaceEvent(Inst *inst,
                                       RaceEventType type,
                                       bool locking);
  StaticRaceEvent *FindStaticRaceEvent(StaticRaceEvent::id_t id,
                                       bool locking);
  StaticRaceEvent *GetStaticRaceEvent(Inst *inst,
                                      RaceEventType type,
                                      bool locking);
  StaticRace *CreateStaticRace(StaticRaceEvent *e0,
                               StaticRaceEvent *e1,
                               bool locking);
  StaticRace *FindStaticRace(StaticRaceEvent *e0,
                             StaticRaceEvent *e1,
                             bool locking);
  StaticRace *FindStaticRace(StaticRace::id_t id,
                             bool locking);
  StaticRace *GetStaticRace(StaticRaceEvent *e0,
                            StaticRaceEvent *e1,
                            bool locking);

  Mutex *internal_lock_;
  StaticRaceEvent::id_t curr_static_event_id_;
  StaticRace::id_t curr_static_race_id_;
  int curr_exec_id_;
  StaticRaceEvent::Map static_event_table_;
  StaticRaceEvent::HashIndex static_event_index_;
  StaticRace::Map static_race_table_;
  StaticRace::HashIndex static_race_index_;
  Race::Vec race_vec_;
  RacyInstSet racy_inst_set_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(RaceDB);
};

} // namespace race

#endif

