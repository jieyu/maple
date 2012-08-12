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
import subprocess
from maple.core import knob

class OfflineTool(knob.KnobUser):
    """ The abstract class for offline tools.
    """
    def __init__(self, name):
        knob.KnobUser.__init__(self)
        self.name = name
        self.debug = False
    def bin_path(self):
        pass
    def cmd(self):
        c = []
        c.append(self.bin_path())
        for k, v in self.knobs.iteritems():
            a = '--' + k + '='
            if self.knob_types[k] == 'bool':
                if v:
                    a += '1'
                else:
                    a += '0'
            elif self.knob_types[k] == 'string':
                if self.knob_metavars[k] == 'PATH':
                    a += os.path.realpath(v)
                else:
                    a += v
            else:
                a += str(v)
            c.append(a)
        return c
    def call(self):
        subprocess.call(self.cmd())
    def run(self):
        proc = subprocess.Popen(self.cmd(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return proc.communicate()
    def register_cmdline_options(self, parser):
        knob.KnobUser.register_cmdline_options(self, parser)
        parser.add_option(
                '--%sdebug' % self.knob_prefix,
                action='store_true',
                dest='%sdebug' % self.knob_prefix,
                default=False,
                help='whether in debugging mode [default: False]')
    def set_cmdline_options(self, options, args):
        knob.KnobUser.set_cmdline_options(self, options, args)
        self.debug = eval('options.%sdebug' % self.knob_prefix)

