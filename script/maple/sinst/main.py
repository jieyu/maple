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
import optparse
from maple.core import logging
from maple.core import pintool
from maple.core import static_info
from maple.core import testing
from maple.sinst import sinst

def __display_sinst_db(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    sinst_db = sinst.SinstDB(sinfo)
    sinst_db.load(options.sinst_in)
    sinst_db.display(output)

def valid_display_set():
    result = set()
    for name in dir(sys.modules[__name__]):
        idx = name.find('__display_')
        if idx != -1:
            result.add(name[idx+10:])
    return result

def valid_display(display):
    return display in valid_display_set()

def display_usage():
    usage = 'usage: <script> display [options] <object>\n\n'
    usage += 'valid objects are:\n'
    for display in valid_display_set():
        usage += '  %s\n' % display
    return usage

def register_display_options(parser):
    parser.add_option(
            '-i', '--input',
            action='store',
            type='string',
            dest='input',
            default='stdin',
            metavar='PATH',
            help='the input file name')
    parser.add_option(
            '-o', '--output',
            action='store',
            type='string',
            dest='output',
            default='stdout',
            metavar='PATH',
            help='the output file name')
    parser.add_option(
            '--sinfo_in',
            action='store',
            type='string',
            dest='sinfo_in',
            default='sinfo.db',
            metavar='PATH',
            help='the input static info database path')
    parser.add_option(
            '--sinfo_out',
            action='store',
            type='string',
            dest='sinfo_out',
            default='sinfo.db',
            metavar='PATH',
            help='the output static info database path')
    parser.add_option(
            '--sinst_in',
            action='store',
            type='string',
            dest='sinst_in',
            default='sinst.db',
            metavar='PATH',
            help='the input shared inst database path')
    parser.add_option(
            '--sinst_out',
            action='store',
            type='string',
            dest='sinst_out',
            default='sinst.db',
            metavar='PATH',
            help='the output shared inst database path')

def __command_display(argv):
    parser = optparse.OptionParser(display_usage())
    register_display_options(parser)
    (options, args) = parser.parse_args(argv)
    if len(args) != 1 or not valid_display(args[0]):
        parser.print_help()
        sys.exit(0)
    # open output
    if options.output == 'stdout':
        output = sys.stdout
    elif options.output == 'stderr':
        output = sys.stderr
    else:
        output = open(options.output, 'w')
    # write output
    eval('__display_%s(output, options)' % args[0])
    # close output
    if options.output == 'stdout':
        pass
    elif options.output == 'stderr':
        pass
    else:
        output.close()

def valid_command_set():
    result = set()
    for name in dir(sys.modules[__name__]):
        idx = name.find('__command_')
        if idx != -1:
            result.add(name[idx+10:])
    return result

def valid_command(command):
    return command in valid_command_set()

def command_usage():
    usage =  'usage: <script> <command> [options] [args]\n\n'
    usage += 'valid commands are:\n'
    for command in valid_command_set():
        usage += '  %s\n' % command
    return usage

def main(argv):
    if len(argv) < 1:
        logging.err(command_usage())
    command = argv[0]
    logging.msg('performing command: %s ...\n' % command, 2) 
    if valid_command(command):
        eval('__command_%s(argv[1:])' % command)
    else:
        logging.err(command_usage())

if __name__ == '__main__':
    main(sys.argv[1:])

