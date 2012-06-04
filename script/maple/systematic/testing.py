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
from maple.core import logging
from maple.core import static_info
from maple.core import testing
from maple.race import testing as race_testing
from maple.systematic import program
from maple.systematic import search

class ChessTestCase(testing.DeathTestCase):
    """ Run a test under the CHESS scheduler.
    """
    def __init__(self, test, mode, threshold, controller):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
        self.controller = controller
    def threshold_check(self):
        if self.search_done():
            return True
        if testing.DeathTestCase.threshold_check(self):
            return True
        return False
    def search_done(self):
        sinfo = static_info.StaticInfo()
        sinfo.load(self.controller.knobs['sinfo_out'])
        prog = program.Program(sinfo)
        prog.load(self.controller.knobs['program_out'])
        search_info = search.SearchInfo(sinfo, program)
        search_info.load(self.controller.knobs['search_out'])
        return search_info.done()
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== chess iteration %d done === (%f) (%s)\n' % (iteration, used_time, os.getcwd()))
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('chess fatal error detected\n')
        else:
            logging.msg('chess threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('chess_runs', runs))
        logging.msg('%-15s %f\n' % ('chess_time', used_time))

class RaceTestCase(race_testing.TestCase):
    """ Run race detector to find all racy instructions.
    """
    def __init__(self, test, mode, threshold, profiler):
        race_testing.TestCase.__init__(self, test, mode, threshold, profiler)

class ChessRaceTestCase(testing.TestCase):
    """ Run race detecctor to find all racy instructions first, and
    then run the chess scheduler with sched_race on.
    """
    def __init__(self, race_testcase, chess_testcase):
        testing.TestCase.__init__(self)
        self.race_testcase = race_testcase
        self.chess_testcase = chess_testcase
    def is_fatal(self):
        assert self.done
        if self.race_testcase.is_fatal() or self.chess_testcase.is_fatal():
            return True
        else:
            return False
    def body(self):
        self.race_testcase.run()
        if self.race_testcase.is_fatal():
            logging.msg('\n')
            logging.msg('---------------------------\n')
            self.race_testcase.log_stat()
        else:
            self.chess_testcase.run()
            logging.msg('\n')
            logging.msg('---------------------------\n')
            self.race_testcase.log_stat()
            self.chess_testcase.log_stat()

