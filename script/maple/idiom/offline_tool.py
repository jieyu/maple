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
from maple.core import offline_tool

class MemoTool(offline_tool.OfflineTool):
    def __init__(self):
        offline_tool.OfflineTool.__init__(self, 'idiom_memo_tool')
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages')
        self.register_knob('sinfo_in', 'string', 'sinfo.db', 'the input static info database path', 'PATH')
        self.register_knob('sinfo_out', 'string', 'sinfo.db', 'the output static info database path', 'PATH')
        self.register_knob('iroot_in', 'string', 'iroot.db', 'the input iroot database path', 'PATH')
        self.register_knob('iroot_out', 'string', 'iroot.db', 'the output iroot database path', 'PATH')
        self.register_knob('memo_in', 'string', 'memo.db', 'the input memoization database path', 'PATH')
        self.register_knob('memo_out', 'string', 'memo.db', 'the output memoization database path', 'PATH')
        self.register_knob('operation', 'string', 'list', 'the operation to perform')
        self.register_knob('arg', 'string', 'null', 'the argument to the operation')
        self.register_knob('path', 'string', 'null', 'the path argument to the operation', 'PATH')
        self.register_knob('num', 'int', 0, 'the integer argument to the operation')
    def bin_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_memo_tool')

