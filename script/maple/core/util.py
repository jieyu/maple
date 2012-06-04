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
import signal
import subprocess
from maple.core import config
from maple.core import logging

def truncate(s, width):
    if len(s) <= width:
        return s
    else:
        return s[0:width]

def kill_process(pid):
    try:
        os.kill(proc.pid, signal.SIGKILL)
    except OSError:
        logging.msg('kill process %d error\n' % proc.pid)

def cxx_compile(source, target, flags, echo=False):
    cmd = []
    cmd.append(config.cxx())
    cmd.extend(['-o', target])
    cmd.extend(flags)
    cmd.append(source)
    fout = None
    ferr = None
    if not echo:
        fout = open(os.devnull, 'w')
        ferr = open(os.devnull, 'w')
    proc = subprocess.Popen(cmd, stdout=fout, stderr=ferr)
    retcode = proc.wait()
    if not echo:
        fout.close()
        ferr.close()
    if retcode != 0:
        return False
    else:
        return True

def c_compile(source, target, flags, echo=False):
    cmd = []
    cmd.append(config.cc())
    cmd.extend(['-o', target])
    cmd.extend(flags)
    cmd.append(source)
    fout = None
    ferr = None
    if not echo:
        fout = open(os.devnull, 'w')
        ferr = open(os.devnull, 'w')
    proc = subprocess.Popen(cmd, stdout=fout, stderr=ferr)
    retcode = proc.wait()
    if not echo:
        fout.close()
        ferr.close()
    if retcode != 0:
        return False
    else:
        return True

