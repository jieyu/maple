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

// File: systematic/scheduler.h - The abstract definition of scheduler
// for systematic concurrency testing.

#ifndef SYSTEMATIC_SCHEDULER_H_
#define SYSTEMATIC_SCHEDULER_H_

#include "core/basictypes.h"
#include "core/knob.h"
#include "core/descriptor.h"
#include "systematic/program.h"

namespace systematic {

// The definition of the controller interface
class ControllerInterface {
 public:
  virtual Knob *GetKnob() = 0;
  virtual StaticInfo *GetStaticInfo() = 0;
  virtual Program *GetProgram() = 0;
  virtual Execution *GetExecution() = 0;
  virtual State *Execute(State *state, Action *action) = 0;

 protected:
  virtual ~ControllerInterface() {}
};

// The abstract definition of scheduler.
class Scheduler {
 public:
  explicit Scheduler(ControllerInterface *controller)
      : controller_(controller) {}

  virtual ~Scheduler() {}

  // virtual functions (can be overrided)
  virtual void Register() = 0;
  virtual bool Enabled() = 0;
  virtual void Setup() = 0;
  virtual void ProgramStart() = 0;
  virtual void ProgramExit() = 0;
  virtual void Explore(State *init_state) = 0;

  // the main entry of the scheduler
  void Main(State *init_state);

  Descriptor *desc() { return &desc_; }
  Knob *knob() { return controller_->GetKnob(); }
  StaticInfo *sinfo() { return controller_->GetStaticInfo(); }
  Program *program() { return controller_->GetProgram(); }
  Execution *execution() { return controller_->GetExecution(); }

 protected:
  State *Execute(State *state, Action *action); // should not be override

 private:
  void SetActionCounters(State *state);
  Action::idx_t &GetThreadCounter(Thread *thd);
  Action::idx_t &GetObjectCounter(Object *obj);

  // should not be visible to subclasses
  ControllerInterface *controller_;
  Descriptor desc_;
  std::map<Thread *, Action::idx_t> tc_map_;
  std::map<Object *, Action::idx_t> oc_map_;

  DISALLOW_COPY_CONSTRUCTORS(Scheduler);
};

} // namespace systematic

#endif

