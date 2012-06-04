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

// File: tracer/log.h - Define trace log.

#ifndef TRACER_LOG_H_
#define TRACER_LOG_H_

#include <fstream>

#include "core/basictypes.h"
#include "core/static_info.h"
#include "core/sync.h"
#include "tracer/log.pb.h"

namespace tracer {

class TraceLog;

class LogEntry {
 public:
  ~LogEntry() {}

  LogEntryType type() { return proto_->type(); }

  thread_id_t thd_id() {
    if (proto_->has_thd_id())
      return proto_->thd_id();
    else
      return INVALID_THD_ID;
  }

  timestamp_t thd_clk() {
    if (proto_->has_thd_clk())
      return proto_->thd_clk();
    else
      return 0;
  }

  inst_id_type inst_id() {
    if (proto_->has_inst_id())
      return proto_->inst_id();
    else
      return INVALID_INST_ID;
  }

  address_t arg(int index) {
    if (index >= 0 && index < proto_->arg_size())
      return proto_->arg(index);
    else
      return 0;
  }

  std::string str_arg(int index) {
    if (index >= 0 && index < proto_->str_arg_size())
      return proto_->str_arg(index);
    else
      return std::string();
  }

  void set_type(LogEntryType type) { proto_->set_type(type); }
  void set_thd_id(thread_id_t thd_id) { proto_->set_thd_id(thd_id); }
  void set_thd_clk(timestamp_t thd_clk) { proto_->set_thd_clk(thd_clk); }
  void set_inst_id(inst_id_type inst_id) { proto_->set_inst_id(inst_id); }
  void add_arg(address_t val) { proto_->add_arg(val); }
  void set_arg(int index, address_t val) { proto_->set_arg(index, val); }
  void add_str_arg(std::string &val) { proto_->add_str_arg(val); }

  void set_str_arg(int index, std::string &val) {
    proto_->set_str_arg(index, val);
  }

 protected:
  explicit LogEntry(LogEntryProto *proto) : proto_(proto) {}

  LogEntryProto *proto_;

 private:
  friend class TraceLog;
};

typedef uint64 trace_log_uid_t;

class TraceLog {
 public:
  TraceLog(const std::string &path);
  ~TraceLog() {}

  void OpenForRead();
  void OpenForWrite();
  void CloseForRead();
  void CloseForWrite();
  bool HasNextEntry();
  LogEntry NextEntry();
  LogEntry NewEntry();

 protected:
  typedef enum {
    OP_MODE_INVALID = 0,
    OP_MODE_READ,
    OP_MODE_WRITE,
  } OpMode;

  trace_log_uid_t GenUid();
  void SwitchSliceForRead();
  void SwitchSliceForWrite();
  void PrepareDirForRead();
  void PrepareDirForWrite();

  std::string path_;
  OpMode mode_;
  LogMetaProto *meta_;
  LogSliceProto *curr_slice_;
  int entry_cursor_;
  bool has_next_;

 private:
  DISALLOW_COPY_CONSTRUCTORS(TraceLog);
};

} // namespace tracer

#endif

