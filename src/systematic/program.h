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

// File: systematic/program.h - The abstract modeling of a multithreaded
// program and its execution.

#ifndef SYSTEMATIC_PROGRAM_H_
#define SYSTEMATIC_PROGRAM_H_

#include <vector>
#include <list>
#include <set>
#include <map>
#include <tr1/unordered_map>
#include "core/basictypes.h"
#include "core/static_info.h"
#include "systematic/program.pb.h"

namespace systematic {

// foward declaration
class Program;
class Execution;

// a thread identifier
class Thread {
 public:
  typedef uint32 uid_t;
  typedef uint32 idx_t;
  typedef size_t hash_val_t;
  typedef std::vector<Thread *> Vec;
  typedef std::set<Thread *> Set;
  typedef std::map<uid_t, Thread *> UidMap;
  typedef std::tr1::unordered_map<hash_val_t, Vec> HashMap;

  uid_t uid() { return uid_; }

 protected:
  Thread()
      : uid_(0),
        creator_(NULL),
        creator_idx_(0) {}

  ~Thread() {}

  hash_val_t Hash();
  bool Match(Thread *thd);
  bool IsMainThread() { return !creator_; }

  uid_t uid_;         // the unique identifier across runs
  Thread *creator_;   // the thread that creates it
  idx_t creator_idx_; // the idx-th thread that is created by creator

 private:
  friend class Program;

  DISALLOW_COPY_CONSTRUCTORS(Thread);
};

// a program object (memory object or synchronization object)
class Object {
 public:
  typedef uint32 uid_t;
  typedef uint32 idx_t;
  typedef size_t hash_val_t;
  typedef std::vector<Object *> Vec;
  typedef std::tr1::unordered_map<uid_t, Object *> UidMap;
  typedef std::tr1::unordered_map<hash_val_t, Vec> HashMap;

  uid_t uid() { return uid_; }

 protected:
  Object() : uid_(0) {}
  virtual ~Object() {}

  virtual hash_val_t Hash() = 0;
  virtual bool Match(Object *obj) = 0;

  uid_t uid_;         // the unique identifier across runs

 private:
  friend class Program;

  DISALLOW_COPY_CONSTRUCTORS(Object);
};

// a static program object (in .data and .bss fields)
class SObject : public Object {
 protected:
  SObject() : image_(NULL), offset_(0) {}
  ~SObject() {}

  hash_val_t Hash();
  bool Match(Object *obj);

  Image *image_;      // the image that contains the object
  address_t offset_;  // the offset in the image

 private:
  friend class Program;

  DISALLOW_COPY_CONSTRUCTORS(SObject);
};

// a dynamic program object (by malloc or new)
class DObject : public Object {
 protected:
  DObject()
      : creator_(NULL),
        creator_inst_(NULL),
        creator_idx_(0),
        offset_(0) {}

  ~DObject() {}

  hash_val_t Hash();
  bool Match(Object *obj);

  Thread *creator_;     // the thread that creates it
  Inst *creator_inst_;  // the inst. that creates the object
  idx_t creator_idx_;   // the idx-th object that is created by creator and inst
  address_t offset_;

 private:
  friend class Program;

  DISALLOW_COPY_CONSTRUCTORS(DObject);
};

// represent a program
class Program {
 public:
  Program()
      : curr_thd_uid_(0),
        curr_obj_uid_(0) {}

  ~Program() {}

  Thread *GetMainThread();
  Thread *GetThread(Thread *creator, Thread::idx_t creator_idx);
  SObject *GetSObject(Image *image, address_t offset);
  DObject *GetDObject(Thread *creator,
                      Inst *creator_inst,
                      Object::idx_t creator_idx,
                      address_t offset);
  Thread *FindThread(Thread::uid_t uid);
  Object *FindObject(Object::uid_t uid);
  void Load(const std::string &db_name, StaticInfo *sinfo);
  void Save(const std::string &db_name, StaticInfo *sinfo);

 protected:
  Thread::uid_t curr_thd_uid_;
  Object::uid_t curr_obj_uid_;
  Thread::UidMap thd_uid_table_;
  Object::UidMap obj_uid_table_;
  Thread::HashMap thd_hash_table_;
  Object::HashMap obj_hash_table_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Program);
};

// an action performed by the program
class Action {
 public:
  typedef uint64 idx_t;
  typedef std::vector<Action *> Vec;
  typedef std::list<Action *> List;
  typedef std::set<Action *> Set;
  typedef std::map<Thread *, Action *> Map;

  bool IsThreadOp();
  bool IsMutexOp();
  bool IsCondOp();
  bool IsBarrierOp();
  bool IsMemOp();
  bool IsYieldOp();
  bool IsWrite();
  std::string ToString();

  Execution *exec() { return exec_; }
  size_t idx() { return idx_; }
  Thread *thd() { return thd_; }
  Object *obj() { return obj_; }
  Operation op() { return op_; }
  Inst *inst() { return inst_; }
  Action::idx_t tc() { return tc_; }
  Action::idx_t oc() { return oc_; }
  bool yield() { return yield_; }
  void set_tc(idx_t tc) { tc_ = tc; }
  void set_oc(idx_t oc) { oc_ = oc; }
  void set_yield(bool yield) { yield_ = yield; };

 protected:
  Action()
      : exec_(NULL),
        idx_(0),
        thd_(NULL),
        obj_(NULL),
        op_(OP_INVALID),
        inst_(NULL),
        tc_(0),
        oc_(0),
        yield_(false) {}

  ~Action() {}

  Execution *exec_; // which execution this action belongs to
  size_t idx_;      // the idx-th action created in the execution
  Thread *thd_;     // the thread that performs the action
  Object *obj_;     // the object on which the action works (can be NULL)
  Operation op_;    // the operation type
  Inst *inst_;      // the inst. that performs the action (can be NULL)

  // the following fields won't be set until the action is executed
  idx_t tc_;        // the tc-th action performed by the thread
  idx_t oc_;        // the oc-th write to the object
  bool yield_;      // whether this action is a yield operation

 private:
  friend class Execution;

  DISALLOW_COPY_CONSTRUCTORS(Action);
};

// a program state represented by a set of actions
class State {
 public:
  typedef std::vector<State *> Vec;

  bool IsInitial();
  bool IsTerminal();
  bool IsEnabled(Thread *thd);
  void AddEnabled(Action *action);
  Action *FindEnabled(Thread *thd);
  State *Prev();
  State *Next();
  std::string ToString();

  Execution *exec() { return exec_; }
  size_t idx() { return idx_; }
  Action::Map *enabled() { return &enabled_; }
  Action *taken() { return taken_; }
  void set_taken(Action *taken) { taken_ = taken; }

 protected:
  State()
      : exec_(NULL),
        idx_(0),
        taken_(NULL) {}

  ~State() {}

  Execution *exec_;     // which execution this state belongs to
  size_t idx_;          // the id-th state in the execution
  Action::Map enabled_; // enabled threads at the moment
  Action *taken_;       // the actual taken action from this state

 private:
  friend class Execution;

  DISALLOW_COPY_CONSTRUCTORS(State);
};

// represent an exection
class Execution {
 public:
  typedef std::vector<Execution *> Vec;

  Execution() {}
  ~Execution() {}

  Action *CreateAction(Thread *thd, Object *obj, Operation op, Inst *inst);
  State *CreateState();
  State *Prev(State *state);
  State *Next(State *state);
  State *FindState(size_t idx);
  void Load(const std::string &db_name, StaticInfo *sinfo, Program *program);
  void Save(const std::string &db_name, StaticInfo *sinfo, Program *program);

 protected:
  Action::Vec action_vec_;
  State::Vec state_vec_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(Execution);
};

// inline functions
inline bool State::IsInitial() {
  return exec_ && idx_ == 0;
}

inline bool State::IsTerminal() {
  return exec_ && enabled_.empty();
}

inline bool State::IsEnabled(Thread *thd) {
  return enabled_.find(thd) != enabled_.end();
}

inline void State::AddEnabled(Action *action) {
  enabled_[action->thd()] = action;
}

inline Action *State::FindEnabled(Thread *thd) {
  Action::Map::iterator it = enabled_.find(thd);
  if (it == enabled_.end())
    return NULL;
  else
    return it->second;
}

inline State *State::Prev() {
  return exec_->Prev(this);
}

inline State *State::Next() {
  return exec_->Next(this);
}

} // namespace systematic

#endif

