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
import threading
import psutil
from maple.core import config
from maple.core import logging
from maple.core import testing

class Client(threading.Thread):
    def __init__(self, input_entry):
        threading.Thread.__init__(self)
        self.input_entry = input_entry
    def run(self):
        uri_idx, num_calls = self.input_entry 
        httperf_args = []
        httperf_args.append(config.benchmark_home('httperf') + '/bin/httperf')
        httperf_args.append('--server=localhost')
        httperf_args.append('--uri=/%d.txt' % uri_idx)
        httperf_args.append('--num-calls=%d' % num_calls)
        httperf_args.append('--num-conns=1')
        logging.msg('[apache] client: uri_idx=%d, num_calls=%d\n' % (uri_idx, num_calls))
        fnull = open(os.devnull, 'w')
        subprocess.call(httperf_args, stdout=fnull, stderr=fnull)
        fnull.close()
        logging.msg('[apache] client done\n')

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
        self.apache_home = config.benchmark_home('apache')
    def setup(self):
        pid_file = self.apache_home + '/logs/httpd.pid'
        if os.path.exists(pid_file):
            os.remove(pid_file)
        log_file = self.apache_home + '/logs/access_log'
        f = open(log_file, 'w')
        f.close()
    def start(self):
        start_cmd = []
        if self.prefix != None:
            start_cmd.extend(self.prefix)
        start_cmd.append(self.server_bin())
        start_cmd.extend(['-k', 'start', '-D', 'ONE_PROCESS'])
        logging.msg('[apache] starting server... \n')
        self.server = subprocess.Popen(start_cmd)
        self.wait_for_pidfile()
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
        stop_cmd.append(self.server_bin())
        stop_cmd.extend(['-k', 'stop'])
        logging.msg('[apache] stopping server... \n')
        subprocess.call(stop_cmd)
        self.server.wait()
    def kill(self):
        self.stop()
    def issue(self):
        clients = []
        ipt = self.input()
        for idx in range(len(ipt)): 
            clients.append(Client(ipt[idx]))
        logging.msg('[apache] issuing requests... \n')
        for i in range(len(clients)):
            clients[i].start()
        for i in range(len(clients)):
            clients[i].join()
    def server_bin(self):
        return self.apache_home + '/bin/httpd'
    def wait_for_pidfile(self):
        pid_file = self.apache_home + '/logs/httpd.pid'
        while not os.path.exists(pid_file):
            time.sleep(1)

def get_test(input_idx='default'):
    return Test(input_idx)

