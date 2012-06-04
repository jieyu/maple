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
Expected Results (observed iroots)
----------------------------------
1     IDIOM_1
	e0: WRITE   [31    target     0x774    mem_idiom4.cc +27]
	e1: WRITE   [34    target     0x738    mem_idiom4.cc +37]
2     IDIOM_1
	e0: WRITE   [37    target     0x742    mem_idiom4.cc +38]
	e1: WRITE   [40    target     0x792    mem_idiom4.cc +30]
3     IDIOM_4
	e0: WRITE   [31    target     0x774    mem_idiom4.cc +27]
	e1: WRITE   [34    target     0x738    mem_idiom4.cc +37]
	e2: WRITE   [37    target     0x742    mem_idiom4.cc +38]
	e3: WRITE   [40    target     0x792    mem_idiom4.cc +30]
4     IDIOM_1
	e0: WRITE   [38    target     0x74c    mem_idiom4.cc +39]
	e1: WRITE   [41    target     0x79c    mem_idiom4.cc +31]
5     IDIOM_3
	e0: WRITE   [31    target     0x774    mem_idiom4.cc +27]
	e1: WRITE   [34    target     0x738    mem_idiom4.cc +37]
	e2: WRITE   [38    target     0x74c    mem_idiom4.cc +39]
	e3: WRITE   [41    target     0x79c    mem_idiom4.cc +31]
"""

def source_name():
    return __name__ + common.cxx_ext()

def iroot_is_expected(r):
    if r.idiom() == 1:
        e0 = r.event(0)
        e1 = r.event(1)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +27' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +37'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +38' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +30'):
            return True
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +39' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +31'):
            return True
    elif r.idiom() == 3:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +27' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +37' and
            e2.is_mem_write() and
            e2.inst().debug_info() == source_name() + ' +39' and
            e3.is_mem_write() and
            e3.inst().debug_info() == source_name() + ' +31'):
            return True
    elif r.idiom() == 4:
        e0 = r.event(0)
        e1 = r.event(1)
        e2 = r.event(2)
        e3 = r.event(3)
        if (e0.is_mem_write() and
            e0.inst().debug_info() == source_name() + ' +27' and
            e1.is_mem_write() and
            e1.inst().debug_info() == source_name() + ' +37' and
            e2.is_mem_write() and
            e2.inst().debug_info() == source_name() + ' +38' and
            e3.is_mem_write() and
            e3.inst().debug_info() == source_name() + ' +30'):
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
    if len(iroot_db.iroot_map) != 5:
        logging.msg('iroot_db size mismatch\n')
        return False
    for r in iroot_db.iroot_map.itervalues():
        if not iroot_is_expected(r):
            logging.msg('iroot mismatch\n')
            return False
    return True

