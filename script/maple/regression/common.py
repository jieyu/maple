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
from maple.core import config
from maple.core import logging
from maple.core import util

def cxx_ext():
    return '.cc'

def c_ext():
    return '.c'

def python_ext():
    return '.py'

def is_cxx_source(ext):
    return ext == cxx_ext()

def is_c_source(ext):
    return ext == c_ext()

def is_python_source(ext):
    return ext == python_ext()

def is_source(ext):
    return is_cxx_source(ext) or is_c_source(ext)

def is_script(ext):
    return ext == python_ext()

def is_package(suite):
    path = config.regression_home()
    for s in suite.split('.'):
        path = os.path.join(path, s)
    if os.path.exists(path) and os.path.isdir(path):
        return True
    return False

def is_testcase(suite):
    path = config.regression_home()
    for s in suite.split('.'):
        path = os.path.join(path, s)
    cxx_path = path + cxx_ext()
    c_path = path + c_ext()
    python_path = path + python_ext()
    source_exists = False
    script_exists = False
    if os.path.exists(cxx_path) and os.path.isfile(cxx_path):
        source_exists = True
    if os.path.exists(c_path) and os.path.isfile(c_path):
        source_exists = True
    if os.path.exists(python_path) and os.path.isfile(python_path):
        script_exists = True
    if source_exists and script_exists:
        return True
    else:
        return False

def testcase_name(suite):
    assert is_testcase(suite)
    return suite.split('.')[-1]

def source_path(suite):
    assert is_testcase(suite)
    path = config.regression_home()
    for s in suite.split('.'):
        path = os.path.join(path, s)
    cxx_path = path + cxx_ext()
    if os.path.exists(cxx_path) and os.path.isfile(cxx_path):
        return cxx_path
    c_path = path + c_ext()
    if os.path.exists(c_path) and os.path.isfile(c_path):
        return c_path
    return None

def script_path(suite):
    assert is_testcase(suite)
    path = config.regression_home()
    for s in suite.split('.'):
        path = os.path.join(path, s)
    python_path = path + python_ext()
    if os.path.exists(python_path) and os.path.isfile(python_path):
        return python_path
    return None

def list_subsuites(suite):
    assert is_package(suite)
    subsuites = []
    path = config.regression_home()
    for s in suite.split('.'):
        path = os.path.join(path, s)
    assert os.path.isdir(path)
    for f in os.listdir(path):
        if f.startswith('.'):
            continue # ignore hidden files and directories
        new_path = os.path.join(path, f)
        if os.path.isfile(new_path):
            name, ext = os.path.splitext(f)
            subsuite = suite + '.' + name
            if is_source(ext) and is_testcase(subsuite):
                subsuites.append(subsuite)
        if os.path.isdir(new_path):
            subsuite = suite + '.' + f
            subsuites.append(subsuite)
    return subsuites

def default_flags(suite):
    assert is_testcase(suite)
    name, ext = os.path.splitext(source_path(suite))
    if is_cxx_source(ext):
        return default_cxx_flags()
    elif is_cxx_source(ext):
        return default_c_flags()
    return None

def default_cxx_flags():
    return ['-g', '-Werror', '-fno-omit-frame-pointer', '-pthread']

def default_c_flags():
    return ['-g', '-Werror', '-fno-omit-frame-pointer', '-pthread']

def compile(source, target, flags, echo=False):
    name, ext = os.path.splitext(source)
    if is_cxx_source(ext):
        return util.cxx_compile(source, target, flags, echo)
    elif is_c_source(ext):
        return util.c_compile(source, target, flags, echo)
    return False

def echo(suite, message):
    logging.msg('regression %s - %s\n' % (suite, message))

