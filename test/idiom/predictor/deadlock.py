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

from maple.core import logging
from maple.core import static_info
from maple.idiom import iroot
from maple.idiom import memo
from maple.regression import common

"""
Expected Results (predicted iroots):
------------------------------------
1     IDIOM_1
	e0: UNLOCK  [49    target     0x754    deadlock.cc +47]
	e1: LOCK    [50    target     0x79a    deadlock.cc +32]
2     IDIOM_1
	e0: UNLOCK  [55    target     0x7b9    deadlock.cc +35]
	e1: LOCK    [45    target     0x735    deadlock.cc +44]
3     IDIOM_1
	e0: UNLOCK  [57    target     0x7c6    deadlock.cc +36]
	e1: LOCK    [46    target     0x742    deadlock.cc +45]
4     IDIOM_1
	e0: UNLOCK  [51    target     0x761    deadlock.cc +48]
	e1: LOCK    [52    target     0x7a7    deadlock.cc +33]
5     IDIOM_5
	e0: LOCK    [50    target     0x79a    deadlock.cc +32]
	e1: LOCK    [46    target     0x742    deadlock.cc +45]
	e2: LOCK    [45    target     0x735    deadlock.cc +44]
	e3: LOCK    [52    target     0x7a7    deadlock.cc +33]
6     IDIOM_5
	e0: LOCK    [45    target     0x735    deadlock.cc +44]
	e1: LOCK    [52    target     0x7a7    deadlock.cc +33]
	e2: LOCK    [50    target     0x79a    deadlock.cc +32]
	e3: LOCK    [46    target     0x742    deadlock.cc +45]

(We should predicted two idiom5 iroots which will lead to deadlock.)
"""

def disabled():
    return True

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +47' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +32'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +35' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +44'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +36' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +45'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +48' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33'):
            return True
    elif r.idiom() == 5:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mutex_lock() and
            e0.inst().debug_info() == source_name() + ' +44' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33' and
            e2.is_mutex_lock() and
            e2.inst().debug_info() == source_name() + ' +32' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +45'):
            return True
        if (e0.is_mutex_lock() and
            e0.inst().debug_info() == source_name() + ' +32' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +45' and
            e2.is_mutex_lock() and
            e2.inst().debug_info() == source_name() + ' +44' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +33'):
            return True
    return False

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['sync_only'] = True
    profiler.knobs['complex_idioms'] = True
    profiler.knobs['predict_deadlock'] = True

def setup_testcase(testcase):
    testcase.threshold = 1

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    if len(iroot_db.iroot_map) != 6:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

