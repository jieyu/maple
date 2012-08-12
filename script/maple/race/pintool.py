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
from maple.core import analyzer

class Detector(analyzer.Analyzer):
    def __init__(self, name):
        analyzer.Analyzer.__init__(self, name)
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')

class Djit(Detector):
    def __init__(self):
        Detector.__init__(self, 'race_djit')
        self.register_knob('enable_djit', 'bool', False, 'whether enable the djit data race detector')
        self.register_knob('track_racy_inst', 'bool', False, 'whether track potential racy instructions')

class Profiler(pintool.Pintool):
    def __init__(self, name='race_profiler'):
        pintool.Pintool.__init__(self, name)
        self.register_knob('ignore_lib', 'bool', False, 'whether ignore accesses from common libraries')
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages')
        self.register_knob('stat_out', 'string', 'stat.out', 'the statistics output file', 'PATH')
        self.register_knob('sinfo_in', 'string', 'sinfo.db', 'the input static info database path', 'PATH')
        self.register_knob('sinfo_out', 'string', 'sinfo.db', 'the output static info database path', 'PATH')
        self.register_knob('race_in', 'string', 'race.db', 'the input race database path', 'PATH')
        self.register_knob('race_out', 'string', 'race.db', 'the output race database path', 'PATH')
        self.add_analyzer(Djit())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'race_profiler.so')

class PctProfiler(Profiler):
    def __init__(self):
        Profiler.__init__(self, 'race_pct_profiler')
        self.register_knob('strict', 'bool', False, 'whether use non-preemptive priorities')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('depth', 'int', 3, 'the target bug depth', 'DEPTH')
        self.register_knob('count_mem', 'bool', True, 'whether use the number of memory accesses as thread counter')
        self.register_knob('pct_history', 'string', 'pct.histo', 'the pct history file path', 'PATH')
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'race_pct_profiler.so')

