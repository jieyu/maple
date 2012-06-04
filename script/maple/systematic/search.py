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
from maple.systematic import program

def search_pb2():
    return proto.module('systematic.search_pb2')

class ActionInfo(object):
    def __init__(self, proto, node):
        self.proto = proto
        self.node = node
        self.sinfo = self.node.info.sinfo
        self.program = self.node.info.program
    def thd(self):
        return self.program.find_thread(self.proto.thd_uid)
    def obj(self):
        if not self.proto.HasField('obj_uid'):
            return None
        else:
            return self.program.find_object(self.proto.obj_uid)
    def op(self):
        return self.proto.op
    def inst(self):
        if not self.proto.HasField('inst_id'):
            return None
        else:
            return self.sinfo.find_inst(self.proto.inst_id)
    def __str__(self):
        content = []
        content.append('%-4d' % self.thd().uid())
        if self.obj() == None:
            content.append('NULL  ')
        else:
            content.append('%-6d' % self.obj().uid())
        content.append('%-14s' % program.operation_name(self.op()))
        if self.inst() == None:
            content.append('NULL')
        else:
            content.append('[%s]' % str(self.inst()))
        return ' '.join(content)

class SearchNode(object):
    def __init__(self, proto, info):
        self.proto = proto
        self.info = info
        self.program = self.info.program
        self.enabled_vec = []
        for enabled_proto in self.proto.enabled:
            action_info = ActionInfo(enabled_proto, self)
            self.enabled_vec.append(action_info)
    def sel(self):
        return self.program.find_thread(self.proto.sel)
    def backtrack(self, idx):
        return self.program.find_thread(self.proto.backtrack[idx])
    def done(self, idx):
        return self.program.find_thread(self.proto.done[idx])
    def enabled(self, idx):
        return self.enabled_vec[idx]
    def num_backtrack(self):
        return len(self.proto.backtrack)
    def num_done(self):
        return len(self.proto.done)
    def num_enabled(self):
        return len(self.enabled_vec)
    def __str__(self):
        content = []
        content.append('sel: %d\n' % self.sel().uid())
        content.append('backtrack: ')
        for idx in range(len(self.proto.backtrack)):
            content.append('%d ' % self.backtrack(idx).uid())
        content.append('\n')
        content.append('done: ')
        for idx in range(len(self.proto.done)):
            content.append('%d ' % self.done(idx).uid())
        content.append('\n')
        content.append('enabled:\n')
        for idx in range(len(self.proto.enabled)):
            content.append('   %s\n' % str(self.enabled(idx)))
        return ''.join(content)

class SearchInfo(object):
    def __init__(self, sinfo, prog):
        self.sinfo = sinfo
        self.program = prog
        self.proto = search_pb2().SearchInfoProto()
        self.node_vec = []
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for node_proto in self.proto.node:
            node = SearchNode(node_proto, self)
            self.node_vec.append(node)
    def done(self):
        return self.proto.done
    def num_runs(self):
        return self.proto.num_runs
    def num_nodes(self):
        return len(self.node_vec)
    def find_node(self, idx):
        return self.node_vec[idx]
    def display(self, f):
        for idx in range(len(self.node_vec)):
            node = self.node_vec[idx]
            f.write('------------\n')
            f.write('| Node %3d |\n' % idx)
            f.write('------------\n')
            f.write(str(node))
        f.write('------------\n')
        f.write('| Summary  |\n')
        f.write('------------\n')
        f.write('done     = %d\n' % self.done())
        f.write('num_runs = %d\n' % self.num_runs())

