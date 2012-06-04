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

def program_pb2():
    return proto.module('systematic.program_pb2')

def operation_name(op):
    d = program_pb2().ActionProto.DESCRIPTOR.fields_by_name['op'].enum_type
    return d.values_by_number[op].name[3:]

class Thread(object):
    def __init__(self, proto, program):
        self.proto = proto
        self.program = program
    def uid(self):
        return self.proto.uid
    def creator(self):
        if not self.proto.HasField('creator_uid'):
            return None
        else:
            return self.program.find_thread(self.proto.creator_uid)
    def creator_idx(self):
        if not self.proto.HasField('creator_idx'):
            return None
        else:
            return self.proto.creator_idx
    def __str__(self):
        content = []
        content.append('%-4d' % self.uid())
        if self.creator() == None:
            content.append('NULL')
            content.append('NULL')
        else:
            content.append('%-4d' % self.creator().uid())
            content.append('%-4d' % self.creator_idx())
        return ' '.join(content)

class Object(object):
    def __init__(self, proto, program):
        self.proto = proto
        self.program = program
        self.sinfo = self.program.sinfo
    def uid(self):
        return self.proto.uid
    def __str__(self):
        return ''

class SObject(Object):
    def __init__(self, proto, program):
        Object.__init__(self, proto, program)
    def image(self):
        return self.sinfo.find_image(self.proto.image_id)
    def offset(self):
        return self.proto.offset
    def __str__(self):
        content = []
        content.append('%-5d' % self.uid())
        content.append('S ')
        content.append('%-10s' % self.image().shortname())
        content.append('0x%-6x' % self.offset())
        return ' '.join(content)

class DObject(Object):
    def __init__(self, proto, program):
        Object.__init__(self, proto, program)
    def creator(self):
        return self.program.find_thread(self.proto.creator_uid)
    def creator_inst(self):
        return self.sinfo.find_inst(self.proto.creator_inst_id)
    def creator_idx(self):
        return self.proto.creator_idx
    def offset(self):
        return self.proto.offset
    def __str__(self):
        content = []
        content.append('%-6d' % self.uid())
        content.append('D ')
        content.append('%-4d' % self.creator().uid())
        content.append('[%-40s]' % str(self.creator_inst()))
        content.append('%-4d' % self.creator_idx())
        content.append('0x%-6x' % self.offset())
        return ' '.join(content)

class Program(object):
    def __init__(self, sinfo):
        self.sinfo = sinfo
        self.proto = program_pb2().ProgramProto()
        self.thread_map = {}
        self.object_map = {}
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for thd_proto in self.proto.thread:
            thd = Thread(thd_proto, self)
            self.thread_map[thd.uid()] = thd
        for sobj_proto in self.proto.sobject:
            sobj = SObject(sobj_proto, self)
            self.object_map[sobj.uid()] = sobj
        for dobj_proto in self.proto.dobject:
            dobj = DObject(dobj_proto, self)
            self.object_map[dobj.uid()] = dobj
    def find_thread(self, uid):
        return self.thread_map[uid]
    def find_object(self, uid):
        return self.object_map[uid]
    def display_thread_table(self, f):
        for thd in self.thread_map.itervalues():
            f.write(str(thd))
            f.write('\n')
    def display_object_table(self, f):
        for obj in self.object_map.itervalues():
            f.write(str(obj))
            f.write('\n')

class Action(object):
    def __init__(self, proto, execution):
        self.proto = proto
        self.execution = execution
        self.sinfo = self.execution.sinfo
        self.program = self.execution.program
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
    def tc(self):
        if not self.proto.HasField('tc'):
            return None
        else:
            return self.proto.tc
    def oc(self):
        if not self.proto.HasField('oc'):
            return None
        else:
            return self.proto.oc

class State(object):
    def __init__(self, proto, execution):
        self.proto = proto
        self.execution = execution
    def enabled(self, idx):
        return self.execution.find_action(self.proto.enabled[idx])
    def taken(self):
        if not self.proto.HasField('taken'):
            return None
        else:
            return self.execution.find_action(self.proto.taken)

class Execution(object):
    def __init__(self, sinfo, program):
        self.sinfo = sinfo
        self.program = program
        self.action_vec = []
        self.state_vec = []
    def load(self, db_name):
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for action_proto in self.proto.action:
            action = Action(action_proto, self)
            action_vec.append(action)
        for state_proto in self.proto.state:
            state = State(state_proto, self)
            state_vec.append(state)
    def find_action(self, idx):
        return self.action_vec[idx]
    def find_state(self, idx):
        return self.state_vec[idx]

