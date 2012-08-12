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

_memo_failed_limit = 2
_memo_total_failed_limit = 6

def iroot_pb2():
    return proto.module('idiom.iroot_pb2')

def memo_pb2():
    return proto.module('idiom.memo_pb2')

class iRootInfo(object):
    def __init__(self, proto, memo):
        self.proto = proto
        self.memo = memo
    def iroot(self):
        return self.memo.iroot_db.find_iroot(self.proto.iroot_id)
    def total_test_runs(self):
        return self.proto.total_test_runs
    def has_async(self):
        return self.proto.HasField('async')
    def async(self):
        return self.proto.async
    def set_total_test_runs(self, n):
        self.proto.total_test_runs = n
    def set_async(self, a):
        self.proto.async = a
    def __str__(self):
        content = []
        content.append(str(self.iroot()))
        content.append('  total_test_runs = %2d, async = %d' % \
                       self.total_test_runs(),
                       self.async())
        return ''.join(content)

class Memo(object):
    def __init__(self, sinfo, iroot_db):
        self.sinfo = sinfo
        self.iroot_db = iroot_db
        self.proto = memo_pb2().MemoProto()
        self.iroot_info_map = {}
        self.exposed_set = set()
        self.failed_set = set()
        self.predicted_set = set()
        self.shadow_exposed_set = set()
        self.candidate_map = {}
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for iroot_info_proto in self.proto.iroot_info:
            iroot_info = iRootInfo(iroot_info_proto, self)
            self.iroot_info_map[iroot_info.iroot().id()] = iroot_info
        for iroot_id in self.proto.exposed:
            iroot = self.iroot_db.find_iroot(iroot_id)
            iroot_info = self.find_iroot_info(iroot)
            self.exposed_set.add(iroot_info)
        for iroot_id in self.proto.failed:
            iroot = self.iroot_db.find_iroot(iroot_id)
            iroot_info = self.find_iroot_info(iroot)
            self.failed_set.add(iroot_info)
        for iroot_id in self.proto.predicted:
            iroot = self.iroot_db.find_iroot(iroot_id)
            iroot_info = self.find_iroot_info(iroot)
            self.predicted_set.add(iroot_info)
        for iroot_id in self.proto.shadow_exposed:
            iroot = self.iroot_db.find_iroot(iroot_id)
            iroot_info = self.find_iroot_info(iroot)
            self.shadow_exposed_set.add(iroot_info)
        for cand_proto in self.proto.candidate:
            iroot = self.iroot_db.find_iroot(cand_proto.iroot_id)
            iroot_info = self.find_iroot_info(iroot)
            self.candidate_map[iroot_info] = cand_proto.test_runs
    def save(self, db_name):
        del self.proto.iroot_info[:]
        del self.proto.exposed[:]
        del self.proto.failed[:]
        del self.proto.predicted[:]
        del self.proto.shadow_exposed[:]
        del self.proto.candidate[:]
        for iroot_id, iroot_info in self.iroot_info_map.iteritems():
            iroot_info_proto = self.proto.iroot_info.add()
            iroot_info_proto.iroot_id = iroot_id
            iroot_info_proto.total_test_runs = iroot_info.total_test_runs()
            if (iroot_info.has_async()):
                iroot_info_proto.async = iroot_info.async()
        for iroot_info in self.exposed_set:
            self.proto.exposed.append(iroot_info.iroot().id())
        for iroot_info in self.failed_set:
            self.proto.failed.append(iroot_info.iroot().id())
        for iroot_info in self.predicted_set:
            self.proto.predicted.append(iroot_info.iroot().id())
        for iroot_info in self.shadow_exposed_set:
            self.proto.shadow_exposed.append(iroot_info.iroot().id())
        for iroot_info, test_runs in self.candidate_map.iteritems():
            cand_proto = self.proto.candidate.add()
            cand_proto.iroot_id = iroot_info.iroot().id()
            cand_proto.test_runs = test_runs
        f = open(db_name, 'wb')
        f.write(self.proto.SerializeToString())
        f.close()
    def size(self):
        return len(self.iroot_info_map)
    def predicted_size(self):
        return len(self.predicted_set)
    def candidate_size(self):
        return len(self.candidate_map)
    def clear_predicted_set(self):
        self.predicted_set = set()
    def clear_candidate_map(self):
        self.candidate_map = {}
    def has_candidate(self, idiom):
        if idiom == 0:
            return len(self.candidate_map) > 0
        for iroot_info in self.candidate_map:
            if iroot_info.iroot().idiom() == idiom:
                return True
        return False
    def refine_candidate(self, memo_failed=True):
        to_remove = []
        for iroot_info, test_runs in self.candidate_map.iteritems():
            if test_runs >= _memo_failed_limit:
                to_remove.append(iroot_info)
        for iroot_info in to_remove:
            del self.candidate_map[iroot_info]
        for iroot_info in self.exposed_set:
            if iroot_info in self.candidate_map:
                del self.candidate_map[iroot_info]
        if memo_failed:
            for iroot_info in self.failed_set:
                if iroot_info in self.candidate_map:
                    del self.candidate_map[iroot_info]
    def mark_unexposed_failed(self):
        for iroot_id, iroot_info in self.iroot_info_map.iteritems():
            if not iroot_info in self.exposed_set:
                self.failed_set.add(iroot_info)
    def merge(self, other):
        for other_iroot_id, other_iroot_info in other.iroot_info_map.iteritems():
            if other_iroot_id in self.iroot_info_map:
                iroot_info = self.iroot_info_map[other_iroot_id]
                if other_iroot_info.total_test_runs() > iroot_info.total_test_runs():
                    iroot_info.set_total_test_runs(other_iroot_info.total_test_runs())
                if other_iroot_info.has_async() and other_iroot_info.async():
                    iroot_info.set_async(True)
            else:
                self.iroot_info_map[other_iroot_id] = other_iroot_info
        for other_iroot_info in other.exposed_set:
            assert other_iroot_info.iroot().id() in self.iroot_info_map
            iroot_info = self.iroot_info_map[other_iroot_info.iroot().id()]
            self.exposed_set.add(iroot_info)
        for other_iroot_info in other.failed_set:
            assert other_iroot_info.iroot().id() in self.iroot_info_map
            iroot_info = self.iroot_info_map[other_iroot_info.iroot().id()]
            self.failed_set.add(iroot_info)
        for other_iroot_info in other.predicted_set:
            assert other_iroot_info.iroot().id() in self.iroot_info_map
            iroot_info = self.iroot_info_map[other_iroot_info.iroot().id()]
            self.predicted_set.add(iroot_info)
        for other_iroot_info in other.shadow_exposed_set:
            assert other_iroot_info.iroot().id() in self.iroot_info_map
            iroot_info = self.iroot_info_map[other_iroot_info.iroot().id()]
            self.shadow_exposed_set.add(iroot_info)
        for other_iroot_info, test_runs in other.candidate_map.iteritems():
            assert other_iroot_info.iroot().id() in self.iroot_info_map
            iroot_info = self.iroot_info_map[other_iroot_info.iroot().id()]
            if iroot_info in self.candidate_map:
                if test_runs > self.candidate_map[iroot_info]:
                    self.candidate_map[iroot_info] = test_runs
            else:
                self.candidate_map[iroot_info] = test_runs
    def test_success(self, iroot_info):
        self.candidate_map[iroot_info] += 1
        iroot_info.set_total_test_runs(iroot_info.total_test_runs() + 1)
        self.exposed_set.add(iroot_info)
    def test_fail(self, iroot_info):
        self.candidate_map[iroot_info] += 1
        iroot_info.set_total_test_runs(iroot_info.total_test_runs() + 1)
        if iroot_info.total_test_runs() >= _memo_total_failed_limit:
            self.failed_set.add(iroot_info)
    def find_iroot_info(self, iroot):
        return self.iroot_info_map[iroot.id()]
    def get_exposed_set(self, idiom):
        results = set()
        for iroot_info in self.exposed_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        return results
    def get_failed_set(self, idiom):
        results = set()
        for iroot_info in self.failed_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        return results
    def get_predicted_set(self, idiom):
        results = set()
        for iroot_info in self.predicted_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        return results
    def get_shadow_exposed_set(self, idiom):
        results = set()
        for iroot_info in self.shadow_exposed_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        return results
    def get_candidates(self, idiom):
        results = {}
        for iroot_info, test_runs in self.candidate_map.iteritems():
            if iroot_info.iroot().idiom() == idiom:
                results[iroot_info] = test_runs
        return results
    def observed_iroot_set(self, idiom):
        results = set()
        for iroot_info in self.exposed_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        for iroot_info in self.shadow_exposed_set:
            if iroot_info.iroot().idiom() == idiom:
                results.add(iroot_info)
        return results
    def display_summary(self, f):
        f.write('Memoization Summary\n')
        f.write('---------------------------\n')
        f.write('# total exposed    = %d\n' % len(self.exposed_set))
        f.write('  # Idiom1         = %d\n' % len(self.get_exposed_set(iroot_pb2().IDIOM_1)))
        f.write('  # Idiom2         = %d\n' % len(self.get_exposed_set(iroot_pb2().IDIOM_2)))
        f.write('  # Idiom3         = %d\n' % len(self.get_exposed_set(iroot_pb2().IDIOM_3)))
        f.write('  # Idiom4         = %d\n' % len(self.get_exposed_set(iroot_pb2().IDIOM_4)))
        f.write('  # Idiom5         = %d\n' % len(self.get_exposed_set(iroot_pb2().IDIOM_5)))
        f.write('# total failed     = %d\n' % len(self.failed_set))
        f.write('  # Idiom1         = %d\n' % len(self.get_failed_set(iroot_pb2().IDIOM_1)))
        f.write('  # Idiom2         = %d\n' % len(self.get_failed_set(iroot_pb2().IDIOM_2)))
        f.write('  # Idiom3         = %d\n' % len(self.get_failed_set(iroot_pb2().IDIOM_3)))
        f.write('  # Idiom4         = %d\n' % len(self.get_failed_set(iroot_pb2().IDIOM_4)))
        f.write('  # Idiom5         = %d\n' % len(self.get_failed_set(iroot_pb2().IDIOM_5)))
        f.write('# total predicted  = %d\n' % len(self.predicted_set))
        f.write('  # Idiom1         = %d\n' % len(self.get_predicted_set(iroot_pb2().IDIOM_1)))
        f.write('  # Idiom2         = %d\n' % len(self.get_predicted_set(iroot_pb2().IDIOM_2)))
        f.write('  # Idiom3         = %d\n' % len(self.get_predicted_set(iroot_pb2().IDIOM_3)))
        f.write('  # Idiom4         = %d\n' % len(self.get_predicted_set(iroot_pb2().IDIOM_4)))
        f.write('  # Idiom5         = %d\n' % len(self.get_predicted_set(iroot_pb2().IDIOM_5)))
        f.write('# total shadow ex  = %d\n' % len(self.shadow_exposed_set))
        f.write('  # Idiom1         = %d\n' % len(self.get_shadow_exposed_set(iroot_pb2().IDIOM_1)))
        f.write('  # Idiom2         = %d\n' % len(self.get_shadow_exposed_set(iroot_pb2().IDIOM_2)))
        f.write('  # Idiom3         = %d\n' % len(self.get_shadow_exposed_set(iroot_pb2().IDIOM_3)))
        f.write('  # Idiom4         = %d\n' % len(self.get_shadow_exposed_set(iroot_pb2().IDIOM_4)))
        f.write('  # Idiom5         = %d\n' % len(self.get_shadow_exposed_set(iroot_pb2().IDIOM_5)))
        f.write('# total candidates = %d\n' % len(self.candidate_map))
        f.write('  # Idiom1         = %d\n' % len(self.get_candidates(iroot_pb2().IDIOM_1)))
        f.write('  # Idiom2         = %d\n' % len(self.get_candidates(iroot_pb2().IDIOM_2)))
        f.write('  # Idiom3         = %d\n' % len(self.get_candidates(iroot_pb2().IDIOM_3)))
        f.write('  # Idiom4         = %d\n' % len(self.get_candidates(iroot_pb2().IDIOM_4)))
        f.write('  # Idiom5         = %d\n' % len(self.get_candidates(iroot_pb2().IDIOM_5)))
        f.write('# total iroot_info = %d\n' % len(self.iroot_info_map))
    def display_exposed_set(self, f):
        for iroot_info in self.exposed_set:
            f.write('%s\n' % str(iroot_info.iroot()))

