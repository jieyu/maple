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

// File: idiom/iroot.cc - Implementation of iroot and iroot database

#include "idiom/iroot.h"

#include <cstdarg>
#include <cassert>
#include <fstream>

namespace idiom {

bool iRoot::HasMem() {
  for (size_t i = 0; i < events_.size(); i++) {
    iRootEvent *e = events_[i];
    if (e->IsMem())
      return true;
  }
  return false;
}

bool iRoot::HasSync() {
  for (size_t i = 0; i < events_.size(); i++) {
    iRootEvent *e = events_[i];
    if (e->IsSync())
      return true;
  }
  return false;
}

bool iRoot::HasCommonLibEvent() {
  for (size_t i = 0; i < events_.size(); i++) {
    iRootEvent *e = events_[i];
    if (e->inst()->image()->IsCommonLib())
      return true;
  }
  return false;
}

int iRoot::GetNumEvents(IdiomType idiom) {
  int num_args = 0;
  switch (idiom) {
    case IDIOM_1:
      num_args = 2;
      break;
    case IDIOM_2:
      num_args = 3;
      break;
    case IDIOM_3:
    case IDIOM_4:
    case IDIOM_5:
      num_args = 4;
      break;
    default:
      assert(0);
      break;
  }
  return num_args;
}

iRootDB::iRootDB(Mutex *lock)
    : internal_lock_(lock),
      curr_event_id_(0),
      curr_iroot_id_(0) {
  // empty
}

iRootEvent *iRootDB::GetiRootEvent(Inst *inst, iRootEventType type,
                                   bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootEvent *event = FindiRootEvent(inst, type, false);
  if (!event)
    event = CreateiRootEvent(inst, type, false);
  return event;
}

iRootEvent *iRootDB::FindiRootEvent(iroot_event_id_t event_id,
                                    bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootEventMap::iterator it = event_map_.find(event_id);
  if (it == event_map_.end())
    return NULL;
  else
    return it->second;
}

iRoot *iRootDB::GetiRoot(IdiomType idiom, bool locking, ...) {
  ScopedLock locker(internal_lock_, locking);

  int num_args = iRoot::GetNumEvents(idiom);
  iRootEventVec events;
  va_list vl;
  va_start(vl, locking);
  for (int i = 0; i < num_args; i++) {
    iRootEvent *event = va_arg(vl, iRootEvent *);
    events.push_back(event);
  }
  va_end(vl);

  iRoot *iroot = FindiRoot(idiom, &events, false);
  if (!iroot)
    iroot = CreateiRoot(idiom, &events, false);
  return iroot;
}

iRoot *iRootDB::FindiRoot(iroot_id_t iroot_id, bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootMap::iterator it = iroot_map_.find(iroot_id);
  if (it == iroot_map_.end())
    return NULL;
  else
    return it->second;
}

iRootEvent *iRootDB::FindiRootEvent(Inst *inst, iRootEventType type,
                                    bool locking) {
  ScopedLock locker(internal_lock_, locking);

  size_t hash_val = HashiRootEvent(inst, type);
  iRootEventHashIndex::iterator it = event_index_.find(hash_val);
  if (it == event_index_.end()) {
    return NULL;
  } else {
    for (size_t i = 0; i < it->second.size(); i++) {
      iRootEvent *event = it->second[i];
      if (event->inst() == inst && event->type() == type)
        return event;
    }
    return NULL;
  }
}

iRootEvent *iRootDB::CreateiRootEvent(Inst *inst, iRootEventType type,
                                      bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootEventProto *event_proto = proto_.add_event();
  iroot_event_id_t event_id = GetNextiRootEventID();
  event_proto->set_id(event_id);
  event_proto->set_inst_id(inst->id());
  event_proto->set_type(type);
  iRootEvent *event = new iRootEvent(inst, event_proto);
  event_map_[event_id] = event;
  size_t hash_val = HashiRootEvent(inst, type);
  event_index_[hash_val].push_back(event); // update index
  return event;
}

iRoot *iRootDB::FindiRoot(IdiomType idiom, iRootEventVec *events,
                          bool locking) {
  ScopedLock locker(internal_lock_, locking);

  size_t hash_val = HashiRoot(idiom, events);
  iRootHashIndex::iterator it = iroot_index_.find(hash_val);
  if (it == iroot_index_.end()) {
    return NULL;
  } else {
    for (size_t i = 0; i < it->second.size(); i++) {
      iRoot *iroot = it->second[i];
      if (iroot->idiom() == idiom) {
        bool match = true;
        for (size_t j = 0; j < events->size(); j++) {
          if (iroot->events_[j] != (*events)[j]) {
            match = false;
            break;
          }
        }
        if (match)
          return iroot;
      }
    }
    return NULL;
  }
}

iRoot *iRootDB::CreateiRoot(IdiomType idiom, iRootEventVec *events,
                            bool locking) {
  ScopedLock locker(internal_lock_, locking);

  iRootProto *iroot_proto = proto_.add_iroot();
  iroot_id_t iroot_id = GetNextiRootID();
  iroot_proto->set_id(iroot_id);
  iroot_proto->set_idiom(idiom);
  //pankit fix
  iroot_proto->set_src_count(0);
  iroot_proto->set_dst_count(0);
  iroot_proto->set_count_pair_bool(0);

  iRoot *iroot = new iRoot(iroot_proto);
  for (size_t i = 0; i < events->size(); i++) {
    iRootEvent *event = (*events)[i];
    iroot_proto->add_event_id(event->id());
    iroot->AddEvent(event);
  }

  iroot_map_[iroot_id] = iroot;
  size_t hash_val = HashiRoot(idiom, events);
  iroot_index_[hash_val].push_back(iroot); // update index
  return iroot;
}

void iRootDB::Load(const std::string &db_name, StaticInfo *sinfo) {
  std::fstream in;
  in.open(db_name.c_str(), std::ios::in | std::ios::binary);
  proto_.ParseFromIstream(&in);
  in.close();
  // setup iroot event map
  for (int i = 0; i < proto_.event_size(); i++) {
    iRootEventProto *event_proto = proto_.mutable_event(i);
    Inst *inst = sinfo->FindInst(event_proto->inst_id());
    iRootEvent *event = new iRootEvent(inst, event_proto);
    iroot_event_id_t event_id = event->id();
    event_map_[event_id] = event;
    size_t hash_val = HashiRootEvent(event->inst(), event->type());
    event_index_[hash_val].push_back(event); // update event index
    if (event_id > curr_event_id_)
      curr_event_id_ = event_id;
  }
  // setup iroot map
  for (int i = 0; i < proto_.iroot_size(); i++) {
    iRootProto *iroot_proto = proto_.mutable_iroot(i);
    iRoot *iroot = new iRoot(iroot_proto);
    iroot_id_t iroot_id = iroot->id();
    // add events to iroot
    for (int j = 0; j < iroot_proto->event_id_size(); j++) {
      iRootEvent *event = FindiRootEvent(iroot_proto->event_id(j), false);
      iroot->AddEvent(event);
    }
    iroot_map_[iroot_id] = iroot;
    size_t hash_val = HashiRoot(iroot->idiom(), &iroot->events_);
    iroot_index_[hash_val].push_back(iroot); // update iroot index
    if (iroot_id > curr_iroot_id_)
      curr_iroot_id_ = iroot_id;
  }
}

void iRootDB::Save(const std::string &db_name, StaticInfo *sinfo) {
  std::fstream out;
  out.open(db_name.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
  proto_.SerializeToOstream(&out);
  out.close();
}

} // namespace idiom

