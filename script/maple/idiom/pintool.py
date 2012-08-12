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
from maple.systematic import scheduler

class SinstAnalyzer(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'sinst_analyzer')
        self.register_knob('enable_sinst', 'bool', False, 'whether enable the shared inst analyzer')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')

class Observer(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'observer')
        self.register_knob('enable_observer', 'bool', False, 'whether enable the iroot observer')
        self.register_knob('shadow_observer', 'bool', False, 'whether the observer is shadow')
        self.register_knob('sync_only', 'bool', False, 'whether only monitor synchronization accesses')
        self.register_knob('complex_idioms', 'bool', False, 'whether target complex idioms')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('vw', 'int', 1000, 'the vulnerability window (# dynamic inst)', 'SIZE')

class ObserverNew(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'observer_new')
        self.register_knob('enable_observer_new', 'bool', False, 'whether enable the iroot observer (NEW)')
        self.register_knob('shadow_observer', 'bool', False, 'whether the observer is shadow')
        self.register_knob('sync_only', 'bool', False, 'whether only monitor synchronization accesses')
        self.register_knob('complex_idioms', 'bool', False, 'whether target complex idioms')
        self.register_knob('single_var_idioms', 'bool', False, 'whether only target single variable idioms')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('vw', 'int', 1000, 'the vulnerability window (# dynamic inst)', 'SIZE')

class Predictor(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'predictor')
        self.register_knob('enable_predictor', 'bool', False, 'whether enable the iroot predictor')
        self.register_knob('sync_only', 'bool', False, 'whether only monitor synchronization accesses')
        self.register_knob('complex_idioms', 'bool', False, 'whether target complex idioms')
        self.register_knob('racy_only', 'bool', False, 'whether only consider sync and racy memory dependencies')
        self.register_knob('predict_deadlock', 'bool', False, 'whether predict and trigger deadlocks (experimental)')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('vw', 'int', 1000, 'the vulnerability window (# dynamic inst)', 'SIZE')

class PredictorNew(analyzer.Analyzer):
    def __init__(self):
        analyzer.Analyzer.__init__(self, 'predictor_new')
        self.register_knob('enable_predictor_new', 'bool', False, 'whether enable the iroot predictor (NEW)')
        self.register_knob('sync_only', 'bool', False, 'whether only monitor synchronization accesses')
        self.register_knob('complex_idioms', 'bool', False, 'whether target complex idioms')
        self.register_knob('single_var_idioms', 'bool', False, 'whether only target single variable idioms')
        self.register_knob('racy_only', 'bool', False, 'whether only consider sync and racy memory dependencies')
        self.register_knob('predict_deadlock', 'bool', False, 'whether predict and trigger deadlocks (experimental)')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('vw', 'int', 1000, 'the vulnerability window (# dynamic inst)', 'SIZE')

class Profiler(pintool.Pintool):
    def __init__(self, name='idiom_profiler'):
        pintool.Pintool.__init__(self, name)
        self.register_knob('ignore_ic_pthread', 'bool', True, 'do not count instructions in pthread')
        self.register_knob('ignore_lib', 'bool', False, 'whether ignore accesses from common libraries')
        self.register_knob('memo_failed', 'bool', True, 'whether memoize fail-to-expose iroots')
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages')
        self.register_knob('stat_out', 'string', 'stat.out', 'the statistics output file', 'PATH')
        self.register_knob('sinfo_in', 'string', 'sinfo.db', 'the input static info database path', 'PATH')
        self.register_knob('sinfo_out', 'string', 'sinfo.db', 'the output static info database path', 'PATH')
        self.register_knob('iroot_in', 'string', 'iroot.db', 'the input iroot database path', 'PATH')
        self.register_knob('iroot_out', 'string', 'iroot.db', 'the output iroot database path', 'PATH')
        self.register_knob('memo_in', 'string', 'memo.db', 'the input memoization database path', 'PATH')
        self.register_knob('memo_out', 'string', 'memo.db', 'the output memoization database path', 'PATH')
        self.register_knob('sinst_in', 'string', 'sinst.db', 'the input shared inst database path', 'PATH')
        self.register_knob('sinst_out', 'string', 'sinst.db', 'the output shared inst database path', 'PATH')
        self.add_analyzer(SinstAnalyzer())
        self.add_analyzer(Observer())
        self.add_analyzer(ObserverNew())
        self.add_analyzer(Predictor())
        self.add_analyzer(PredictorNew())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_profiler.so')

class PctProfiler(Profiler):
    def __init__(self):
        Profiler.__init__(self, 'idiom_pct_profiler')
        self.register_knob('strict', 'bool', False, 'whether use non-preemptive priorities')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('depth', 'int', 3, 'the target bug depth', 'DEPTH')
        self.register_knob('count_mem', 'bool', True, 'whether use the number of memory accesses as thread counter')
        self.register_knob('pct_history', 'string', 'pct.histo', 'the pct history file path', 'PATH')
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_pct_profiler.so')

class RandSchedProfiler(Profiler):
    def __init__(self):
        Profiler.__init__(self, 'idiom_randsched_profiler')
        self.register_knob('strict', 'bool', False, 'whether use non-preemptive priorities')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('delay', 'bool', False, 'whether inject delay instead of changing priorities at each change point')
        self.register_knob('float', 'bool', True, 'whether the number of change points depends on execution length')
        self.register_knob('float_interval', 'int', 50000, 'average number of memory accesses between two change points', 'N')
        self.register_knob('num_chg_pts', 'int', 3, 'number of change points (when float is set to False)', 'N')
        self.register_knob('rand_history', 'string', 'rand.histo', 'the rand history file path', 'PATH')
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_randsched_profiler.so')

class ChessProfiler(Profiler):
    def __init__(self):
        Profiler.__init__(self, 'idiom_chess_profiler')
        self.register_knob('sched_app', 'bool', True, 'whether only schedule operations from the application')
        self.register_knob('sched_race', 'bool', False, 'whether schedule racy memory operations (for racy programs)')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('realtime_priority', 'int', 1, 'the realtime priority on which all the user thread should be run', 'PRIORITY')
        self.register_knob('program_in', 'string', 'program.db', 'the input database for the modeled program', 'PATH')
        self.register_knob('program_out', 'string', 'program.db', 'the output database for the modeled program', 'PATH')
        self.register_knob('race_in', 'string', 'race.db', 'the input race database path', 'PATH')
        self.register_knob('race_out', 'string', 'race.db', 'the output race database path', 'PATH')
        self.schedulers = {}
        self.add_scheduler(scheduler.RandomScheduler())
        self.add_scheduler(scheduler.ChessScheduler())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_chess_profiler.so')
    def add_scheduler(self, s):
        self.merge_knob(s)
        self.schedulers[s.name] = s

class Scheduler(pintool.Pintool):
    def __init__(self):
        pintool.Pintool.__init__(self, 'idiom_scheduler')
        self.register_knob('ignore_ic_pthread', 'bool', True, 'do not count instructions in pthread')
        self.register_knob('ignore_lib', 'bool', False, 'whether ignore accesses from common libraries')
        self.register_knob('strict', 'bool', True, 'whether use non-preemptive priorities')
        self.register_knob('cpu', 'int', 0, 'which cpu to run on', 'CPU_ID')
        self.register_knob('unit_size', 'int', 4, 'the monitoring granularity in bytes', 'SIZE')
        self.register_knob('vw', 'int', 1000, 'the vulnerability window (# dynamic inst)', 'SIZE')
        self.register_knob('random_seed',  'int', 0, 'the random seed (0 means using current time)', 'SEED')
        self.register_knob('target_iroot', 'int', 0, 'the target iroot (0 means choosing any)', 'ID')
        self.register_knob('target_idiom', 'int', 0, 'the target idiom (0 means any idiom)', 'IDIOM')
        self.register_knob('memo_failed', 'bool', True, 'whether memoize fail-to-expose iroots')
        self.register_knob('yield_with_delay', 'bool', True, 'whether inject delays for async iroots')
        self.register_knob('test_history', 'string', 'test.histo', 'the test history file path', 'PATH')
        self.register_knob('debug_out', 'string', 'stdout', 'the output file for the debug messages')
        self.register_knob('stat_out', 'string', 'stat.out', 'the statistics output file', 'PATH')
        self.register_knob('sinfo_in', 'string', 'sinfo.db', 'the input static info database path', 'PATH')
        self.register_knob('sinfo_out', 'string', 'sinfo.db', 'the output static info database path', 'PATH')
        self.register_knob('iroot_in', 'string', 'iroot.db', 'the input iroot database path', 'PATH')
        self.register_knob('iroot_out', 'string', 'iroot.db', 'the output iroot database path', 'PATH')
        self.register_knob('memo_in', 'string', 'memo.db', 'the input memoization database path', 'PATH')
        self.register_knob('memo_out', 'string', 'memo.db', 'the output memoization database path', 'PATH')
        self.register_knob('sinst_in', 'string', 'sinst.db', 'the input shared inst database path', 'PATH')
        self.register_knob('sinst_out', 'string', 'sinst.db', 'the output shared inst database path', 'PATH')
        self.add_analyzer(SinstAnalyzer())
        self.add_analyzer(Observer())
        self.add_analyzer(ObserverNew())
    def so_path(self):
        return os.path.join(config.build_home(self.debug), 'idiom_scheduler.so')

