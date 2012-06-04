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

def race_pb2():
    return proto.module('race.race_pb2')

class StaticRaceEvent(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
        self.sinfo = db.sinfo
    def id(self):
        return self.proto.id
    def inst(self):
        return self.sinfo.find_inst(self.proto.inst_id)
    def type(self):
        return self.proto.type
    def type_name(self):
        if self.type() == race_pb2().RACE_EVENT_READ:
            return 'READ'
        elif self.type() == race_pb2().RACE_EVENT_WRITE:
            return 'WRITE'
        else:
            return 'INVALID'
    def __str__(self):
        content = []
        content.append('%-4d' % self.id())
        content.append('%-7s' % self.type_name())
        content.append('[%-40s]' % str(self.inst()))
        return ' '.join(content)

class StaticRace(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
        self.sinfo = db.sinfo
    def id(self):
        return self.proto.id
    def event(self, idx):
        event_id = self.proto.event_id[idx]
        return self.db.find_static_event(event_id)
    def num_events(self):
        return len(self.proto.event_id)
    def __str__(self):
        content = []
        content.append('Static Race %-4d' % self.id())
        for idx in range(self.num_events()):
            content.append('  %s' % str(self.event(idx)))
        return '\n'.join(content)

class RaceEvent(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
    def thd_id(self):
        return self.proto.thd_id
    def static_event(self):
        return self.db.find_static_event(self.proto.static_id)
    def __str__(self):
        content = []
        content.append('[T%lx]' % self.thd_id())
        content.append('%s' % str(self.static_event()))
        return ' '.join(content)

class Race(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
        self.event_vec = []
        for e_proto in self.proto.event:
            e = RaceEvent(e_proto, db)
            self.event_vec.append(e)
    def exec_id(self):
        return self.proto.exec_id
    def addr(self):
        return self.proto.addr
    def event(self, idx):
        return self.event_vec[idx]
    def num_events(self):
        return len(self.proto.event)
    def static_race(self):
        return self.db.find_static_race(self.proto.static_id)
    def __str__(self):
        content = []
        content.append('Dynamic Race: %-4d 0x%-8x' % (self.exec_id(), self.addr()))
        for idx in range(self.num_events()):
            content.append('  %s' % self.event(idx))
        return '\n'.join(content)

class RaceDB(object):
    def __init__(self, sinfo):
        self.sinfo = sinfo
        self.proto = race_pb2().RaceDBProto()
        self.static_event_map = {}
        self.static_race_map = {}
        self.race_vec = []
        self.racy_inst_set = set()
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for e_proto in self.proto.static_event:
            e = StaticRaceEvent(e_proto, self)
            self.static_event_map[e.id()] = e
        for r_proto in self.proto.static_race:
            r = StaticRace(r_proto, self)
            self.static_race_map[r.id()] = r
        for r_proto in self.proto.race:
            r = Race(r_proto, self)
            self.race_vec.append(r)
        for inst_id in self.proto.racy_inst_id:
            inst = self.sinfo.find_inst(inst_id)
            self.racy_inst_set.add(inst)
    def num_static_races(self):
        return len(self.proto.static_race)
    def num_racy_insts(self):
        return len(self.proto.racy_inst_id)
    def find_static_event(self, id):
        return self.static_event_map[id]
    def find_static_race(self, id):
        return self.static_race_map[id]
    def display_static_race(self, f):
        for r in self.static_race_map.itervalues():
            f.write('%s\n' % str(r))
    def display_race(self, f):
        for r in self.race_vec:
            f.write('%s\n' % str(r))
    def display_racy_inst(self, f):
        for inst in self.racy_inst_set:
            f.write('%s\n' % str(inst))

