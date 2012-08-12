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
import time
import subprocess
import psutil
from maple.core import config
from maple.core import logging
from maple.core import testing

class Client(object):
    def __init__(self, input_entry):
        self.input_entry = input_entry
    def start(self):
        uri_idx, num_calls = self.input_entry 
        httperf_args = []
        httperf_args.append(config.benchmark_home('httperf') + '/bin/httperf')
        httperf_args.append('--server=localhost')
        httperf_args.append('--port=8080')
        httperf_args.append('--uri=/test/%d.txt' % uri_idx)
        httperf_args.append('--num-calls=%d' % num_calls)
        httperf_args.append('--num-conns=1')
        logging.msg('client: uri_idx=%d, num_calls=%d\n' % (uri_idx, num_calls))
        self.fnull = open(os.devnull, 'w')
        self.proc = subprocess.Popen(httperf_args, stdout=self.fnull, stderr=self.fnull)
    def join(self):
        self.proc.wait()
        self.fnull.close()
        self.proc = None
        self.fnull = None
        logging.msg('client done\n')

class Test(testing.ServerTest):
    def __init__(self, input_idx):
        testing.ServerTest.__init__(self, input_idx)
        # input format: [(uri_idx, num_calls), ...]
        self.add_input([(1, 50), (1, 30)])
        self.add_input([(2, 40), (1, 20)])
        self.add_input([(3, 30), (4, 20)])
        self.add_input([(6, 50), (7, 20)])
        self.add_input([(3, 50), (2, 20)])
        self.add_input([(3, 40), (3, 20)])
        self.add_input([(1, 20), (5, 20)])
        self.add_input([(9, 40), (9, 30)])
    def setup(self):
        if os.path.exists(self.pid_file()):
            os.remove(self.pid_file())
        f = open(self.log_file(), 'w')
        f.close()
    def start(self):
        start_cmd = []
        if self.prefix != None:
            start_cmd.extend(self.prefix)
        start_cmd.append(self.bin())
        start_cmd.extend(['-k', 'start', '-D', 'ONE_PROCESS'])
        logging.msg('starting server for apache\n')
        self.server = subprocess.Popen(start_cmd)
        while not os.path.exists(self.pid_file()):
            time.sleep(1)
        p = psutil.Process(self.server.pid)
        while p.get_num_threads() != 4:
            time.sleep(1)
        time.sleep(1)
    def stop(self):
        p = psutil.Process(self.server.pid)
        while p.get_cpu_percent() > 5.0:
            time.sleep(1)
        time.sleep(1)
        stop_cmd = []
        stop_cmd.append(self.bin())
        stop_cmd.extend(['-k', 'stop'])
        logging.msg('stopping server for apache\n')
        subprocess.call(stop_cmd)
        self.server.wait()
        self.server = None
    def kill(self):
        self.stop()
    def issue(self):
        clients = []
        ipt = self.input()
        for idx in range(len(ipt)): 
            clients.append(Client(ipt[idx]))
        logging.msg('issuing requests for apache\n')
        for i in range(len(clients)):
            clients[i].start()
        for i in range(len(clients)):
            clients[i].join()
    def home(self):
        return config.benchmark_home('apache')
    def bin(self):
        return self.home() + '/bin/httpd'
    def pid_file(self):
        return self.home() + '/logs/httpd.pid'
    def log_file(self):
        return self.home() + '/logs/access_log'

def get_test(input_idx='default'):
    return Test(input_idx)

