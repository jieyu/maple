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
import copy
import time
import subprocess
from maple.core import config
from maple.core import util

class TestResult:
    INVALID   = 0
    NORMAL    = 1
    CRASH     = 2
    HANG      = 3
    MISMATCH  = 4
    NOT_FOUND = 5

class Test(object):
    """ The abstract class for tests.
    """
    def __init__(self, input_idx):
        self.inputs = []
        self.input_idx = input_idx
        self.prefix = None
        self.done = False
        self.result = TestResult.INVALID
        self.start_time = 0.0
        self.end_time = 0.0
    def input(self):
        if self.input_idx == 'default':
            return self.default_input()
        elif self.input_idx == 'random':
            return self.random_input()
        else:
            return self.get_input(int(self.input_idx))
    def add_input(self, input):
        self.inputs.append(input)
    def get_input(self, input_idx):
        assert input_idx < len(self.inputs)
        return self.inputs[input_idx]
    def default_input(self):
        assert len(self.inputs) > 0
        return self.inputs[0]
    def random_input(self):
        assert len(self.inputs) > 0
        random.seed()
        return random.choice(self.inputs)
    def set_prefix(self, prefix):
        self.prefix = prefix
    def used_time(self):
        assert self.done
        return self.end_time - self.start_time
    def is_fatal(self):
        assert self.done
        assert self.result != TestResult.INVALID
        if self.result != TestResult.NORMAL:
            return True
        else:
            return False
    def setup(self):
        pass
    def tear_down(self):
        pass
    def body(self):
        pass
    def run(self):
        self.setup()
        self.start_time = time.time()
        self.body()
        self.end_time = time.time()
        self.tear_down()
        self.done = True

class CmdlineTest(Test):
    """ Represents cmdline tests. The input format is a tuple (A, I)
    in which A is a list of arguments and I is a tuple of standard
    I/O files (stdin, stdout, stderr). (None means using default)
    """
    def __init__(self, input_idx):
        Test.__init__(self, input_idx)
        self.fio = [None, None, None]
    def cmd(self):
        c = []
        if self.prefix != None:
            c.extend(self.prefix)
        c.extend(self.input()[0])
        return c
    def sio(self):
        return self.input()[1]
    def body(self):
        self.open_stdio()
        proc = subprocess.Popen(self.cmd(),
                                stdin=self.fio[0],
                                stdout=self.fio[1],
                                stderr=self.fio[2])
        while True:
            time.sleep(0.1)
            retcode = proc.poll()
            if retcode != None:
                proc.wait()
                if retcode < 0:
                    self.result = TestResult.CRASH
                else:
                    if self.check_offline():
                        self.result = TestResult.MISMATCH
                    else:
                        self.result = TestResult.NORMAL
                break
            if self.check_hang():
                self.result = TestResult.HANG
                util.kill_process(proc.pid)
                break
            if self.check_online():
                self.result = TestResult.MISMATCH
                util.kill_process(proc.pid)
                break
        self.close_stdio()
    def check_hang(self):
        return False
    def check_online(self):
        return False
    def check_offline(self):
        return False
    def open_stdio(self):
        sin = self.sio()[0]
        sout = self.sio()[1]
        serr = self.sio()[2]
        if sin != None:
            self.fio[0] = open(sin)
        if sout != None:
            self.fio[1] = open(sout, 'a')
        if serr != None:
            self.fio[2] = open(serr, 'a')
    def close_stdio(self):
        sin = self.sio()[0]
        sout = self.sio()[1]
        serr = self.sio()[2]
        if sin != None:
            self.fio[0].close()
        if sout != None:
            self.fio[1].close()
        if serr != None:
            self.fio[2].close()

class InteractiveTest(CmdlineTest):
    """ Represents tests that are fired directedly by the users on
    the console. So the input is just a list of arguments.
    """
    def __init__(self, args, sin=None, sout=None, serr=None):
        CmdlineTest.__init__(self, 'default')
        self.add_input((args, [sin, sout, serr]))

class ServerTest(Test):
    """ The server test.
    """
    def __init(self, input_idx): 
        Test.__init__(self, input_idx)
    def body(self):
        self.start()
        self.issue()
        hang, crash, mismatch = self.check_online()
        if hang:
            self.result = TestResult.HANG
            self.kill()
        elif crash:
            self.result = TestResult.CRASH
            self.kill()
        elif mismatch:
            self.result = TestResult.MISMATCH
            self.stop()
        else:
            self.stop()
            if self.check_offline():
                self.result = TestResult.MISMATCH
            else:
                self.result = TestResult.NORMAL
    def start(self):
        pass
    def stop(self):
        pass
    def kill(self):
        pass
    def issue(self):
        pass
    def check_online(self):
        return (False, False, False)
    def check_offline(self):
        return False

class TestCase(object):
    """ The abstract class for test cases.
    """
    def __init__(self):
        self.done = False
        self.start_time = 0.0
        self.end_time = 0.0
    def used_time(self):
        assert self.done
        return self.end_time - self.start_time
    def elapsed_time(self):
        return time.time() - self.start_time
    def setup(self):
        pass
    def tear_down(self):
        pass
    def body(self):
        pass
    def run(self):
        self.setup()
        self.start_time = time.time()
        self.body()
        self.end_time = time.time()
        self.tear_down()
        self.done = True

class DeathTestCase(TestCase):
    """ The class for death test case which will run the same test
    again and again until a threshold (timeout or runout) is reached.
    """
    def __init__(self, test, mode, threshold):
        TestCase.__init__(self)
        self.test = test
        self.mode = mode
        self.threshold = threshold
        self.test_history = []
        self.result = None
    def used_runs(self):
        assert self.done
        return len(self.test_history)
    def is_fatal(self):
        assert self.result != None
        if self.result == 'FATAL':
            return True
        else:
            return False
    def body(self):
        self.before_all_tests()
        while True:
            test = copy.deepcopy(self.test)
            self.test_history.append(test)
            self.before_each_test()
            test.run()
            self.after_each_test()
            if test.is_fatal():
                self.result = 'FATAL'
                break
            if self.threshold_check():
                self.result = 'NORMAL'
                break
        self.after_all_tests()
    def threshold_check(self):
        if self.mode == 'runout':
            if len(self.test_history) >= int(self.threshold):
                return True
        elif self.mode == 'timeout':
            if self.elapsed_time() >= float(self.threshold):
                return True
        return False
    def before_each_test(self):
        pass
    def after_each_test(self):
        pass
    def before_all_tests(self):
        pass
    def after_all_tests(self):
        pass

