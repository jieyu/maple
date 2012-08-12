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
import signal
import subprocess
import threading
import psutil
from maple.core import config
from maple.core import logging
from maple.core import testing
from maple.extra.memcached import memcache

"""
This should give you a feel for how this module operates::

import memcache
mc = memcache.Client(['127.0.0.1:11211'], debug=0)

mc.set("some_key", "Some value")
value = mc.get("some_key")

mc.set("another_key", 3)
mc.delete("another_key")

mc.set("key", "1")   # note that the key used for incr/decr must be a string.
mc.incr("key")
mc.decr("key")
"""

def wait_for_idle(pid):
    p = psutil.Process(pid)
    while True:
        print p.get_num_threads()
        time.sleep(1)

def __client_0():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    for i in range(10):
        mc.set('key%d' % i, '%d' % i)
    for i in range(10):
        val = mc.get('key%d' % i)

def __client_1():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    for i in range(10):
        mc.set('key%d' % i, '%d' % (i * 10))
    for i in range(10):
        val = mc.get('key%d' % i)

def __client_2():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    mc.set('key', '1')
    for i in range(10):
        val = mc.get('key')

def __client_3():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    mc.set('key', '5')
    for i in range(10):
        val = mc.get('key')

def __client_4():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    for i in range(10):
        val = mc.set('key', '%d' % i)

def __client_5():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    mc.set('key', '50')
    for i in range(10):
        mc.incr('key')

def __client_6():
    mc = memcache.Client(['127.0.0.1:12345'], debug=0)
    mc.set('key', '50')
    for i in range(10):
        mc.decr('key')

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
        self.add_input((2, [0, 2]))
        self.add_input((2, [0, 5]))
        self.add_input((2, [1, 6]))
        self.add_input((2, [5, 4]))
        self.add_input((3, [5, 2]))
        self.add_input((3, [2, 3, 4]))
        self.add_input((3, [5, 6, 4]))
    def start(self):
        num_threads, client_indexes = self.input()
        start_cmd = []
        if self.prefix != None:
            start_cmd.extend(self.prefix)
        start_cmd.append(self.bin())
        start_cmd.extend(['-p', '12345'])
        start_cmd.extend(['-t', str(num_threads)])
        if os.getuid() == 0:
            start_cmd.extend(['-u', 'root'])
        logging.msg('starting server for memcached\n')
        self.server = subprocess.Popen(start_cmd)
        p = psutil.Process(self.server.pid)
        while p.get_num_threads() < (num_threads + 2):
            time.sleep(1)
        while p.get_cpu_percent() > 25.0:
            time.sleep(1)
    def stop(self):
        p = psutil.Process(self.server.pid)
        while p.get_cpu_percent() > 25.0:
            time.sleep(1)
        logging.msg('stopping server for memcached\n')
        os.kill(self.server.pid, signal.SIGINT)
        self.server.wait()
        self.server = None
    def kill(self):
        self.stop()
    def issue(self):
        clients = []
        num_threads, client_indexes = self.input()
        for i in range(len(client_indexes)):
            clients.append(Client(client_indexes[i]))
        logging.msg('issuing requests for memcached\n')
        for i in range(len(clients)):
            clients[i].start()
        for i in range(len(clients)):
            clients[i].join()
    def bin(self):
        return config.benchmark_home('memcached') + '/bin/memcached'

def get_test(input_idx='default'):
    return Test(input_idx)

