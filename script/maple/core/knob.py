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

# The abstract base class for those use knobs.
class KnobUser(object):
    def __init__(self):
        self.knob_prefix = ''
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
        self.knobs[name] = self.knob_defaults[name]
    def merge_knob(self, other):
        for k, v in other.knobs.iteritems():
            self.knob_types[k] = other.knob_types[k]
            self.knob_defaults[k] = other.knob_defaults[k]
            self.knob_helps[k] = other.knob_helps[k]
            self.knob_metavars[k] = other.knob_metavars[k]
            self.knobs[k] = v
    def register_cmdline_options(self, parser):
        for k, v in self.knobs.iteritems():
            opt_str = '--%s%s' % (self.knob_prefix, k)
            if self.knob_types[k] == 'bool':
                if self.knob_defaults[k] == False:
                    parser.add_option(
                            '--%s%s' % (self.knob_prefix, k),
                            action='store_true',
                            dest='%s%s' % (self.knob_prefix, k),
                            default=False,
                            help='%s [default: False]' % self.knob_helps[k])
                else:
                    parser.add_option(
                            '--no_%s%s' % (self.knob_prefix, k),
                            action='store_false',
                            dest='%s%s' % (self.knob_prefix, k),
                            default=True,
                            help='%s [default: True]' % self.knob_helps[k])
            else:
                parser.add_option(
                        '--%s%s' % (self.knob_prefix, k),
                        action='store',
                        type=self.knob_types[k],
                        dest='%s%s' % (self.knob_prefix, k),
                        default=self.knob_defaults[k],
                        metavar=self.knob_metavars[k],
                        help='%s [default: %s]' % \
                                (self.knob_helps[k],
                                 str(self.knob_defaults[k])))
    def set_cmdline_options(self, options, args):
        for k in self.knobs.keys():
            self.knobs[k] = eval('options.%s%s' % (self.knob_prefix, k))

