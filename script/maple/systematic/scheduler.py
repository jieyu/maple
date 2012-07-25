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

from maple.core import knob

class Scheduler(knob.KnobUser):
    def __init__(self, name):
        knob.KnobUser.__init__(self)
        self.name = name

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

