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

from maple.core import logging
from maple.core import static_info
from maple.core import testing
from maple.race import race

class TestCase(testing.DeathTestCase):
    def __init__(self, test, mode, threshold, profiler):
        testing.DeathTestCase.__init__(self, test, mode, threshold)
        self.profiler = profiler
        self.num_static_races = 0
        self.num_racy_insts = 0
        self.stable_cnt = 0
    def threshold_check(self):
        if testing.DeathTestCase.threshold_check(self):
            return True
        if self.mode == 'stable':
            sinfo = static_info.StaticInfo()
            sinfo.load(self.profiler.knobs['sinfo_out'])
            race_db = race.RaceDB(sinfo)
            race_db.load(self.profiler.knobs['race_out'])
            if (race_db.num_static_races() != self.num_static_races or
                race_db.num_racy_insts() != self.num_racy_insts):
                self.stable_cnt = 0
            else:
                self.stable_cnt += 1
                if self.stable_cnt >= int(self.threshold):
                    return True
            self.num_static_races = race_db.num_static_races()
            self.num_racy_insts = race_db.num_racy_insts()
        return False
    def after_each_test(self):
        iteration = len(self.test_history)
        used_time = self.test_history[-1].used_time()
        logging.msg('=== race iteration %d done === (%f)\n' % (iteration, used_time))
    def after_all_tests(self):
        if self.is_fatal():
            logging.msg('race fatal error detected\n')
        else:
            logging.msg('race threshold reached\n')
    def log_stat(self):
        runs = len(self.test_history)
        used_time = self.used_time()
        logging.msg('%-15s %d\n' % ('race_runs', runs))
        logging.msg('%-15s %f\n' % ('race_time', used_time))

