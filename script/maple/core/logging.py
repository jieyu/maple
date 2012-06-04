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

import sys
from maple.core import config

_message_on = True
_message_level = 1

def err(msg):
    sys.stderr.write('[%s] %s' % (config.tool_name(), msg))
    sys.exit(1)

def msg(msg, level=1):
    if _message_on and level <= _message_level:
        sys.stdout.write('[%s] %s' % (config.tool_name(), msg))

def set_message_level(level):
    global _message_level
    _message_level = level

def message_off():
    global _message_on
    _message_on = False

def message_on():
    global _message_on
    _message_on = True

