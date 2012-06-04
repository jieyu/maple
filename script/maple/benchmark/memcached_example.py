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
import getpass
import signal
import subprocess
import threading
from maple.core import config
from maple.core import logging
from maple.core import testing
from maple.extra.memcached import memcache

def __client_0():
    mc = memcache.Client(['127.0.0.1:11211'], debug=0)
    for i in range(10):
        mc.set('key%d' % i, '%d' % i)
    for i in range(10):
        val = mc.get('key%d' % i)

def __client_1():
    mc = memcache.Client(['127.0.0.1:11211'], debug=0)
    for i in range(10):
        mc.set('key%d' % i, '%d' % (i * 10))
    for i in range(10):
        val = mc.get('key%d' % i)

class Client(threading.Thread):
    def __init__(self, client_idx):
        threading.Thread.__init__(self)
        self.client_idx = client_idx
    def run(self):
        eval('__client_%d()' % self.client_idx)

class Test(testing.ServerTest):
    def __init__(self, input_idx):
        testing.ServerTest.__init__(self, input_idx)
        self.add_input((2, [0, 1]))
    def start(self):
        num_threads, client_indexes = self.input()
        start_cmd = []
        if self.prefix != None:
            start_cmd.extend(self.prefix)
        start_cmd.append(self.server_bin())
        start_cmd.extend(['-t', str(num_threads)])
        if getpass.getuser() == 'root':
            start_cmd.extend(['-u', 'root'])
        logging.msg('starting memcached server...\n')
        logging.msg('cmd = %s\n' % str(start_cmd))
        self.server = subprocess.Popen(start_cmd)
        time.sleep(1)
    def stop(self):
        time.sleep(1)
        logging.msg('stopping memcached server...\n')
        os.kill(self.server.pid, signal.SIGINT)
    def kill(self):
        logging.msg('killing memcached server...\n')
        os.kill(self.server.pid, signal.SIGKILL)
    def issue(self):
        clients = []
        num_threads, client_indexes = self.input()
        for i in range(len(client_indexes)):
            clients.append(Client(client_indexes[i]))
        for i in range(len(clients)):
            clients[i].start()
        for i in range(len(clients)):
            clients[i].join()
    def server_bin(self):
        return config.benchmark_home('memcached') + '/bin/memcached'

def get_test(input_idx='default'):
    return Test(input_idx)

