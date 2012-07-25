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

def history_pb2():
    return proto.module('pct.history_pb2')

class HistoryEntry(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
    def inst_count(self):
        return self.proto.inst_count
    def num_threads(self):
        return self.proto.num_threads
    def __str__(self):
        content = []
        content.append('%-4d' % self.num_threads())
        content.append('%d' % self.inst_count())
        return ' '.join(content)

class History(object):
    def __init__(self):
        self.proto = history_pb2().HistoryTableProto()
        self.history = []
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for history_proto in self.proto.history:
            entry = HistoryEntry(history_proto, self)
            self.history.append(entry)
    def avg_inst_count(self):
        total = 0.0
        for entry in self.history:
            total += entry.inst_count()
        return total / len(self.history)
    def avg_num_threads(self):
        total = 0.0
        for entry in self.history:
            total += entry.num_threads()
        return total / len(self.history)
    def display(self, f):
        for entry in self.history:
            f.write('%s\n' % str(entry))
    def display_summary(self, f):
        f.write('PCT History Summary\n')
        f.write('---------------------------\n')
        f.write('Avg inst count   = %f\n' % self.avg_inst_count())
        f.write('Avg num threads  = %f\n' % self.avg_num_threads())

