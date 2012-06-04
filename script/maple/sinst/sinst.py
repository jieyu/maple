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

def sinst_pb2():
    return proto.module('sinst.sinst_pb2')

class SinstDB(object):
    def __init__(self, sinfo):
        self.sinfo = sinfo
        self.proto = sinst_pb2().SharedInstTableProto()
        self.sinst_map = {}
    def load(self, db_name):
        if not os.path.exists(db_name):
            return
        f = open(db_name, 'rb')
        self.proto.ParseFromString(f.read())
        f.close()
        for sinst_proto in self.proto.shared_inst:
            sinst = self.sinfo.find_inst(sinst_proto.inst_id)
            self.sinst_map[sinst.id()] = sinst
    def display(self, f):
        for sinst in self.sinst_map.itervalues():
            f.write('%s\n' % str(sinst))

