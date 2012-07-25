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
from maple.core import config
from maple.core import logging
from maple.core import static_info
from maple.core import pintool
from maple.core import testing
from maple.race import pintool as race_pintool
from maple.systematic import program
from maple.systematic import search
from maple.systematic import pintool as systematic_pintool
from maple.systematic import testing as systematic_testing

# global variables
_separator = '---'

def get_prefix(pin, tool):
    c = []
    c.append(pin.pin())
    c.extend(pin.options())
    c.extend(tool.options())
    c.append('--')
    return c

def separate_opt_prog(argv):
    if not _separator in argv:
        return argv, []
    else:
        opt_argv = argv[0:argv.index(_separator)]
        prog_argv = argv[argv.index(_separator)+1:]
        return opt_argv, prog_argv

def __display_thread_table(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    prog = program.Program(sinfo)
    prog.load(options.program_in)
    prog.display_thread_table(output)

def __display_object_table(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    prog = program.Program(sinfo)
    prog.load(options.program_in)
    prog.display_object_table(output)

def __display_search_info(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    prog = program.Program(sinfo)
    prog.load(options.program_in)
    search_info = search.SearchInfo(sinfo, prog)
    search_info.load(options.search_in)
    search_info.display(output)

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
            '--program_in',
            action='store',
            type='string',
            dest='program_in',
            default='program.db',
            metavar='PATH',
            help='the input database for the modeled program')
    parser.add_option(
            '--program_out',
            action='store',
            type='string',
            dest='program_out',
            default='program.db',
            metavar='PATH',
            help='the output database for the modeled program')
    parser.add_option(
            '--search_in',
            action='store',
            type='string',
            dest='search_in',
            default='search.db',
            metavar='PATH',
            help='the input file that contains the search information')
    parser.add_option(
            '--search_out',
            action='store',
            type='string',
            dest='search_out',
            default='search.db',
            metavar='PATH',
            help='the output file that contains the search information')

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

def register_chess_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='finish',
            metavar='MODE',
            help='the chess mode: finish, runout, timeout')
    parser.add_option(
            '--%sthreshold' % prefix,
            action='store',
            type='int',
            dest='%sthreshold' % prefix,
            default=1,
            metavar='N',
            help='the threshold (depends on mode)')

def __command_chess(argv):
    pin = pintool.Pin(config.pin_home())
    controller = systematic_pintool.Controller()
    controller.knob_defaults['enable_chess_scheduler'] = True
    # parse cmdline options
    usage = 'usage: <script> chess [options] --- program'
    parser = optparse.OptionParser(usage)
    register_chess_cmdline_options(parser)
    controller.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    controller.set_cmdline_options(options, args)
    # run chess
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, controller))
    testcase = systematic_testing.ChessTestCase(test,
                                                options.mode,
                                                options.threshold,
                                                controller)
    testcase.run()
    testcase.log_stat()

def register_race_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='runout',
            metavar='MODE',
            help='the race detector mode: runout, timeout, stable')
    parser.add_option(
            '--%sthreshold' % prefix,
            action='store',
            type='int',
            dest='%sthreshold' % prefix,
            default=1,
            metavar='N',
            help='the threshold (depends on mode)')

def __command_chess_race(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = race_pintool.PctProfiler()
    profiler.knob_prefix = 'race_'
    profiler.knob_defaults['enable_djit'] = True
    profiler.knob_defaults['ignore_lib'] = True
    profiler.knob_defaults['track_racy_inst'] = True
    controller = systematic_pintool.Controller()
    controller.knob_prefix = 'chess_'
    controller.knob_defaults['enable_chess_scheduler'] = True
    controller.knob_defaults['sched_race'] = True
    # parse cmdline options
    usage = 'usage: <script> chess_race [options] --- program'
    parser = optparse.OptionParser(usage)
    register_race_cmdline_options(parser, 'race_')
    profiler.register_cmdline_options(parser)
    register_chess_cmdline_options(parser, 'chess_')
    controller.register_cmdline_options(parser)
    parser.set_defaults(race_mode='stable')
    parser.set_defaults(race_threshold=3)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    controller.set_cmdline_options(options, args)
    # create race testcase
    race_test = testing.InteractiveTest(prog_argv)
    race_test.set_prefix(get_prefix(pin, profiler))
    race_testcase = systematic_testing.RaceTestCase(race_test,
                                                    options.race_mode,
                                                    options.race_threshold,
                                                    profiler)
    # create chess testcase
    chess_test = testing.InteractiveTest(prog_argv)
    chess_test.set_prefix(get_prefix(pin, controller))
    chess_testcase = systematic_testing.ChessTestCase(chess_test,
                                                      options.chess_mode,
                                                      options.chess_threshold,
                                                      controller)
    # run
    testcase = systematic_testing.ChessRaceTestCase(race_testcase,
                                                    chess_testcase)
    testcase.run()

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

