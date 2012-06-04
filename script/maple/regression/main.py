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
from maple.core import config
from maple.core import logging
from maple.regression import common

def regression(suite, argv):
    top_level = suite.split('.')[0]
    __import__('maple.regression.%s' % top_level)
    module = sys.modules['maple.regression.%s' % top_level]
    module.main(suite, argv)

def find_suites(prefix, dir):
    suites = []
    for f in os.listdir(dir):
        if f.startswith('.'):
            continue # ignore hidden files and directories
        path = os.path.join(dir, f)
        if os.path.isfile(path):
            name, ext = os.path.splitext(f)
            suite = prefix + name
            if common.is_source(ext) and common.is_testcase(suite):
                suites.append(suite)
        if os.path.isdir(path):
            new_prefix = prefix + f + '.'
            suites.append(prefix + f)
            suites.extend(find_suites(new_prefix, path))
    return suites

def valid_suite_set():
    base_dir = config.regression_home()
    return find_suites('', base_dir)

def valid_suite(suite):
    return suite in valid_suite_set()

def command_usage():
    usage =  'usage: <script> <suite>\n\n'
    usage += 'valid suites are:\n'
    for suite in valid_suite_set():
        usage += '  %s\n' % suite
    return usage

def main(argv):
    if len(argv) < 1:
        logging.err(command_usage())
    suite = argv[0]
    logging.msg('performing regression: %s ...\n' % suite, 2) 
    if valid_suite(suite):
        regression(suite, argv[1:])
    else:
        logging.err(command_usage())

if __name__ == '__main__':
    main(sys.argv[1:])

