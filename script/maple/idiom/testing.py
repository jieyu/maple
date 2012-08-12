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
from maple.core import logging
from maple.core import static_info
from maple.core import pintool
from maple.core import testing
from maple.idiom import offline_tool
from maple.race import testing as race_testing
from maple.systematic import testing as systematic_testing

def has_candidate(tool):
    memo_tool = offline_tool.MemoTool()
    memo_tool.knobs['operation'] = 'has_candidate'
    memo_tool.knobs['arg'] = str(tool.knobs['target_idiom'])
    memo_tool.knobs['sinfo_in'] = tool.knobs['sinfo_out']
    memo_tool.knobs['iroot_in'] = tool.knobs['iroot_out']
    memo_tool.knobs['memo_in'] = tool.knobs['memo_out']
    stdout, stderr = memo_tool.run()
    if int(stdout) != 0:
        return True
    else:
        return False

def predicted_size(tool):
    memo_tool = offline_tool.MemoTool()
    memo_tool.knobs['operation'] = 'total_predicted'
    memo_tool.knobs['sinfo_in'] = tool.knobs['sinfo_out']
    memo_tool.knobs['iroot_in'] = tool.knobs['iroot_out']
    memo_tool.knobs['memo_in'] = tool.knobs['memo_out']
    stdout, stderr = memo_tool.run()
    return int(stdout)

def log_coverage(tool, used_time):
    memo_tool = offline_tool.MemoTool()
    memo_tool.knobs['operation'] = 'total_exposed'
    memo_tool.knobs['sinfo_in'] = tool.knobs['sinfo_out']
    memo_tool.knobs['iroot_in'] = tool.knobs['iroot_out']
    memo_tool.knobs['memo_in'] = tool.knobs['memo_out']
    stdout, stderr = memo_tool.run()
    exposed = stdout.split()
    f = open('coverage', 'a')
    f.write('%-25s ' % tool.name)
    f.write('%-10f ' % used_time)
    f.write('%-6d ' % int(exposed[0]))
    f.write('%-6d ' % int(exposed[1]))
    f.write('%-6d ' % int(exposed[2]))
    f.write('%-6d ' % int(exposed[3]))
    f.write('%-6d\n' % int(exposed[4]))
    f.close()

class NativeTestCase(testing.DeathTestCase):
    def __init__(self, test, mode, threshold):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== native iteration %d done === (%f) (%s)\n' % (iteration, used_time, os.getcwd()))
        f = open('timing', 'a')
        f.write('%f\n' % used_time)
        f.close()
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('native fatal error detected\n')
        else:
            logging.msg('native threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('native_runs', runs))
        logging.msg('%-15s %f\n' % ('native_time', used_time))

class RandomTestCase(testing.DeathTestCase):
    def __init__(self, test, mode, threshold, profiler):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
        self.profiler = profiler
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== random iteration %d done === (%f) (%s)\n' % (iteration, used_time, os.getcwd()))
        log_coverage(self.profiler, used_time)
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('random fatal error detected\n')
        else:
            logging.msg('random threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('random_runs', runs))
        logging.msg('%-15s %f\n' % ('random_time', used_time))

class ProfileTestCase(testing.DeathTestCase):
    def __init__(self, test, mode, threshold, profiler):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
        self.profiler = profiler
        self.predicted_size = 0
        self.stable_cnt = 0
    def threshold_check(self):
        if testing.DeathTestCase.threshold_check(self):
            return True
        if self.mode == 'stable':
            size = predicted_size(self.profiler)
            if size != self.predicted_size:
                self.stable_cnt = 0
            else:
                self.stable_cnt += 1
                if self.stable_cnt >= int(self.threshold):
                    return True
            self.predicted_size = size
        return False
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== profile iteration %d done === (%f) (%s)\n' % (iteration, used_time, os.getcwd()))
        log_coverage(self.profiler, used_time)
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('profile fatal error detected\n')
        else:
            logging.msg('profile threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('profile_runs', runs))
        logging.msg('%-15s %f\n' % ('profile_time', used_time))

class ActiveTestCase(testing.DeathTestCase):
    def __init__(self, test, mode, threshold, scheduler):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
        self.scheduler = scheduler
    def threshold_check(self):
        if not has_candidate(self.scheduler):
            return True
        if testing.DeathTestCase.threshold_check(self):
            return True
        return False
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== active iteration %d done === (%f) (%s)\n' % (iteration, used_time, os.getcwd()))
        log_coverage(self.scheduler, used_time)
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('active fatal error detected\n')
        else:
            logging.msg('active threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('active_runs', runs))
        logging.msg('%-15s %f\n' % ('active_time', used_time))

class IdiomTestCase(testing.TestCase):
    """ Represent the default idiom test process, that is, profile
    first then active test.
    """
    def __init__(self, profile_testcase, active_testcase):
        testing.TestCase.__init__(self)
        self.profile_testcase = profile_testcase
        self.active_testcase = active_testcase
    def is_fatal(self):
        assert self.done
        if self.profile_testcase.is_fatal() or self.active_testcase.is_fatal():
            return True
        else:
            return False
    def body(self):
        self.profile_testcase.run()
        if self.profile_testcase.is_fatal():
            logging.msg('\n')
            logging.msg('---------------------------\n')
            self.profile_testcase.log_stat()
        else:
            self.active_testcase.run()
            logging.msg('\n')
            logging.msg('---------------------------\n')
            self.profile_testcase.log_stat()
            self.active_testcase.log_stat()

class RaceTestCase(race_testing.TestCase):
    """ Run race detector to find all racy instructions.
    """
    def __init__(self, test, mode, threshold, profiler):
        race_testing.TestCase.__init__(self, test, mode, threshold, profiler)

class ChessTestCase(systematic_testing.ChessTestCase):
    """ Run a test under the CHESS scheduler.
    """
    def __init__(self, test, mode, threshold, controller):
        systematic_testing.ChessTestCase.__init__(self, test, mode, threshold, controller)
    def after_each_test(self):
        systematic_testing.ChessTestCase.after_each_test(self)
        used_time = self.test_history[-1].used_time()
        log_coverage(self.controller, used_time)

class ChessRaceTestCase(systematic_testing.ChessRaceTestCase):
    """ Run race detecctor to find all racy instructions first, and
    then run the chess scheduler with sched_race on.
    """
    def __init__(self, race_testcase, chess_testcase):
        systematic_testing.ChessRaceTestCase.__init__(self, race_testcase, chess_testcase)

