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
	e0: WRITE   [51    target     0x600    mem_idiom4.cc +28]
	e1: WRITE   [51    target     0x600    mem_idiom4.cc +28]
2     IDIOM_1
	e0: WRITE   [52    target     0x60a    mem_idiom4.cc +29]
	e1: WRITE   [52    target     0x60a    mem_idiom4.cc +29]
3     IDIOM_4
	e0: WRITE   [51    target     0x600    mem_idiom4.cc +28]
	e1: WRITE   [51    target     0x600    mem_idiom4.cc +28]
	e2: WRITE   [52    target     0x60a    mem_idiom4.cc +29]
	e3: WRITE   [52    target     0x60a    mem_idiom4.cc +29]
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +28' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +28'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +29' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +29'):
            return True
    elif r.idiom() == 4:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +28' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +28' and
            e2.is_mem_write() and
            e2.inst().debug_info() == source_name() + ' +29' and
            e3.is_mem_write() and
            e3.inst().debug_info() == source_name() + ' +29'):
            return True
    return False

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['complex_idioms'] = True

def setup_testcase(testcase):
    testcase.threshold = 2

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    if len(iroot_db.iroot_map) != 3:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

