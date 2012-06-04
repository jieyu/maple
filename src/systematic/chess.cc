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

// File: systematic/chess.cc - The implementation of the CHESS scheduler
// which systematically explore thread interleavings using iterative
// preemption bound.

#include "systematic/chess.h"

#include <sys/stat.h>
#include <sstream>
#include "core/logging.h"

namespace systematic {

ChessScheduler::ChessScheduler(ControllerInterface *controller)
    : Scheduler(controller),
      pb_enable_(false),
      por_enable_(false),
      pb_limit_(0),
      useless_(false),
      divergence_(false),
      curr_state_(NULL),
      curr_action_(NULL),
      curr_node_(NULL),
      prefix_size_(0),
      curr_preemptions_(0),
      curr_hash_val_(0),
      curr_exec_id_(0) {
  // empty
}

ChessScheduler::~ChessScheduler() {
  // empty
}

void ChessScheduler::Register() {
  knob()->RegisterBool("enable_chess_scheduler", "whether use the CHESS scheduler", "0");
  knob()->RegisterBool("fair", "whether enable the fair control module", "1");
  knob()->RegisterBool("pb", "whether enable preemption bound search", "1");
  knob()->RegisterBool("por", "whether enable parital order reduction", "1");
  knob()->RegisterBool("abort_diverge", "whether abort when divergence happens", "1");
  knob()->RegisterInt("pb_limit", "the maximum number of preemption an execution can have", "2");
  knob()->RegisterStr("search_in", "the input file that contains the search information", "search.db");
  knob()->RegisterStr("search_out", "the output file that contains the search information", "search.db");
  knob()->RegisterStr("por_info_path", "the dir path that stores the partial order reduction information", "por-info");
}

bool ChessScheduler::Enabled() {
  return knob()->ValueBool("enable_chess_scheduler");
}

void ChessScheduler::Setup() {
  // settings and flags
  fair_enable_ = knob()->ValueBool("fair");
  pb_enable_ = knob()->ValueBool("pb");
  por_enable_ = knob()->ValueBool("por");
  pb_limit_ = knob()->ValueInt("pb_limit");
  por_info_path_ = knob()->ValueStr("por_info_path");

  // load search info
  search_info_.Load(knob()->ValueStr("search_in"), sinfo(), program());
  if (search_info_.Done()) {
    printf("[CHESS] search done\n");
    exit(0);
  }
  prefix_size_ = search_info_.StackSize();
  DEBUG_FMT_PRINT_SAFE("prefix size = %d\n", (int)prefix_size_);

  // setup descriptor
  desc()->SetHookYieldFunc();

  // seed the random number generator
  srand((unsigned int)time(NULL));
}

void ChessScheduler::ProgramStart() {
  // init components
  if (pb_enable_)
    PbInit();
  if (por_enable_)
    PorInit();
}

void ChessScheduler::ProgramExit() {
  // fini components
  if (pb_enable_)
    PbFini();
  if (por_enable_)
    PorFini();

  // save search info
  if (!divergence_) {
    search_info_.UpdateForNext();
    search_info_.Save(knob()->ValueStr("search_out"), sinfo(), program());
  }
}

void ChessScheduler::Explore(State *init_state) {
  // start with the initial state
  curr_state_ = init_state;
  // run until no enabled thread
  while (!curr_state_->IsTerminal()) {
    // get next node in the search stack
    curr_node_ = search_info_.GetNextNode(curr_state_);
    if (!curr_node_) {
      // divergence run
      DivergenceRun();
      return;
    }
    // update backtrack: add all enabled thread to backtrack
    // this is necessary because we want to explore all possible
    // interleavings. we only need to do it once.
    if (!IsPrefix())
      UpdateBacktrack();
    // update fair control status
    if (fair_enable_)
      FairUpdate();
    // pick the next action to execute
    Action *next_action = PickNext();
    if (!next_action) {
      // useless run
      UselessRun();
      return;
    }
    // update search node
    curr_node_->set_sel(next_action->thd());
    if (!IsPrefix())
      curr_node_->AddDone(next_action->thd());
    DEBUG_FMT_PRINT_SAFE("Schedule Point: %s\n",
                         curr_node_->ToString().c_str());
    // execute the action and move to next state
    if (pb_enable_)
      PbUpdate(next_action);
    if (por_enable_)
      PorUpdate(next_action);
    curr_action_ = next_action;
    curr_state_ = Execute(curr_state_, next_action);
  }
}

void ChessScheduler::DivergenceRun() {
  printf("[CHESS] divergence happens\n");
  // mark this run as divergence
  divergence_ = true;

  // abort if needed
  if (knob()->ValueBool("abort_diverge"))
    assert(0);

  // run until no enabled threads
  while (!curr_state_->IsTerminal()) {
    // just pick an enabled thread randomly
    Action *next_action = PickNextRandom();
    DEBUG_ASSERT(next_action);
    // execute the next action
    curr_action_ = next_action;
    curr_state_ = Execute(curr_state_, next_action);
  }
}

void ChessScheduler::UselessRun() {
  printf("[CHESS] useless run\n");
  // mark this run as useless
  useless_ = true;
  // run until no enabled threads
  while (!curr_state_->IsTerminal()) {
    // just pick an enabled thread randomly
    Action *next_action = PickNextRandom();
    DEBUG_ASSERT(next_action);
    // execute the next action
    curr_action_ = next_action;
    curr_state_ = Execute(curr_state_, next_action);
  }
}

Action *ChessScheduler::PickNext() {
  // replay the prefix
  if (IsPrefix()) {
    Action *next_action = curr_state_->FindEnabled(curr_node_->sel());
    DEBUG_ASSERT(next_action);
    return next_action;
  }

  // first pass, for each undone enabled action, check whether
  // the next state is visted or not. if yes, mark it as done
  // also, check preemptions. if exceed the bound, mark it as done
  for (Action::Map::iterator it = curr_state_->enabled()->begin();
       it != curr_state_->enabled()->end(); ++it) {
    Action *action = it->second;
    if (!curr_node_->IsDone(action->thd())) {
      // action is not done
      // 1) check fair (if fair is enabled)
      if (fair_enable_) {
        if (!FairEnabled(action)) {
          DEBUG_FMT_PRINT_SAFE("Fair pruned\n");
          curr_node_->AddDone(action->thd());
        }
      }
      // 2) check preemptions (if pb is enabled)
      if (pb_enable_) {
        if (!PbEnabled(action)) {
          DEBUG_FMT_PRINT_SAFE("PB pruned\n");
          curr_node_->AddDone(action->thd());
        }
      }
      // 3) check visited (if por is enabled)
      if (por_enable_) {
        if (PorVisited(action)) {
          DEBUG_FMT_PRINT_SAFE("POR pruned\n");
          curr_node_->AddDone(action->thd());
        }
      }
    }
  }
  // second pass, find an undone enabled action
  // a heuristic we used here is that we always favor non
  // preemptive choices over preemptive ones
  Action *next_action = NULL;
  for (Action::Map::iterator it = curr_state_->enabled()->begin();
       it != curr_state_->enabled()->end(); ++it) {
    Action *action = it->second;
    if (!curr_node_->IsDone(action->thd())) {
      // action is not done
      if (!next_action) {
        next_action = action;
      } else {
        if (!IsPreemptiveChoice(action))
          next_action = action;
      }
    }
  }
  // return the next action (could be NULL)
  return next_action;
}

Action *ChessScheduler::PickNextRandom() {
  Action::Map *enabled = curr_state_->enabled();
  Action *target = NULL;
  int counter = 1;
  for (Action::Map::iterator it = enabled->begin(); it != enabled->end(); ++it){
    Action *current = it->second;
    // decide whether to pick the current one
    if (RandomChoice(1.0 / (double)counter)) {
      target = current;
    }
    // increment the counter
    counter += 1;
  }
  DEBUG_ASSERT(target);
  return target;
}

bool ChessScheduler::IsPreemptiveChoice(Action *action) {
  if (curr_action_ &&
      curr_state_->IsEnabled(curr_action_->thd()) &&
      action->thd() != curr_action_->thd())
    return true;
  else
    return false;
}

bool ChessScheduler::IsFrontier() {
  return curr_node_->idx() + 1 == prefix_size_;
}

bool ChessScheduler::IsPrefix() {
  return !IsFrontier() && curr_node_->idx() < prefix_size_;
}

void ChessScheduler::UpdateBacktrack() {
  for (Action::Map::iterator it = curr_state_->enabled()->begin();
       it != curr_state_->enabled()->end(); ++it) {
    Action *action = it->second;
    curr_node_->AddBacktrack(action->thd());
  }
}

bool ChessScheduler::RandomChoice(double true_rate) {
  double val = rand() / (RAND_MAX + 1.0);
  if (val < true_rate)
    return true;
  else
    return false;
}

ChessScheduler::hash_val_t ChessScheduler::Hash(Action *action) {
  DEBUG_ASSERT(action->obj() && action->inst());
  hash_val_t hash_val = 0;
  hash_val += (hash_val_t)action->thd()->uid();
  hash_val += (hash_val_t)(action->obj()->uid() << 2);
  hash_val += (hash_val_t)(action->op() << 5);
  hash_val += (hash_val_t)(action->inst()->id() << 7);
  hash_val += (hash_val_t)(action->tc() << 13);
  hash_val += (hash_val_t)(action->oc() << 23);
  return hash_val;
}

// fair related
void ChessScheduler::FairUpdate() {
  fair_ctrl_.Update(curr_state_);
  DEBUG_FMT_PRINT_SAFE("Fair control status\n%s",
                       fair_ctrl_.ToString().c_str());
}

bool ChessScheduler::FairEnabled(Action *next_action) {
  return fair_ctrl_.Enabled(curr_state_, next_action);
}

// preemption bound related
void ChessScheduler::PbInit() {
  DEBUG_ASSERT(pb_enable_);
  curr_preemptions_ = 0;
}

void ChessScheduler::PbFini() {
  DEBUG_ASSERT(pb_enable_);
  // empty
}

void ChessScheduler::PbUpdate(Action *next_action) {
  DEBUG_ASSERT(pb_enable_);
  if (IsPreemptiveChoice(next_action)) {
    curr_preemptions_ += 1;
    DEBUG_FMT_PRINT_SAFE("Preemption\n");
  }
}

bool ChessScheduler::PbEnabled(Action *next_action) {
  if (IsPreemptiveChoice(next_action)) {
    if (curr_preemptions_ + 1 > pb_limit_) {
      return false;
    }
  }
  return true;
}

// partial order reduction related functions
void ChessScheduler::PorInit() {
  DEBUG_ASSERT(por_enable_);
  curr_hash_val_ = 0;
  PorLoad();
}

void ChessScheduler::PorFini() {
  DEBUG_ASSERT(por_enable_);
  if (!divergence_ && !useless_)
    PorSave();
}

void ChessScheduler::PorUpdate(Action *next_action) {
  DEBUG_ASSERT(por_enable_);

  // skip transparent actions
  if (!next_action->obj())
    return;

  curr_hash_val_ = HashJoin(curr_hash_val_, Hash(next_action));
  // update visited states
  VisitedState *vs = new VisitedState;
  vs->hash_val = curr_hash_val_;
  vs->preemptions = curr_preemptions_; // PbUpdate is called already
  vs->exec_id = curr_exec_id_;
  vs->state_idx = curr_state_->idx() + 1; //  next state idx
  curr_visited_states_.push_back(vs);
}

bool ChessScheduler::PorVisited(Action *next_action) {
  DEBUG_ASSERT(por_enable_);

  // skip transparent actions
  if (!next_action->obj())
    return false;

  // check whether the state to which the next_action will
  // lead is visted or not
  hash_val_t new_hash_val = HashJoin(curr_hash_val_, Hash(next_action));
  int new_preemptions = curr_preemptions_;
  if (IsPreemptiveChoice(next_action))
    new_preemptions += 1;
  VisitedState::HashMap::iterator hit = visited_states_.find(new_hash_val);
  if (hit != visited_states_.end()) {
    for (VisitedState::Vec::iterator vit = hit->second.begin();
         vit != hit->second.end(); ++vit) {
      VisitedState *vs = *vit;
      Execution *vs_exec = PorGetExec(vs->exec_id);
      State *vs_state = vs_exec->FindState(vs->state_idx);
      DEBUG_ASSERT(vs_state);
      DEBUG_FMT_PRINT_SAFE("matching hash found, val = 0x%lx\n", new_hash_val);
      DEBUG_FMT_PRINT_SAFE("   preemption = %d, exec_id = %d, state_idx = %d\n",
                           vs->preemptions, vs->exec_id, (int)vs->state_idx);
      if (vs->preemptions <= new_preemptions &&
          PorStateMatch(curr_state_, next_action, vs_state)) {
        return true;
      }
    }
  }
  return false;
}

bool ChessScheduler::PorStateMatch(State *state,
                                   Action *action,
                                   State *vs_state) {
  DEBUG_ASSERT(state->exec() != vs_state->exec());
  // check whether there will is an one-on-one mapping
  // between actions in the two executions

  // 1) get all actions before vs_state in vs_exec
  ActionHashMap vs_action_hash_table;
  for (State *s = vs_state->Prev(); s; s = s->Prev()) {
    Action *a = s->taken();
    // skip transparent actions
    if (!a->obj())
      continue;
    vs_action_hash_table[Hash(a)].push_back(a);
  }

  // 2) check with all actions in exec
  for (State *s = state; s; s = s->Prev()) {
    Action *a = (s == state ? action : s->taken());
    // skip transparent actions
    if (!a->obj())
      continue;
    ActionHashMap::iterator hit = vs_action_hash_table.find(Hash(a));
    if (hit == vs_action_hash_table.end()) {
      DEBUG_FMT_PRINT_SAFE("   vs hash not found\n");
      DEBUG_FMT_PRINT_SAFE("   %s\n", a->ToString().c_str());
      return false;
    } else {
      bool found = false;
      for (Action::List::iterator lit = hit->second.begin();
           lit != hit->second.end(); ++lit) {
        Action *vs_a = *lit;
        if (a->thd() == vs_a->thd() &&
            a->obj() == vs_a->obj() &&
            a->op() == vs_a->op() &&
            a->inst() == vs_a->inst() &&
            a->tc() == vs_a->tc() &&
            a->oc() == vs_a->oc()) {
          hit->second.erase(lit);
          found = true;
          break;
        }
      }
      if (!found) {
        DEBUG_FMT_PRINT_SAFE("   vs match not found\n");
        return false;
      }
    }
  }
  // two states match when reach here
  return true;
}

Execution *ChessScheduler::PorGetExec(int exec_id) {
  DEBUG_ASSERT(por_enable_);

  // check whether the execution is loaded or not
  // if not, load it from file
  ExecutionTable::iterator it = loaded_execs_.find(exec_id);
  if (it == loaded_execs_.end()) {
    DEBUG_FMT_PRINT_SAFE("loading execution %d\n", exec_id);
    // prepare the directory for por
    PorPrepareDir();
    // load execution from file
    std::stringstream exec_path_ss;
    exec_path_ss << por_info_path_ << '/' << std::dec << exec_id;
    Execution *exec = new Execution;
    exec->Load(exec_path_ss.str().c_str(), sinfo(), program());
    loaded_execs_[exec_id] = exec;
    return exec;
  } else {
    return it->second;
  }
}

void ChessScheduler::PorLoad() {
  DEBUG_ASSERT(por_enable_);

  // prepare the directory for por
  PorPrepareDir();

  // load info from file
  ChessPorProto info_proto;
  std::stringstream por_info_path_ss;
  por_info_path_ss << por_info_path_ << "/info";
  std::fstream in(por_info_path_ss.str().c_str(),
                  std::ios::in | std::ios::binary);
  if (in.is_open())
    info_proto.ParseFromIstream(&in);
  in.close();
  curr_exec_id_ = info_proto.num_execs() + 1; // initially is zero
  for (int i = 0; i < info_proto.visited_state_size(); i++) {
    ChessPorProto::VisitedStateProto *proto
        = info_proto.mutable_visited_state(i);
    VisitedState *vs = new VisitedState;
    vs->hash_val = proto->hash_val();
    vs->preemptions = proto->preemptions();
    vs->exec_id = proto->exec_id();
    vs->state_idx = proto->state_idx();
    visited_states_[vs->hash_val].push_back(vs);
  }
}

void ChessScheduler::PorSave() {
  DEBUG_ASSERT(por_enable_);

  // prepare the directory for por
  PorPrepareDir();

  // save info to file
  ChessPorProto info_proto;
  std::stringstream por_info_path_ss;
  por_info_path_ss << por_info_path_ << "/info";
  info_proto.set_num_execs(curr_exec_id_);
  for (VisitedState::HashMap::iterator hit = visited_states_.begin();
       hit != visited_states_.end(); ++hit) {
    for (VisitedState::Vec::iterator vit = hit->second.begin();
         vit != hit->second.end(); ++vit) {
      VisitedState *vs = *vit;
      ChessPorProto::VisitedStateProto *proto = info_proto.add_visited_state();
      proto->set_hash_val(vs->hash_val);
      proto->set_preemptions(vs->preemptions);
      proto->set_exec_id(vs->exec_id);
      proto->set_state_idx(vs->state_idx);
    }
  }
  for (VisitedState::Vec::iterator vit = curr_visited_states_.begin();
       vit != curr_visited_states_.end(); ++vit) {
    VisitedState *vs = *vit;
    ChessPorProto::VisitedStateProto *proto = info_proto.add_visited_state();
    proto->set_hash_val(vs->hash_val);
    proto->set_preemptions(vs->preemptions);
    proto->set_exec_id(vs->exec_id);
    proto->set_state_idx(vs->state_idx);
  }
  std::fstream out(por_info_path_ss.str().c_str(),
                   std::ios::out | std::ios::trunc | std::ios::binary);
  info_proto.SerializeToOstream(&out);
  out.close();

  // save the current execution to file
  std::stringstream exec_path_ss;
  exec_path_ss << por_info_path_ << '/' << std::dec << curr_exec_id_;
  execution()->Save(exec_path_ss.str().c_str(), sinfo(), program());
}

void ChessScheduler::PorPrepareDir() {
  // check whether the path exists. if not, create it
  struct stat sb;
  if (stat(por_info_path_.c_str(), &sb)) {
    // the dir does not exists, create it
    int res = mkdir(por_info_path_.c_str(), 0755);
    assert(!res);
  } else {
    assert(S_ISDIR(sb.st_mode)); // it should be a directory
  }
}

} // namespace systematic

