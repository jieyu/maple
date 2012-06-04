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
from maple.idiom import iroot

def history_pb2():
    return proto.module('idiom.history_pb2')

def test_result_str(success):
    if success == None:
        return 'None'
    elif success:
        return 'Success'
    else:
        return 'Fail'

class TestHistoryEntry(object):
    def __init__(self, proto, db):
        self.proto = proto
        self.db = db
    def iroot(self):
        return self.db.iroot_db.find_iroot(self.proto.iroot_id)
    def seed(self):
        return self.proto.seed
    def success(self):
        if self.proto.HasField('success'):
            return self.proto.success
        else:
            return None
    def __str__(self):
        content = []
        content.append('%-5d' % self.iroot().id())
        content.append('%-7s' % iroot.idiom_type_name(self.iroot().idiom()))
        content.append('%-7s' % test_result_str(self.success()))
        content.append('%d' % self.seed())
        return ' '.join(content)

class TestHistory(object):
    def __init__(self, sinfo, iroot_db):
        self.sinfo = sinfo
        self.iroot_db = iroot_db
        self.proto = history_pb2().HistoryTableProto()
        self.history = []
    def load(self, histo_name):
        if not os.path.exists(histo_name):
            return
        f = open(histo_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for history_proto in self.proto.history:
            entry = TestHistoryEntry(history_proto, self)
            self.history.append(entry)
    def num_success(self, idiom):
        succ_count = 0
        for entry in self.history:
            if entry.iroot().idiom() == idiom:
                if entry.success() != None:
                    if entry.success():
                        succ_count += 1
        return succ_count
    def num_fail(self, idiom):
        fail_count = 0
        for entry in self.history:
            if entry.iroot().idiom() == idiom:
                if entry.success() != None:
                    if not entry.success():
                        fail_count += 1
        return fail_count
    def num_iroot(self, idiom):
        iroot_id_set = set()
        for entry in self.history:
            if entry.iroot().idiom() == idiom:
                iroot_id_set.add(entry.iroot().id())
        return len(iroot_id_set)
    def display(self, f):
        for entry in self.history:
            f.write('%s\n' % str(entry))
    def display_summary(self, f):
        f.write('Test History Summary\n')
        f.write('---------------------------\n')
        for idiom in range(1,6):
            num_success = self.num_success(idiom)
            num_fail = self.num_fail(idiom)
            num_iroot = self.num_iroot(idiom)
            f.write('# Idiom%d tests      = %d\n' % (idiom, num_success + num_fail))
            f.write('  # succ            = %d\n' % num_success)
            f.write('  # fail            = %d\n' % num_fail)
            f.write('  # succ iroot      = %d\n' % num_success)
            f.write('  # fail iroot      = %d\n' % (num_iroot - num_success))

