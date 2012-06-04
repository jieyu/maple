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
	e0: WRITE   [51    target     0x61e    mem_idiom4_vw.cc +32]
	e1: WRITE   [51    target     0x61e    mem_idiom4_vw.cc +32]
2     IDIOM_1
	e0: WRITE   [52    target     0x62d    mem_idiom4_vw.cc +34]
	e1: WRITE   [52    target     0x62d    mem_idiom4_vw.cc +34]

(We should not predict any idiom4 iroot because of vw)
"""

def source_name():
    return __name__ + common.cxx_ext()

def setup_profiler(profiler):
    profiler.knobs['ignore_lib'] = True
    profiler.knobs['complex_idioms'] = True
    profiler.knobs['vw'] = 50

def setup_testcase(testcase):
    testcase.threshold = 2

def verify(profiler, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(profiler.knobs['sinfo_out'])
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(profiler.knobs['iroot_out'])
    for r in iroot_db.iroot_map.itervalues():
        if r.idiom() == 4:
            logging.msg('idiom4 iroot should not be predicted\n')
            return False
    return True

