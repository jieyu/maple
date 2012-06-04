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
	e0: UNLOCK  [53    target     0x775    mutex_filter2.cc +34]
	e1: LOCK    [46    target     0x734    mutex_filter2.cc +29]
2     IDIOM_1
	e0: UNLOCK  [51    target     0x768    mutex_filter2.cc +33]
	e1: LOCK    [47    target     0x741    mutex_filter2.cc +30]
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +34' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +33' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +30'):
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
    if len(iroot_db.iroot_map) != 2:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

