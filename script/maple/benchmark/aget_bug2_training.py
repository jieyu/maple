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
import subprocess
from maple.core import config
from maple.core import logging
from maple.core import testing

_sio = [None, os.devnull, 'stderr']

class Test(testing.ServerTest):
    def __init__(self, input_idx):
        testing.ServerTest.__init__(self, input_idx)
        self.add_input(([self.bin(), '-n1', 'http://apache.cyberuse.com/httpd/httpd-2.2.21.tar.gz', '-l', 'aget.file'], _sio))
    def setup(self):
        if os.path.exists('aget.file'):
            os.remove('aget.file')
    def tear_down(self):
        if os.path.exists('aget.file'):
            os.remove('aget.file')
    def start(self):
        args, sio = self.input()
        cmd = []
        if self.prefix != None:
            cmd.extend(self.prefix)
        cmd.extend(args)
        self.proc = subprocess.Popen(cmd)
    def stop(self):
        self.proc.wait()
    def bin(self):
        return config.benchmark_home('aget_bug2') + '/aget'

def get_test(input_idx='default'):
    return Test(input_idx)

