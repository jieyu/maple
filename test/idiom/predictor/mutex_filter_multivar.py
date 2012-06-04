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
        e0: UNLOCK  [59    target     0x787    mutex_filter_multivar.cc +31]
        e1: LOCK    [62    target     0x72e    mutex_filter_multivar.cc +38]
2     IDIOM_1
        e0: UNLOCK  [64    target     0x745    mutex_filter_multivar.cc +40]
        e1: LOCK    [55    target     0x766    mutex_filter_multivar.cc +28]
3     IDIOM_1
        e0: WRITE   [60    target     0x71a    mutex_filter_multivar.cc +37]
        e1: WRITE   [56    target     0x769    mutex_filter_multivar.cc +29]
4     IDIOM_1
        e0: WRITE   [56    target     0x769    mutex_filter_multivar.cc +29]
        e1: WRITE   [60    target     0x71a    mutex_filter_multivar.cc +37]
5     IDIOM_1
        e0: WRITE   [63    target     0x731    mutex_filter_multivar.cc +39]
        e1: WRITE   [57    target     0x773    mutex_filter_multivar.cc +30]
6     IDIOM_1
        e0: WRITE   [57    target     0x773    mutex_filter_multivar.cc +30]
        e1: WRITE   [63    target     0x731    mutex_filter_multivar.cc +39]
7     IDIOM_4
        e0: WRITE   [64    target     0x71a    mutex_filter_multivar.cc +37]
        e1: WRITE   [56    target     0x769    mutex_filter_multivar.cc +29]
        e2: UNLOCK  [59    target     0x787    mutex_filter_multivar.cc +31]
        e3: LOCK    [65    target     0x72e    mutex_filter_multivar.cc +38]
8     IDIOM_4
        e0: WRITE   [60    target     0x71a    mutex_filter_multivar.cc +37]
        e1: WRITE   [56    target     0x769    mutex_filter_multivar.cc +29]
        e2: WRITE   [57    target     0x773    mutex_filter_multivar.cc +30]
        e3: WRITE   [63    target     0x731    mutex_filter_multivar.cc +39]

(We should not predict 29->37...39->30)
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +31' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +38'):
            return True
        if (e0.is_mutex_unlock() and
            e0.inst().debug_info() == source_name() + ' +40' and
            e1.is_mutex_lock() and
            e1.inst().debug_info() == source_name() + ' +28'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +37' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +29' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +37'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +39' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +30'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +30' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +39'):
            return True
    elif r.idiom() == 4:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +37' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mutex_unlock() and
            e2.inst().debug_info() == source_name() + ' +31' and
            e3.is_mutex_lock() and
            e3.inst().debug_info() == source_name() + ' +38'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +37' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +29' and
            e2.is_mem_write() and
            e2.inst().debug_info() == source_name() + ' +30' and
            e3.is_mem_write() and
            e3.inst().debug_info() == source_name() + ' +39'):
            return True
    return False

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['complex_idioms'] = True
    profiler.knobs['vw'] = 2000

def setup_testcase(testcase):
    testcase.threshold = 2

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    if len(iroot_db.iroot_map) != 8:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

