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

// File: race/race.cc - Implementation of the race and race database.

#include "race/race.h"

#include "core/logging.h"

namespace race {

size_t StaticRaceEvent::Hash() {
  return (size_t)inst_ + (size_t)type_;
}

bool StaticRaceEvent::Match(StaticRaceEvent *e) {
  return inst_ == e->inst_ && type_ == e->type_;
}

size_t StaticRace::Hash() {
  size_t hash_val = 0;
  for (size_t i = 0; i < event_vec_.size(); i++)
    hash_val += (size_t)event_vec_[i];
  return hash_val;
}

bool StaticRace::Match(StaticRace *r) {
  if (event_vec_.size() != r->event_vec_.size())
    return false;
  for (size_t i = 0; i < event_vec_.size(); i++) {
    if (event_vec_[i] != r->event_vec_[i])
      return false;
  }
  return true;
}

RaceDB::RaceDB(Mutex *lock)
    : internal_lock_(lock),
      curr_static_event_id_(0),
      curr_static_race_id_(0),
      curr_exec_id_(0) {
  // empty
}

RaceDB::~RaceDB() {
  delete internal_lock_;
}

Race *RaceDB::CreateRace(address_t addr, thread_id_t t0, Inst *i0,
                         RaceEventType p0, thread_id_t t1, Inst *i1,
                         RaceEventType p1, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  Race *race = new Race;
  race->exec_id_ = curr_exec_id_;
  race->addr_ = addr;
  // create race events
  RaceEvent *e0 = new RaceEvent;
  RaceEvent *e1 = new RaceEvent;
  e0->thd_id_ = t0;
  e1->thd_id_ = t1;
  e0->static_event_ = GetStaticRaceEvent(i0, p0, false);
  e1->static_event_ = GetStaticRaceEvent(i1, p1, false);
  race->event_vec_.push_back(e0);
  race->event_vec_.push_back(e1);
  // get static race
  race->static_race_ = GetStaticRace(e0->static_event_,
                                     e1->static_event_,
                                     false);
  // put self into race vector
  race_vec_.push_back(race);
  return race;
}

void RaceDB::SetRacyInst(Inst *inst, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  racy_inst_set_.insert(inst);
}

bool RaceDB::RacyInst(Inst *inst, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  return racy_inst_set_.find(inst) != racy_inst_set_.end();
}

void RaceDB::Load(const std::string &db_name, StaticInfo *sinfo) {
  RaceDBProto proto;
  // load from file
  std::fstream in(db_name.c_str(), std::ios::in | std::ios::binary);
  if (in.is_open())
    proto.ParseFromIstream(&in);
  in.close();
  // load static events
  for (int i = 0; i < proto.static_event_size(); i++) {
    StaticRaceEventProto *e_proto = proto.mutable_static_event(i);
    StaticRaceEvent *e = new StaticRaceEvent;
    e->id_ = e_proto->id();
    e->inst_ = sinfo->FindInst(e_proto->inst_id());
    DEBUG_ASSERT(e->inst_);
    e->type_ = e_proto->type();
    static_event_table_[e->id_] = e;
    static_event_index_[e->Hash()].push_back(e);
    if (curr_static_event_id_ < e->id_)
      curr_static_event_id_ = e->id_;
  }
  // load static races
  for (int i = 0; i < proto.static_race_size(); i++) {
    StaticRaceProto *r_proto = proto.mutable_static_race(i);
    StaticRace *r = new StaticRace;
    r->id_ = r_proto->id();
    for (int j = 0; j < r_proto->event_id_size(); j++) {
      StaticRaceEvent *e = FindStaticRaceEvent(r_proto->event_id(j), false);
      DEBUG_ASSERT(e);
      r->event_vec_.push_back(e);
    }
    static_race_table_[r->id_] = r;
    static_race_index_[r->Hash()].push_back(r);
    if (curr_static_race_id_ < r->id_)
      curr_static_race_id_ = r->id_;
  }
  // load races
  for (int i = 0; i < proto.race_size(); i++) {
    RaceProto *r_proto = proto.mutable_race(i);
    Race *r = new Race;
    r->exec_id_ = r_proto->exec_id();
    r->addr_ = r_proto->addr();
    for (int j = 0; j < r_proto->event_size(); j++) {
      RaceEventProto *e_proto = r_proto->mutable_event(j);
      RaceEvent *e = new RaceEvent;
      e->thd_id_ = e_proto->thd_id();
      e->static_event_ = FindStaticRaceEvent(e_proto->static_id(), false);
      DEBUG_ASSERT(e->static_event_);
      r->event_vec_.push_back(e);
    }
    r->static_race_ = FindStaticRace(r_proto->static_id(), false);
    DEBUG_ASSERT(r->static_race_);
    race_vec_.push_back(r);
    if (curr_exec_id_ < r->exec_id_)
      curr_exec_id_ = r->exec_id_;
  }
  curr_exec_id_++;
  // load racy insts
  for (int i = 0; i < proto.racy_inst_id_size(); i++) {
    Inst *inst = sinfo->FindInst(proto.racy_inst_id(i));
    DEBUG_ASSERT(inst);
    racy_inst_set_.insert(inst);
  }
}

void RaceDB::Save(const std::string &db_name, StaticInfo *sinfo) {
  RaceDBProto proto;
  // save static events
  for (StaticRaceEvent::Map::iterator it = static_event_table_.begin();
       it != static_event_table_.end(); ++it) {
    StaticRaceEvent *e = it->second;
    StaticRaceEventProto *e_proto = proto.add_static_event();
    e_proto->set_id(e->id_);
    e_proto->set_inst_id(e->inst_->id());
    e_proto->set_type(e->type_);
  }
  // save static races
  for (StaticRace::Map::iterator it = static_race_table_.begin();
       it != static_race_table_.end(); ++it) {
    StaticRace *r = it->second;
    StaticRaceProto *r_proto = proto.add_static_race();
    r_proto->set_id(r->id_);
    for (StaticRaceEvent::Vec::iterator vit = r->event_vec_.begin();
         vit != r->event_vec_.end(); ++vit) {
      StaticRaceEvent *e = *vit;
      r_proto->add_event_id(e->id_);
    }
  }
  // save races
  for (Race::Vec::iterator it = race_vec_.begin();
       it != race_vec_.end(); ++it) {
    Race *r = *it;
    RaceProto *r_proto = proto.add_race();
    r_proto->set_exec_id(r->exec_id_);
    r_proto->set_addr(r->addr_);
    for (RaceEvent::Vec::iterator eit = r->event_vec_.begin();
         eit != r->event_vec_.end(); ++eit) {
      RaceEvent *e = *eit;
      RaceEventProto *e_proto = r_proto->add_event();
      e_proto->set_thd_id(e->thd_id_);
      e_proto->set_static_id(e->static_event_->id_);
    }
    r_proto->set_static_id(r->static_race_->id_);
  }
  // save racy insts
  for (RacyInstSet::iterator it = racy_inst_set_.begin();
       it != racy_inst_set_.end(); ++it) {
    Inst *inst = *it;
    proto.add_racy_inst_id(inst->id());
  }
  // save to file
  std::fstream out(db_name.c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  proto.SerializeToOstream(&out);
  out.close();
}

// helper functions
StaticRaceEvent *RaceDB::CreateStaticRaceEvent(Inst *inst,
                                               RaceEventType type,
                                               bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRaceEvent *static_event = new StaticRaceEvent;
  static_event->id_ = ++curr_static_event_id_;
  static_event->inst_ = inst;
  static_event->type_ = type;
  static_event_table_[static_event->id_] = static_event;
  static_event_index_[static_event->Hash()].push_back(static_event);
  return static_event;
}

StaticRaceEvent *RaceDB::FindStaticRaceEvent(Inst *inst,
                                             RaceEventType type,
                                             bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRaceEvent static_event;
  static_event.inst_ = inst;
  static_event.type_ = type;
  size_t hash_val = static_event.Hash();
  StaticRaceEvent::HashIndex::iterator hit = static_event_index_.find(hash_val);
  if (hit != static_event_index_.end()) {
    for (StaticRaceEvent::Vec::iterator vit = hit->second.begin();
         vit != hit->second.end(); ++vit) {
      StaticRaceEvent *e = *vit;
      if (static_event.Match(e))
        return e;
    }
  }
  return NULL;
}

StaticRaceEvent *RaceDB::FindStaticRaceEvent(StaticRaceEvent::id_t id,
                                             bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRaceEvent::Map::iterator it = static_event_table_.find(id);
  if (it == static_event_table_.end())
    return NULL;
  else
    return it->second;
}

StaticRaceEvent *RaceDB::GetStaticRaceEvent(Inst *inst,
                                            RaceEventType type,
                                            bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRaceEvent *static_event = FindStaticRaceEvent(inst, type, false);
  if (!static_event)
    static_event = CreateStaticRaceEvent(inst, type, false);
  return static_event;
}

StaticRace *RaceDB::CreateStaticRace(StaticRaceEvent *e0,
                                     StaticRaceEvent *e1,
                                     bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRace *static_race = new StaticRace;
  static_race->id_ = ++curr_static_race_id_;
  static_race->event_vec_.push_back(e0);
  static_race->event_vec_.push_back(e1);
  static_race_table_[static_race->id_] = static_race;
  static_race_index_[static_race->Hash()].push_back(static_race);
  return static_race;
}

StaticRace *RaceDB::FindStaticRace(StaticRaceEvent *e0,
                                   StaticRaceEvent *e1,
                                   bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRace static_race;
  static_race.event_vec_.push_back(e0);
  static_race.event_vec_.push_back(e1);
  size_t hash_val = static_race.Hash();
  StaticRace::HashIndex::iterator hit = static_race_index_.find(hash_val);
  if (hit != static_race_index_.end()) {
    for (StaticRace::Vec::iterator vit = hit->second.begin();
         vit != hit->second.end(); ++vit) {
      StaticRace *r = *vit;
      if (static_race.Match(r))
        return r;
    }
  }
  return NULL;
}

StaticRace *RaceDB::FindStaticRace(StaticRace::id_t id,
                                   bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRace::Map::iterator it = static_race_table_.find(id);
  if (it == static_race_table_.end())
    return NULL;
  else
    return it->second;
}

StaticRace *RaceDB::GetStaticRace(StaticRaceEvent *e0,
                                  StaticRaceEvent *e1,
                                  bool locking) {
  ScopedLock locker(internal_lock_, locking);

  StaticRace *static_race = FindStaticRace(e0, e1, false);
  if (!static_race)
    static_race = CreateStaticRace(e0, e1, false);
  return static_race;
}

} // namespace race

