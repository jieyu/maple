// Eden - A dynamic program analysis framework.
//
// Copyright (C) 2011 Jie Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: delta/randsched_profiler.cpp - Implementation of the PSet profile controller.

#include "delta/randsched_profiler.hpp"

namespace delta {

void RandSchedProfiler::HandlePreSetup() {
  randsched::Scheduler::HandlePreSetup();
//  ExecutionControl::HandlePreSetup();

  knob_->RegisterStr("iroot_in", "The input memorization database.", "iroot.db");
  knob_->RegisterStr("iroot_out", "The output memorization database.", "iroot.db");
  knob_->RegisterStr("ilist_out", "The output memorization database.", "ilist.db");  

  observer_ = new Observer(knob_);
  observer_->Register();
}

void RandSchedProfiler::HandlePostSetup() {
  randsched::Scheduler::HandlePostSetup();
  //ExecutionControl::HandlePostSetup();

  iroot_db_ = new idiom::iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);

  ilist_ = new iList(CreateMutex());

  if (observer_->Enabled()){
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, ilist_);
    AddAnalyzer(observer_);
  }
}

void RandSchedProfiler::HandleProgramExit(){
  randsched::Scheduler::HandleProgramExit();
  //ExecutionControl::HandleProgramExit();

  // output iroot.db
  iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
  // output ilist.db
  ilist_->Save(knob_->ValueStr("ilist_out"), sinfo_);
} 

} // namespace delta

