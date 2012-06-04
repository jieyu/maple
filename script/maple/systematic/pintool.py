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

class Scheduler(object):
    def __init__(self, name):
        self.name = name
        self.knob_types = {}
        self.knob_defaults = {}
        self.knob_helps = {}
        self.knob_metavars = {}
        self.knobs = {}
    def register_knob(self, name, type, default, help, metavar=''):
        self.knob_types[name] = type
        self.knob_defaults[name] = default
        self.knob_helps[name] = help
        self.knob_metavars[name] = metavar
        self.knobs[name] = default

class RandomScheduler(Scheduler):
    def __init__(self):
        Scheduler.__init__(self, 'random_scheduler')
        self.register_knob('enable_random_scheduler', 'bool', False, 'whether use the random scheduler')

class ChessScheduler(Scheduler):
    def __init__(self):
        Scheduler.__init__(self, 'chess_scheduler')
        self.register_knob('enable_chess_scheduler', 'bool', False, 'whether use the CHESS scheduler')
        self.register_knob('fair', 'bool', True, 'whether enable the fair control module')
        self.register_knob('pb', 'bool', True, 'whether enable preemption bound search')
        self.register_knob('por', 'bool', True, 'whether enable parital order reduction')
        self.register_knob('abort_diverge', 'bool', True, 'whether abort when divergence happens')
        self.register_knob('pb_limit', 'int', 2, 'the maximum number of preemption an execution can have', 'LIMIT')
        self.register_knob('search_in', 'string', 'search.db', 'the input file that contains the search information', 'PATH')
        self.register_knob('search_out', 'string', 'search.db', 'the output file that contains the search information', 'PATH')
        self.register_knob('por_info_path', 'string', 'por-info', 'the dir path that stores the partial order reduction information', 'PATH')

class Controller(pintool.Pintool):
    def __init__(self):
        pintool.Pintool.__init__(self, 'chess_controller')
        self.schedulers = {}
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages', 'PATH') 
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
        self.add_scheduler(RandomScheduler())
        self.add_scheduler(ChessScheduler())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'systematic_controller.so')
    def add_scheduler(self, s):
        self.schedulers[s.name] = s
        for k, v in s.knobs.iteritems():
            self.knobs[k] = v
            self.knob_types[k] = s.knob_types[k]
            self.knob_defaults[k] = s.knob_defaults[k]
            self.knob_helps[k] = s.knob_helps[k]
            self.knob_metavars[k] = s.knob_metavars[k]

