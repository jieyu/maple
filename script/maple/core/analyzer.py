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

class Analyzer(object):
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

class DebugAnalyzer(Analyzer):
    def __init__(self):
        Analyzer.__init__(self, 'debug_analyzer')
        self.register_knob('enable_debug', 'bool', False, 'whether enable the debug analyzer')
        self.register_knob('debug_mem', 'bool', False, 'whether debug mem accesses')
        self.register_knob('debug_atomic', 'bool', False, 'whether debug atomic inst')
        self.register_knob('debug_main', 'bool', True, 'whether debug main functions')
        self.register_knob('debug_pthread', 'bool', True, 'whether debug pthread functions')
        self.register_knob('debug_malloc', 'bool', True, 'whether debug malloc functions')
        self.register_knob('debug_syscall', 'bool', False, 'whether debug system calls')
        self.register_knob('debug_track_clk', 'bool', True, 'whether track per thread clock')

