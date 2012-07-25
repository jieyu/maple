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
import shutil
import socket
import subprocess
import threading
import time
import psutil
from maple.core import config
from maple.core import logging
from maple.core import testing

def __client_0(test):
    subprocess.call([test.mysql(), '-u', 'root', '-D', 'test'], stdin=file(test.insert_sql))

class Client(threading.Thread):
    def __init__(self, client_idx, test):
        threading.Thread.__init__(self)
        self.client_idx = client_idx
        self.test = test
    def run(self):
        eval('__client_%d(self.test)' % self.client_idx)

class Test(testing.ServerTest):
    def __init__(self, input_idx):
        testing.ServerTest.__init__(self, input_idx)
        self.add_input([0])
    def setup(self):
        # Make sure we are running the script using root permission.
        assert os.getuid() == 0
        # Prepare SQL files.
        self.create_sql = self.home() + '/create.sql'
        f = open(self.create_sql, 'w')
        f.write('create table t1 (id int);')
        f.close()
        self.insert_sql = self.home() + '/insert.sql'
        f = open(self.insert_sql, 'w')
        for i in range(50):
            f.write('insert into t1 values (%d);' % i)
        f.close()
        self.flush_sql = self.home() + '/flush.sql'
        f = open(self.flush_sql, 'w')
        f.write('flush logs;')
        f.close()
        # Setup test database.
        shutil.rmtree(self.var(), ignore_errors=True)
        subprocess.call([self.mysql_install_db()])
        self.server = subprocess.Popen([self.mysqld_safe(), '--user=root', '--log-bin'])
        self.wait_for_idle()
        subprocess.call([self.mysql(), '-u', 'root', '-D', 'test'], stdin=file(self.create_sql))
        self.wait_for_idle()
        subprocess.call([self.mysqladmin(), '--user=root', 'shutdown'])
        self.server.wait()
        # Wrap the mysqld.
        self.wrap_mysqld()
    def tear_down(self):
        # Remove SQL files.
        os.remove(self.create_sql)
        os.remove(self.insert_sql)
        os.remove(self.flush_sql)
        # Unwrap the mysqld.
        self.unwrap_mysqld()
    def start(self):
        logging.msg('starting server for mysql_bug_791_training\n')
        self.server = subprocess.Popen([self.mysqld_safe(), '--user=root', '--log-bin'])
        self.wait_for_idle()
    def stop(self):
        self.wait_for_idle()
        logging.msg('stopping server for mysql_bug_791_training\n')
        time.sleep(2)
        subprocess.call([self.mysqladmin(), '--user=root', 'shutdown'])
        self.server.wait()
        self.server = None
    def issue(self):
        clients = []
        ipt = self.input()
        for idx in range(len(ipt)):
            clients.append(Client(ipt[idx], self))
        logging.msg('issuing requests for mysql_bug_791_training\n')
        for i in range(len(clients)):
            clients[i].start()
        for i in range(len(clients)):
            clients[i].join()
    def wrap_mysqld(self):
        cmd = []
        if self.prefix != None:
            cmd.extend(self.prefix)
        cmd.append(self.mysqld_real())
        cmd.append('$*')
        os.rename(self.mysqld(), self.mysqld_real())
        script = open(self.mysqld(), 'w')
        script.write('#!/bin/sh\n\n')
        for c in cmd:
            script.write('%s ' % c)
        script.write('\n\n')
        script.close()
        os.chmod(self.mysqld(), 0755)
    def unwrap_mysqld(self):
        os.remove(self.mysqld())
        os.rename(self.mysqld_real(), self.mysqld())
    def wait_for_idle(self):
        while not os.path.exists(self.pid_file()):
            time.sleep(0.1)
        p = psutil.Process(self.server.pid)
        while True:
            if p.get_cpu_percent() < 10.0:
                break
    def home(self):
        return config.benchmark_home('mysql_bug_791')
    def pid_file(self):
        return self.home() + '/var/' + socket.gethostname() + '.pid'
    def mysql_install_db(self):
        return self.home() + '/bin/mysql_install_db'
    def mysqld_safe(self):
        return self.home() + '/bin/mysqld_safe'
    def mysql(self):
        return self.home() + '/bin/mysql'
    def mysqladmin(self):
        return self.home() + '/bin/mysqladmin'
    def var(self):
        return self.home() + '/var'
    def mysqld(self):
        return self.home() + '/libexec/mysqld'
    def mysqld_real(self):
        return self.home() + '/libexec/mysqld.real'

def get_test(input_idx='default'):
    return Test(input_idx)

