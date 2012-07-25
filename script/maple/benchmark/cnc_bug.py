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
import signal
import subprocess
import time
from maple.core import config
from maple.core import logging
from maple.core import testing

_sio = [None, os.devnull, 'stderr']

class Test(testing.ServerTest):
    def __init__(self, input_idx):
        testing.ServerTest.__init__(self, input_idx)
        self.add_input(['1000', '10'])
    def setup(self):
        os.environ['LD_LIBRARY_PATH'] = self.home()
        os.environ['DIST_CNC'] = 'SOCKETS'
    def tear_down(self):
        del os.environ['LD_LIBRARY_PATH']
        del os.environ['DIST_CNC']
    def start(self):
        logging.msg('starting server for cnc_bug\n')
        ipt = self.input()
        cmd = []
        if self.prefix != None:
            cmd.extend(self.prefix)
        cmd.append(self.pinger())
        cmd.extend(ipt)
        os.environ['CNC_SOCKET_HOST'] = '1'
        os.environ['CNC_SCHEDULER'] = 'TBB_QUEUE'
        self.server = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        self.contact_string = self.wait_for_contact_string()
        logging.msg('contact string = %s\n' % self.contact_string)
    def issue(self):
        time.sleep(1)
        logging.msg('issuing requests for cnc_bug\n')
        del os.environ['CNC_SOCKET_HOST']
        del os.environ['CNC_SCHEDULER']
        os.environ['CNC_SOCKET_CLIENT'] = self.contact_string
        self.client = subprocess.Popen([self.ponger()])
        self.client.wait()
        del os.environ['CNC_SOCKET_CLIENT']
        self.retcode = self.server.wait()
        self.server = None
        self.client = None
    def check_online(self):
        if self.retcode < 0:
            return (False, True, False)
        else:
            return (False, False, False)
    def wait_for_contact_string(self):
        while True:
            line = self.server.stdout.readline()
            if not line:
                return None
            str_vec = line.split()
            if len(str_vec) == 7:
                return str_vec[6]
    def home(self):
        return config.benchmark_home('cnc_bug')
    def pinger(self):
        return self.home() + '/cnc_pinger'
    def ponger(self):
        return self.home() + '/cnc_ponger'

def get_test(input_idx='default'):
    return Test(input_idx)

