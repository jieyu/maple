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
from maple.core import config
from maple.core import testing

_sio = [None, os.devnull, 'stderr']

class Test(testing.CmdlineTest):
    def __init__(self, input_idx):
        testing.CmdlineTest.__init__(self, input_idx)
        self.add_input(([self.bin(), '-n2', 'http://zodiac.eecs.umich.edu/testfile', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n3', 'http://zodiac.eecs.umich.edu/testfile', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n2', 'http://zodiac.eecs.umich.edu/test/46.txt', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n3', 'http://zodiac.eecs.umich.edu/test/46.txt', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n2', 'http://zodiac.eecs.umich.edu/test/47.txt', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n2', 'http://zodiac.eecs.umich.edu/test/48.txt', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n2', 'http://zodiac.eecs.umich.edu/test/49.txt', '-l', 'aget.file'], _sio))
        self.add_input(([self.bin(), '-n3', 'http://zodiac.eecs.umich.edu/test/49.txt', '-l', 'aget.file'], _sio))
    def bin(self):
        return config.benchmark_home('aget') + '/aget'
    def setup(self):
        assert not os.path.exists('aget.file')
    def tear_down(self):
        assert os.path.exists('aget.file')
        os.remove('aget.file')

def get_test(input_idx='default'):
    return Test(input_idx)

