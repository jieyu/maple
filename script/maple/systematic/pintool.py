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
from maple.core import pintool
from maple.systematic import scheduler

class Controller(pintool.Pintool):
    def __init__(self):
        pintool.Pintool.__init__(self, 'chess_controller')
        self.schedulers = {}
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages')
        self.register_knob('stat_out', 'string', 'stat.out', 'the statistics output file', 'PATH')
        self.register_knob('sinfo_in', 'string', 'sinfo.db', 'the input static info database path', 'PATH')
        self.register_knob('sinfo_out', 'string', 'sinfo.db', 'the output static info database path', 'PATH')
        self.register_knob('sched_app', 'bool', True, 'whether only schedule operations from the application')
        self.register_knob('sched_race', 'bool', False, 'whether schedule racy memory operations (for racy programs)')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('realtime_priority', 'int', 1, 'the realtime priority on which all the user thread should be run', 'PRIORITY')
        self.register_knob('program_in', 'string', 'program.db', 'the input database for the modeled program', 'PATH')
        self.register_knob('program_out', 'string', 'program.db', 'the output database for the modeled program', 'PATH')
        self.register_knob('race_in', 'string', 'race.db', 'the input race database path', 'PATH')
        self.register_knob('race_out', 'string', 'race.db', 'the output race database path', 'PATH')
        self.add_scheduler(scheduler.RandomScheduler())
        self.add_scheduler(scheduler.ChessScheduler())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'systematic_controller.so')
    def add_scheduler(self, s):
        self.merge_knob(s)
        self.schedulers[s.name] = s

