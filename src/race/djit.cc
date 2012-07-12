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

// File: race/djit.cc - Implementation of the data race detector using
// the Djit algorithm.

#include "race/djit.h"

#include "core/logging.h"

namespace race {

Djit::Djit() : track_racy_inst_(false) {
  // do nothing
}

Djit::~Djit() {
  // empty
}

void Djit::Register() {
  Detector::Register();

  knob_->RegisterBool("enable_djit", "whether enable the djit data race detector", "0");
  knob_->RegisterBool("track_racy_inst", "whether track potential racy instructions", "0");
}

bool Djit::Enabled() {
  return knob_->ValueBool("enable_djit");
}

void Djit::Setup(Mutex *lock, RaceDB *race_db) {
  Detector::Setup(lock, race_db);

  track_racy_inst_ = knob_->ValueBool("track_racy_inst");
}

Djit::Meta *Djit::GetMeta(address_t iaddr) {
  Meta::Table::iterator it = meta_table_.find(iaddr);
  if (it == meta_table_.end()) {
    Meta *meta = new DjitMeta(iaddr);
    meta_table_[iaddr] = meta;
    return meta;
  } else {
    return it->second;
  }
}

void Djit::ProcessRead(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  DjitMeta *djit_meta = dynamic_cast<DjitMeta *>(meta);
  DEBUG_ASSERT(djit_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  // check writers
  VectorClock &writer_vc = djit_meta->writer_vc;
  if (!writer_vc.HappensBefore(curr_vc)) {
    DEBUG_FMT_PRINT_SAFE("RAW race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", djit_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    djit_meta->racy = true;
    // RAW race detected, report them
    for (writer_vc.IterBegin(); !writer_vc.IterEnd(); writer_vc.IterNext()) {
      thread_id_t thd_id = writer_vc.IterCurrThd();
      timestamp_t clk = writer_vc.IterCurrClk();
      if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
        DEBUG_ASSERT(djit_meta->writer_inst_table.find(thd_id) !=
                     djit_meta->writer_inst_table.end());
        Inst *writer_inst = djit_meta->writer_inst_table[thd_id];
        // report the race
        ReportRace(djit_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                   curr_thd_id, inst, RACE_EVENT_READ);
      }
    }
  }
  // update meta data
  djit_meta->reader_vc.SetClock(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  djit_meta->reader_inst_table[curr_thd_id] = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    djit_meta->race_inst_set.insert(inst);
  }
}

void Djit::ProcessWrite(thread_id_t curr_thd_id, Meta *meta, Inst *inst) {
  // cast the meta
  DjitMeta *djit_meta = dynamic_cast<DjitMeta *>(meta);
  DEBUG_ASSERT(djit_meta);
  // get the current vector clock
  VectorClock *curr_vc = curr_vc_map_[curr_thd_id];
  VectorClock &writer_vc = djit_meta->writer_vc;
  VectorClock &reader_vc = djit_meta->reader_vc;
  // check writers
  if (!writer_vc.HappensBefore(curr_vc)) {
    DEBUG_FMT_PRINT_SAFE("WAW race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", djit_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    djit_meta->racy = true;
    // WAW race detected, report them
    for (writer_vc.IterBegin(); !writer_vc.IterEnd(); writer_vc.IterNext()) {
      thread_id_t thd_id = writer_vc.IterCurrThd();
      timestamp_t clk = writer_vc.IterCurrClk();
      if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
        DEBUG_ASSERT(djit_meta->writer_inst_table.find(thd_id) !=
                     djit_meta->writer_inst_table.end());
        Inst *writer_inst = djit_meta->writer_inst_table[thd_id];
        // report the race
        ReportRace(djit_meta, thd_id, writer_inst, RACE_EVENT_WRITE,
                   curr_thd_id, inst, RACE_EVENT_WRITE);
      }
    }
  }
  // check readers
  if (!reader_vc.HappensBefore(curr_vc)) {
    DEBUG_FMT_PRINT_SAFE("WAR race detcted [T%lx]\n", curr_thd_id);
    DEBUG_FMT_PRINT_SAFE("  addr = 0x%lx\n", djit_meta->addr);
    DEBUG_FMT_PRINT_SAFE("  inst = [%s]\n", inst->ToString().c_str());
    // mark the meta as racy
    djit_meta->racy = true;
    // WAR race detected, report them
    for (reader_vc.IterBegin(); !reader_vc.IterEnd(); reader_vc.IterNext()) {
      thread_id_t thd_id = reader_vc.IterCurrThd();
      timestamp_t clk = reader_vc.IterCurrClk();
      if (curr_thd_id != thd_id && clk > curr_vc->GetClock(thd_id)) {
        DEBUG_ASSERT(djit_meta->reader_inst_table.find(thd_id) !=
                     djit_meta->reader_inst_table.end());
        Inst *reader_inst = djit_meta->reader_inst_table[thd_id];
        // report the race
        ReportRace(djit_meta, thd_id, reader_inst, RACE_EVENT_READ,
                   curr_thd_id, inst, RACE_EVENT_WRITE);
      }
    }
  }
  // update meta data
  writer_vc.SetClock(curr_thd_id, curr_vc->GetClock(curr_thd_id));
  djit_meta->writer_inst_table[curr_thd_id] = inst;
  // update race inst set if needed
  if (track_racy_inst_) {
    djit_meta->race_inst_set.insert(inst);
  }
}

void Djit::ProcessFree(Meta *meta) {
  // cast the meta
  DjitMeta *djit_meta = dynamic_cast<DjitMeta *>(meta);
  DEBUG_ASSERT(djit_meta);
  // update racy inst set if needed
  if (track_racy_inst_ && djit_meta->racy) {
    for (DjitMeta::InstSet::iterator it = djit_meta->race_inst_set.begin();
         it != djit_meta->race_inst_set.end(); ++it) {
      race_db_->SetRacyInst(*it, true);
    }
  }
  delete djit_meta;
}

} // namespace race

