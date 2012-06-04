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
	e0: UNLOCK  [59    target     0x719    sync_idiom1.cc +36]
	e1: LOCK    [53    target     0x6d8    sync_idiom1.cc +31]
2     IDIOM_1
	e0: UNLOCK  [59    target     0x719    sync_idiom1.cc +36]
	e1: LOCK    [57    target     0x70c    sync_idiom1.cc +35]
3     IDIOM_1
	e0: UNLOCK  [59    target     0x719    sync_idiom1.cc +36]
	e1: LOCK    [72    target     0x7ea    sync_idiom1.cc +55]
4     IDIOM_1
	e0: UNLOCK  [54    target     0x6e5    sync_idiom1.cc +32]
	e1: LOCK    [53    target     0x6d8    sync_idiom1.cc +31]
5     IDIOM_1
	e0: UNLOCK  [54    target     0x6e5    sync_idiom1.cc +32]
	e1: LOCK    [57    target     0x70c    sync_idiom1.cc +35]
6     IDIOM_1
	e0: UNLOCK  [56    target     0x6ff    sync_idiom1.cc +34]
	e1: LOCK    [50    target     0x6be    sync_idiom1.cc +29]
7     IDIOM_1
	e0: UNLOCK  [56    target     0x6ff    sync_idiom1.cc +34]
	e1: LOCK    [55    target     0x6f2    sync_idiom1.cc +33]
8     IDIOM_1
	e0: UNLOCK  [56    target     0x6ff    sync_idiom1.cc +34]
	e1: LOCK    [70    target     0x7d0    sync_idiom1.cc +53]
9     IDIOM_1
	e0: UNLOCK  [51    target     0x6cb    sync_idiom1.cc +30]
	e1: LOCK    [50    target     0x6be    sync_idiom1.cc +29]
10    IDIOM_1
	e0: UNLOCK  [51    target     0x6cb    sync_idiom1.cc +30]
	e1: LOCK    [55    target     0x6f2    sync_idiom1.cc +33]
11    IDIOM_1
	e0: UNLOCK  [28    target     0x74a    sync_idiom1.cc +44]
	e1: LOCK    [50    target     0x6be    sync_idiom1.cc +29]
12    IDIOM_1
	e0: UNLOCK  [30    target     0x764    sync_idiom1.cc +46]
	e1: LOCK    [53    target     0x6d8    sync_idiom1.cc +31]
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +36' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +31'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +36' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +35'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +36' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +55'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +32' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +31'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +32' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +35'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +53'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +33'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +44' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +46' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +31'):
            return True
    return False

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['sync_only'] = True

def setup_testcase(testcase):
    testcase.threshold = 1

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    if len(iroot_db.iroot_map) != 12:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

