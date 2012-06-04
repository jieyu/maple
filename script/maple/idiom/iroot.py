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
from maple.core import logging
from maple.core import proto

def iroot_pb2():
    return proto.module('idiom.iroot_pb2')

def idiom_type_name(t):
    if t == iroot_pb2().IDIOM_1:
        return 'IDIOM_1'
    elif t == iroot_pb2().IDIOM_2:
        return 'IDIOM_2'
    elif t == iroot_pb2().IDIOM_3:
        return 'IDIOM_3'
    elif t == iroot_pb2().IDIOM_4:
        return 'IDIOM_4'
    elif t == iroot_pb2().IDIOM_5:
        return 'IDIOM_5'
    else:
        return 'INVALID'

def iroot_event_type_name(t):
    if t == iroot_pb2().IROOT_EVENT_MEM_READ:
        return 'READ'
    elif t == iroot_pb2().IROOT_EVENT_MEM_WRITE:
        return 'WRITE'
    elif t == iroot_pb2().IROOT_EVENT_MUTEX_LOCK:
        return 'LOCK'
    elif t == iroot_pb2().IROOT_EVENT_MUTEX_UNLOCK:
        return 'UNLOCK'
    else:
        return 'INVALID'

class iRootEvent(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
    def id(self):
        return self.proto.id
    def inst(self):
        return self.db.sinfo.find_inst(self.proto.inst_id)
    def type(self):
        return self.proto.type
    def is_mem_read(self):
        return self.type() == iroot_pb2().IROOT_EVENT_MEM_READ
    def is_mem_write(self):
        return self.type() == iroot_pb2().IROOT_EVENT_MEM_WRITE
    def is_mutex_lock(self):
        return self.type() == iroot_pb2().IROOT_EVENT_MUTEX_LOCK
    def is_mutex_unlock(self):
        return self.type() == iroot_pb2().IROOT_EVENT_MUTEX_UNLOCK
    def __str__(self):
        content = []
        content.append('%-7s' % iroot_event_type_name(self.type()))
        content.append('[%-40s]' % str(self.inst()))
        return ' '.join(content)

class iRoot(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
    def id(self):
        return self.proto.id
    def idiom(self):
        return self.proto.idiom
    def event(self, idx):
        event_id = self.proto.event_id[idx]
        return self.db.find_iroot_event(event_id)
    def __str__(self):
        content = []
        content.append('%-5d ' % self.id())
        content.append('%-7s' % idiom_type_name(self.idiom()))
        content.append('\n')
        for idx in range(len(self.proto.event_id)):
            e = self.event(idx)
            content.append('\t')
            content.append('e%d: %s' % (idx, str(e)))
            content.append('\n')
        return ''.join(content)

class iRootDB(object):
    def __init__(self, sinfo):
        self.sinfo = sinfo
        self.proto = iroot_pb2().iRootDBProto()
        self.event_map = {}
        self.iroot_map = {}
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for event_proto in self.proto.event:
            event = iRootEvent(event_proto, self)
            self.event_map[event.id()] = event
        for iroot_proto in self.proto.iroot:
            iroot = iRoot(iroot_proto, self)
            self.iroot_map[iroot.id()] = iroot
    def find_iroot_event(self, event_id):
        return self.event_map[event_id]
    def find_iroot(self, iroot_id):
        return self.iroot_map[iroot_id]
    def display(self, f):
        for iroot in self.iroot_map.itervalues():
            f.write(str(iroot))

