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
import glob
from maple.core import config
from maple.core import testing

_sio = [None, os.devnull, 'stderr']

class Test(testing.CmdlineTest):
    def __init__(self, input_idx):
        testing.CmdlineTest.__init__(self, input_idx)
        self.add_input(([self.bin(), '-n2', 'jieyu'] + self.get_cc_files(config.pkg_home() + '/src/systematic'), _sio))
        self.add_input(([self.bin(), '-n3', 'Authors'] + self.get_cc_files(config.pkg_home() + '/src/systematic'), _sio))
        self.add_input(([self.bin(), '-n3', 'main'] + self.get_all_files(config.pkg_home() + '/src/pct'), _sio))
        self.add_input(([self.bin(), '-n2', 'Apache', config.pkg_home() + '/src/idiom/memo.cc'], _sio))
        self.add_input(([self.bin(), '-n2', 'get_benchmark'] + self.get_py_files(config.pkg_home() + '/script/maple/benchmark'), _sio))
        self.add_input(([self.bin(), '-n3', 'PreSetup'] + self.get_all_files(config.pkg_home() + '/src/core'), _sio))
        self.add_input(([self.bin(), '-n3', 'NO_EXIST'] + self.get_all_files(config.pkg_home() + '/src/idiom'), _sio))
        self.add_input(([self.bin(), '-n2', 'main'] + self.get_cc_files(config.pkg_home() + '/test/idiom/observer'), _sio))
    def bin(self):
        return config.benchmark_home('pfscan') + '/pfscan'
    def get_cc_files(self, path):
        return glob.glob('%s/*.cc' % path)
    def get_h_files(self, path):
        return glob.glob('%s/*.h' % path)
    def get_py_files(self, path):
        return glob.glob('%s/*.py' % path)
    def get_all_files(self, path):
        return glob.glob('%s/*' % path)

def get_test(input_idx='default'):
    return Test(input_idx)

