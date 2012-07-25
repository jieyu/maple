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
import sys
import imp
import shutil
import subprocess
from maple.core import config
from maple.core import logging
from maple.core import pintool
from maple.core import testing
from maple.systematic import pintool as systematic_pintool
from maple.systematic import testing as systematic_testing
from maple.regression import common

def get_prefix(pin, tool):
    c = []
    c.append(pin.pin())
    c.extend(pin.options())
    c.extend(tool.options())
    c.append('--')
    return c

def clean_currdir():
    for f in os.listdir(os.getcwd()):
        if os.path.isfile(f):
            os.remove(f)
        if os.path.isdir(f):
            shutil.rmtree(f)

def systematic_chess(suite):
    assert common.is_testcase(suite)
    clean_currdir()
    testcase = common.testcase_name(suite)
    source_path = common.source_path(suite)
    script_path = common.script_path(suite)
    target_path = os.path.join(os.getcwd(), 'target')
    output_path = os.path.join(os.getcwd(), 'stdout')
    f, p, d = imp.find_module(testcase, [os.path.dirname(script_path)])
    module = imp.load_module(testcase, f, p, d)
    f.close()
    flags = common.default_flags(suite)
    if hasattr(module, 'disabled'):
        common.echo(suite, 'disabled!')
        return True
    if hasattr(module, 'setup_flags'):
        module.setup_flags(flags)
    if not common.compile(source_path, target_path, flags, True):
        common.echo(suite, 'failed! compile error')
        return False
    pin = pintool.Pin(config.pin_home())
    controller = systematic_pintool.Controller()
    controller.knobs['enable_chess_scheduler'] = True
    if hasattr(module, 'setup_controller'):
        module.setup_controller(controller)
    test = testing.InteractiveTest([target_path], sout=output_path)
    test.set_prefix(get_prefix(pin, controller))
    testcase = systematic_testing.ChessTestCase(test, 'finish', 1, controller)
    if hasattr(module, 'setup_testcase'):
        module.setup_testcase(testcase)
    logging.message_off()
    testcase.run()
    logging.message_on()
    if not hasattr(module, 'verify'):
        common.echo(suite, 'failed! no verify')
        return False
    else:
        success = module.verify(controller, testcase)
        if success:
            common.echo(suite, 'succeeded!')
        else:
            common.echo(suite, 'failed!')
        return success

def handle(suite):
    if common.is_package(suite):
        fail = False
        for subsuite in common.list_subsuites(suite):
            if not handle(subsuite):
                fail = True
        return not fail
    elif common.is_testcase(suite):
        handler_name = '_'.join(suite.split('.')[:-1])
        if not eval('%s(suite)' % handler_name):
            backupdir = os.getcwd() + '_' + suite
            if not os.path.exists(backupdir):
                shutil.copytree(os.getcwd(), backupdir)
            return False
        else:
            return True

def main(suite, argv):
    basedir = os.getcwd()
    workdir = os.path.join(basedir, 'regression-workdir')
    if not os.path.exists(workdir):
        os.mkdir(workdir)
    os.chdir(workdir)
    handle(suite)
    os.chdir(basedir)
    shutil.rmtree(workdir)

if __name__ == '__main__':
    main('systematic', sys.argv[1:])

