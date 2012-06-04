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
Expected Results (predicted idiom4 iroots):
-------------------------------------------
12    IDIOM_4
	e0: UNLOCK  [48    target     0x741    sync_idiom4.cc +30]
	e1: LOCK    [46    target     0x734    sync_idiom4.cc +29]
	e2: UNLOCK  [50    target     0x75b    sync_idiom4.cc +32]
	e3: LOCK    [49    target     0x74e    sync_idiom4.cc +31]
13    IDIOM_4
	e0: UNLOCK  [48    target     0x741    sync_idiom4.cc +30]
	e1: LOCK    [46    target     0x734    sync_idiom4.cc +29]
	e2: UNLOCK  [54    target     0x78f    sync_idiom4.cc +36]
	e3: LOCK    [49    target     0x74e    sync_idiom4.cc +31]
15    IDIOM_4
	e0: UNLOCK  [48    target     0x741    sync_idiom4.cc +30]
	e1: LOCK    [51    target     0x768    sync_idiom4.cc +33]
	e2: UNLOCK  [54    target     0x78f    sync_idiom4.cc +36]
	e3: LOCK    [49    target     0x74e    sync_idiom4.cc +31]
20    IDIOM_4
	e0: UNLOCK  [50    target     0x75b    sync_idiom4.cc +32]
	e1: LOCK    [49    target     0x74e    sync_idiom4.cc +31]
	e2: UNLOCK  [52    target     0x775    sync_idiom4.cc +34]
	e3: LOCK    [51    target     0x768    sync_idiom4.cc +33]
22    IDIOM_4
	e0: UNLOCK  [52    target     0x775    sync_idiom4.cc +34]
	e1: LOCK    [46    target     0x734    sync_idiom4.cc +29]
	e2: UNLOCK  [50    target     0x75b    sync_idiom4.cc +32]
	e3: LOCK    [53    target     0x782    sync_idiom4.cc +35]
23    IDIOM_4
	e0: UNLOCK  [52    target     0x775    sync_idiom4.cc +34]
	e1: LOCK    [46    target     0x734    sync_idiom4.cc +29]
	e2: UNLOCK  [54    target     0x78f    sync_idiom4.cc +36]
	e3: LOCK    [53    target     0x782    sync_idiom4.cc +35]
25    IDIOM_4
	e0: UNLOCK  [52    target     0x775    sync_idiom4.cc +34]
	e1: LOCK    [51    target     0x768    sync_idiom4.cc +33]
	e2: UNLOCK  [54    target     0x78f    sync_idiom4.cc +36]
	e3: LOCK    [53    target     0x782    sync_idiom4.cc +35]
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 4:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +32' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +31'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +36' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +31'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +36' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +31'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +32' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +31' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +34' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +33'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +32' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +35'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +36' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +35'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +36' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +35'):
            return True
        return False
    else:
        return True # ignore iroots of other idioms

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['sync_only'] = True
    profiler.knobs['complex_idioms'] = True

def setup_testcase(testcase):
    testcase.threshold = 1

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    num_idiom4_iroots = 0
    for r in iroot_db.iroot_map.itervalues():
        if r.idiom() == 4:
            num_idiom4_iroots += 1
    if num_idiom4_iroots != 7:
        logging.msg('num idiom4 iroots mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

