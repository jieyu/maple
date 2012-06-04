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

from maple.core import config
from maple.core import pintool
from maple.core import analyzer

class RecorderAnalyzer(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'recorder')
        self.register_knob('enable_recorder', 'bool', False, 'whether enable the recorder analyzer')
        self.register_knob('trace_log_path', 'string', 'trace-log', 'the trace log path', 'PATH')
        self.register_knob('trace_mem', 'bool', True, 'whether record memory accesses')
        self.register_knob('trace_atomic', 'bool', True, 'whether record atomic instructions')
        self.register_knob('trace_main', 'bool', True, 'whether record thread main functions')
        self.register_knob('trace_pthread', 'bool', True, 'whether record pthread functions')
        self.register_knob('trace_malloc', 'bool', True, 'whether record memory allocation functions')
        self.register_knob('trace_syscall', 'bool', True, 'whether record system calls')
        self.register_knob('trace_track_clk', 'bool', True, 'whether track per thread clock')

class Profiler(pintool.Pintool):
    def __init__(self):
        pintool.Pintool.__init__(self, 'trace_recorder')
        self.register_knob('ignore_ic_pthread', 'bool', True, 'do not count instructions in pthread')
        self.register_knob('ignore_lib', 'bool', False, 'whether ignore accesses from common libraries')
        self.add_analyzer(RecorderAnalyzer())
    def so_path(self):
        return config.build_home(self.debug) + '/tracer_profiler.so'

