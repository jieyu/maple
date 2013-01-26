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

// File: idiom/iroot.h - Define iroot and iroot database.

#ifndef IDIOM_IROOT_H_
#define IDIOM_IROOT_H_

#include <vector>
#include <tr1/unordered_map>

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/atomic.h"
#include "idiom/iroot.pb.h" // protobuf head file

namespace idiom {

// forward declaration
class iRootDB;

#define IROOT_EVENT_TYPE_ARRAYSIZE iRootEventType_ARRAYSIZE

typedef uint32 iroot_event_id_t;
#define INVALID_IROOT_EVENT_ID static_cast<iroot_event_id_t>(-1)

class iRootEvent {
 public:
  iroot_event_id_t id() { return proto_->id(); }
  Inst *inst() { return inst_; }
  iRootEventType type() { return proto_->type(); }

  bool IsMem() {
    if (type() == IROOT_EVENT_MEM_READ ||
        type() == IROOT_EVENT_MEM_WRITE)
      return true;
    else
      return false;
  }

  bool IsSync() {
    if (type() == IROOT_EVENT_MUTEX_LOCK ||
        type() == IROOT_EVENT_MUTEX_UNLOCK)
      return true;
    else
      return false;
  }

 protected:
  iRootEvent(Inst *inst, iRootEventProto *proto) : inst_(inst), proto_(proto) {}
  ~iRootEvent() {}

  Inst *inst_;
  iRootEventProto *proto_;

 private:
  friend class iRootDB;

  DISALLOW_COPY_CONSTRUCTORS(iRootEvent);
};

typedef uint32 iroot_id_t;
#define INVALID_IROOT_ID static_cast<iroot_id_t>(-1)

class iRoot {
 public:
  iroot_id_t id() { return proto_->id(); }
  IdiomType idiom() { return proto_->idiom(); }

  void AddEvent(iRootEvent *event) { events_.push_back(event); }

  void AddCountPair(std::pair<int, int>* counts) { 

      proto_->set_count_pair_bool(1);
      proto_->set_src_count(counts->first);
      proto_->set_dst_count(counts->second);
  }

  uint32 getDstCount() { return proto_->src_count(); }
  uint32 getSrcCount() { return proto_->dst_count(); }
  uint32 getCountPairBool() { return proto_->count_pair_bool(); }

  iRootEvent *GetEvent(int index) { return events_[index]; }
  bool HasMem();
  bool HasSync();
  bool HasCommonLibEvent();

  static int GetNumEvents(IdiomType idiom);

 protected:
  iRoot(iRootProto *proto) : proto_(proto) {}
  ~iRoot() {}

  std::vector<iRootEvent *> events_;
  iRootProto *proto_;

 private:
  friend class iRootDB;

  DISALLOW_COPY_CONSTRUCTORS(iRoot);
};

class iRootDB {
 public:
  explicit iRootDB(Mutex *lock);
  ~iRootDB() {}

  iRootEvent *GetiRootEvent(Inst *inst, iRootEventType type, bool locking);
  iRootEvent *FindiRootEvent(iroot_event_id_t event_id, bool locking);
  iRoot *GetiRoot(IdiomType idiom, bool locking, ...);
  iRoot *FindiRoot(iroot_id_t iroot_id, bool locking);
  void Load(const std::string &db_name, StaticInfo *sinfo);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 protected:
  typedef std::vector<iRootEvent *> iRootEventVec;
  typedef std::vector<iRoot *> iRootVec;
  typedef std::tr1::unordered_map<iroot_event_id_t, iRootEvent *> iRootEventMap;
  typedef std::tr1::unordered_map<iroot_id_t, iRoot *> iRootMap;
  typedef std::tr1::unordered_map<size_t, iRootEventVec> iRootEventHashIndex;
  typedef std::tr1::unordered_map<size_t, iRootVec> iRootHashIndex;

  iRootEvent *FindiRootEvent(Inst *inst, iRootEventType type, bool locking);
  iRootEvent *CreateiRootEvent(Inst *inst, iRootEventType type, bool locking);
  iRoot *FindiRoot(IdiomType idiom, iRootEventVec *events, bool locking);
  iRoot *CreateiRoot(IdiomType idiom, iRootEventVec *events, bool locking);

  iroot_event_id_t GetNextiRootEventID() {
    return ATOMIC_ADD_AND_FETCH(&curr_event_id_, 1);
  }

  iroot_id_t GetNextiRootID() {
    return ATOMIC_ADD_AND_FETCH(&curr_iroot_id_, 1);
  }

  static size_t HashiRootEvent(Inst *inst, iRootEventType type) {
    return (size_t)inst + (size_t)type;
  }

  static size_t HashiRoot(IdiomType idiom, iRootEventVec *events) {
    size_t hash_val = (size_t)idiom;
    for (size_t i = 0; i < events->size(); i++) {
      hash_val += (size_t)(*events)[i];
    }
    return hash_val;
  }

  Mutex *internal_lock_;
  iroot_event_id_t curr_event_id_;
  iroot_id_t curr_iroot_id_;
  iRootEventMap event_map_;
  iRootMap iroot_map_;
  iRootEventHashIndex event_index_;
  iRootHashIndex iroot_index_;
  iRootDBProto proto_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(iRootDB);
};

} // namespace idiom

#endif

