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
from maple.systematic import program
from maple.systematic import search
from maple.regression import common

def source_name():
    return __name__ + common.cxx_ext()

def setup_controller(controller):
    controller.debug = True
    controller.knobs['por'] = False
    controller.knobs['fair'] = False
    controller.knobs['pb_limit'] = 1

def setup_testcase(testcase):
    testcase.mode = 'finish'

def verify(controller, testcase):
    sinfo = static_info.StaticInfo()
    sinfo.load(controller.knobs['sinfo_out'])
    prog = program.Program(sinfo)
    prog.load(controller.knobs['program_out'])
    search_info = search.SearchInfo(sinfo, prog)
    search_info.load(controller.knobs['search_out'])
    if search_info.num_runs() != 8:
        return False
    return True

