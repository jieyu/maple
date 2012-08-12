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
import socket

def host_name():
    return socket.gethostname().split('.')[0]

def tool_name():
    return 'MAPLE'

def pin_home():
    if not os.environ.has_key('PIN_HOME'):
        sys.exit('please specify the environment variable PIN_HOME')
    else:
        return os.environ['PIN_HOME']

def pkg_home():
    path = __file__
    return path[0:path.rfind('/script')]

def build_home(debug):
    if debug:
        return pkg_home() + '/build-debug'
    else:
        return pkg_home() + '/build-release'

def regression_home():
    return pkg_home() + '/test'

def cxx():
    return '/usr/bin/g++'

def cc():
    return '/usr/bin/gcc'

def benchmark_home(name):
    bench_home = {}
    if host_name() == 'zodiac':
        bench_home['splash2'] = '/opt/splash/splash2'
        bench_home['aget'] = '/opt/aget/aget-devel'
        bench_home['pfscan'] = '/opt/pfscan/pfscan-1.0-ctest'
        bench_home['memcached'] = '/opt/memcached/memcached-1.4.4'
    if host_name() == 'eden':
        bench_home['memcached'] = '/opt/benchmark/memcached/memcached-1.4.4'
    if host_name() == 'd-106-219':
        bench_home['httperf'] = '/opt/httperf/httperf-0.9.0'
        bench_home['apache'] = '/opt/project/eden-software/bugs/apache/httpd-2.0.48'
        bench_home['mysql'] = '/opt/bug/mysql-bug-791/mysql-4.0.12'
        bench_home['pbzip2_bug_unknown'] = '/opt/project/eden-software/apps/pbzip2-0.9.5'
        bench_home['mysql_bug_791'] = '/opt/bug/mysql-bug-791/mysql-4.0.12'
    if host_name() == 'bowser':
        bench_home['splash2'] = '/x/jieyu/benchmark/splash2'
        bench_home['parsec_1_0'] = '/x/jieyu/benchmark/parsec/parsec-1.0'
        bench_home['aget'] = '/x/jieyu/benchmark/aget/aget-devel'
        bench_home['pfscan'] = '/x/jieyu/benchmark/pfscan/pfscan-1.0'
        bench_home['pbzip2'] = '/x/jieyu/benchmark/pbzip2/pbzip2-0.9.5'
        bench_home['memcached'] = '/x/jieyu/benchmark/memcached/memcached-1.4.4'
        bench_home['apache'] = '/x/jieyu/bug/apache-bug-25520/httpd-2.0.48'
        bench_home['httperf'] = '/x/jieyu/tool/httperf/httperf-0.9.0'
        bench_home['pbzip2_bug'] = '/x/jieyu/bug/pbzip2-bug/pbzip2-0.9.4'
        bench_home['apache_bug_25520'] = '/x/jieyu/bug/apache-bug-25520/httpd-2.0.48'
        bench_home['memcached_bug_127'] = '/x/jieyu/benchmark/memcached/memcached-1.4.4'
        bench_home['memcached_bug_unknown'] = '/x/jieyu/benchmark/memcached/memcached-1.4.4'
        bench_home['aget_bug1'] = '/x/jieyu/bug/aget-bug1/aget-devel'
        bench_home['aget_bug2'] = '/x/jieyu/bug/aget-bug2/aget-devel2'
        bench_home['cnc_bug'] = '/x/jieyu/bug/cnc-bug/cncbw'
    if not name in bench_home:
        print 'benchmark home not found'
        sys.exit(-1)
    return bench_home[name]

