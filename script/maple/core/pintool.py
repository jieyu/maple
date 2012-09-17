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
from maple.core import analyzer
from maple.core import knob

class Pin(object):
    """ The wrapper class for PIN binary instrumentation tool
    """
    def __init__(self, home_path):
        self.home_path = home_path
        self.logging_mode = False
        self.debugging_mode = False
        self.knobs = {}
    def pin(self):
        return self.home_path + '/pin'
    def options(self):
        c = []
        if self.logging_mode:
            c.append('-xyzzy')
            c.append('-mesgon')
            c.append('log_signal')
            c.append('-mesgon')
            c.append('log_syscall')
        if self.debugging_mode:
            c.append('-pause_tool')
            c.append('10')
        for k, v in self.knobs.iteritems():
            c.append('-' + k)
            c.append(str(v))
        return c

class Pintool(knob.KnobUser):
    """ The abstract class for PIN tools.
    """
    def __init__(self, name):
        knob.KnobUser.__init__(self)
        self.name = name
        self.debug = False
        self.analyzers = {}
        self.add_analyzer(analyzer.DebugAnalyzer())
    def so_path(self):
        pass
    def options(self):
        c = []
        c.append('-t')
        c.append(self.so_path())
        for k, v in self.knobs.iteritems():
            c.append('-' + k)
            if self.knob_types[k] == 'bool':
                if v:
                    c.append('1')
                else:
                    c.append('0')
            elif self.knob_types[k] == 'string':
                if self.knob_metavars[k] == 'PATH':
                    c.append(os.path.realpath(v))
                else:
                    c.append(v)
            else:
                c.append(str(v))
        return c
    def register_cmdline_options(self, parser):
        knob.KnobUser.register_cmdline_options(self, parser)
        parser.add_option(
                '--%sdebug' % self.knob_prefix,
                action='store_true',
                dest='%sdebug' % self.knob_prefix,
                default=False,
                help='whether use the debug version of the pintool [default: False]')
    def set_cmdline_options(self, options, args):
        knob.KnobUser.set_cmdline_options(self, options, args)
        self.debug = eval('options.%sdebug' % self.knob_prefix)
    def add_analyzer(self, a):
        self.analyzers[a.name] = a
        self.merge_knob(a)

