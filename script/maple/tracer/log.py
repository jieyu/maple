"""Copyright 2011 The University of Michigan

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Authors - Jie Yu (jieyu@umich.edu)
"""

import os
from maple.core import proto

def log_pb2():
    return proto.module('tracer.log_pb2')

def log_entry_type_name(t):
    d = log_pb2().LogEntryProto.DESCRIPTOR.fields_by_name['type'].enum_type
    return d.values_by_number[t].name[10:]

class LogEntry(object):
    def __init__(self, proto, log):
        self.proto = proto
        self.log = log
    def type(self):
        return self.proto.type
    def thd_id(self):
        if self.proto.HasField('thd_id'):
            return self.proto.thd_id
        else:
            return None
    def thd_clk(self):
        if self.proto.HasField('thd_clk'):
            return self.proto.thd_clk
        else:
            return None
    def inst(self):
        if self.proto.HasField('inst_id'):
            return self.log.sinfo.find_inst(self.proto.inst_id)
        else:
            return None
    def arg(self, idx):
        return self.proto.arg[idx]
    def num_args(self):
        return len(self.proto.arg)
    def str_arg(self, idx):
        return self.proto.str_arg[idx]
    def num_str_args(self):
        return len(self.proto.str_arg)
    def __str__(self):
        content = []
        content.append('%-27s' % log_entry_type_name(self.type()))
        tid = self.thd_id()
        clk = self.thd_clk()
        ins = self.inst()
        if tid != None:
            content.append('T%x' % tid)
        if clk != None:
            content.append('%08x' % clk)
        if ins != None:
            content.append('[%-40s]' % str(ins))
        for p in self.proto.arg:
            content.append('%x' % p)
        for p in self.proto.str_arg:
            content.append('%s' % p)
        return ' '.join(content)

class TraceLog(object):
    def __init__(self, sinfo):
        self.sinfo = sinfo
        self.meta = log_pb2().LogMetaProto()
        self.slice = log_pb2().LogSliceProto()
        self.mode = None
        self.path = None
        self.entry_cursor = 0
        self.has_next = False
    def open_for_read(self, path):
        if not os.path.isdir(path):
            return False
        meta_path = path + '/meta'
        slice_path = path + '/1'
        if not os.path.exists(meta_path):
            return False
        if not os.path.exists(slice_path):
            return False
        # read meta data
        f = open(meta_path, 'rb')
        self.meta.ParseFromString(f.read())
        f.close()
        # read log slice
        f = open(slice_path, 'rb')
        self.slice.ParseFromString(f.read())
        f.close()
        # setup
        self.mode = 'READ'
        self.path = path
        self.entry_cursor = 0
        if len(self.slice.entry) > 0:
            self.has_next = True
        else:
            self.has_next = False
        return True
    def close_for_read(self):
        assert self.mode == 'READ'
        self.slice.Clear()
        self.meta.Clear()
        self.mode = None
        self.path = None
        self.entry_cursor = 0
        self.has_next = False
    def has_next_entry(self):
        assert self.mode == 'READ'
        if not self.has_next:
            self.switch_slice_for_read()
        return self.has_next
    def next_entry(self):
        assert self.mode == 'READ'
        assert self.has_next
        entry_proto = self.slice.entry[self.entry_cursor]
        entry = LogEntry(entry_proto, self)
        self.entry_cursor += 1
        if self.entry_cursor == len(self.slice.entry):
            self.has_next = False
        return entry
    def switch_slice_for_read(self):
        assert self.mode == 'READ'
        curr_slice_no = self.slice.slice_no
        next_slice_no = curr_slice_no + 1
        self.slice.Clear()
        slice_path = self.path + '/%d' % next_slice_no
        if not os.path.exists(slice_path):
            self.has_next = False
        else:
            f = open(slice_path, 'rb')
            self.slice.ParseFromString(f.read())
            f.close()
            assert len(self.slice.entry) > 0
            self.entry_cursor = 0
            self.has_next = True
    def display(self, f, path):
        self.open_for_read(path)
        while self.has_next_entry():
            entry = self.next_entry()
            f.write('%s\n' % str(entry))
        self.close_for_read()

