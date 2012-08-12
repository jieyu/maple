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
import subprocess
import optparse
from maple.core import config
from maple.core import logging
from maple.core import pintool
from maple.core import static_info
from maple.core import testing
from maple.pct import history as pct_history
from maple.race import pintool as race_pintool
from maple.idiom import iroot
from maple.idiom import memo
from maple.idiom import history as idiom_history
from maple.idiom import pintool as idiom_pintool
from maple.idiom import offline_tool as idiom_offline_tool
from maple.idiom import testing as idiom_testing

# global variables
_separator = '---'

def get_prefix(pin, tool=None):
    c = []
    c.append(pin.pin())
    c.extend(pin.options())
    if tool != None:
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

def __display_image_table(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    sinfo.display_image_table(output)

def __display_inst_table(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    sinfo.display_inst_table(output)

def __display_iroot_db(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    iroot_db.display(output)

def __display_memo_exposed_set(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_db.display_exposed_set(output)

def __display_memo_summary(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_db.display_summary(output)

def __display_test_history(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    test_history = idiom_history.TestHistory(sinfo, iroot_db)
    test_history.load(options.test_history)
    test_history.display(output)

def __display_test_history_summary(output, options):
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    history = idiom_history.TestHistory(sinfo, iroot_db)
    history.load(options.test_history)
    history.display_summary(output)

def __display_pct_history(output, options):
    history = pct_history.History()
    history.load(options.pct_history)
    history.display(output)

def __display_pct_history_summary(output, options):
    history = pct_history.History()
    history.load(options.pct_history)
    history.display_summary(output)

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
            '--iroot_in',
            action='store',
            type='string',
            dest='iroot_in',
            default='iroot.db',
            metavar='PATH',
            help='the input iroot database path')
    parser.add_option(
            '--iroot_out',
            action='store',
            type='string',
            dest='iroot_out',
            default='iroot.db',
            metavar='PATH',
            help='the output iroot database path')
    parser.add_option(
            '--memo_in',
            action='store',
            type='string',
            dest='memo_in',
            default='memo.db',
            metavar='PATH',
            help='the input memoization database path')
    parser.add_option(
            '--memo_out',
            action='store',
            type='string',
            dest='memo_out',
            default='memo.db',
            metavar='PATH',
            help='the output memoization database path')
    parser.add_option(
            '--test_history',
            action='store',
            type='string',
            dest='test_history',
            default='test.histo',
            metavar='PATH',
            help='the test history path')
    parser.add_option(
            '--pct_history',
            action='store',
            type='string',
            dest='pct_history',
            default='pct.histo',
            metavar='PATH',
            help='the PCT history path')

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

def __modify_memo_input_change(options):
    if not os.path.exists(options.memo_in):
        return
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_db.clear_predicted_set()
    memo_db.clear_candidate_map()
    memo_db.save(options.memo_out)
    logging.msg('memo input change done!\n')

def __modify_memo_mark_unexposed_failed(options):
    if not os.path.exists(options.memo_in):
        return
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_db.mark_unexposed_failed()
    memo_db.save(options.memo_out)
    logging.msg('memo mark unexposed failed done!\n')

def __modify_memo_refine_candidate(options):
    if not os.path.exists(options.memo_in):
        return
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_db.refine_candidate()
    memo_db.save(options.memo_out)
    logging.msg('memo refine candidate done!\n')

def __modify_memo_merge(options):
    if not os.path.exists(options.memo_in):
        return
    if not os.path.exists(options.memo_merge_in):
        return
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_merge_db = memo.Memo(sinfo, iroot_db)
    memo_merge_db.load(options.memo_merge_in)
    memo_db.merge(memo_merge_db)
    memo_db.save(options.memo_out)
    logging.msg('memo merge done!\n')

def __modify_memo_apply(options):
    if not os.path.exists(options.memo_in):
        return
    if not os.path.exists(options.memo_merge_in):
        return
    sinfo = static_info.StaticInfo()
    sinfo.load(options.sinfo_in)
    iroot_db = iroot.iRootDB(sinfo)
    iroot_db.load(options.iroot_in)
    memo_db = memo.Memo(sinfo, iroot_db)
    memo_db.load(options.memo_in)
    memo_merge_db = memo.Memo(sinfo, iroot_db)
    memo_merge_db.load(options.memo_merge_in)
    memo_db.merge(memo_merge_db)
    memo_db.refine_candidate()
    memo_db.save(options.memo_out)
    logging.msg('memo apply done!\n')

def valid_modify_set():
    result = set()
    for name in dir(sys.modules[__name__]):
        idx = name.find('__modify_')
        if idx != -1:
            result.add(name[idx+9:])
    return result

def valid_modify(modify):
    return modify in valid_modify_set()

def modify_usage():
    usage = 'usage: <script> modify [options] <object>\n\n'
    usage += 'valid objects are:\n'
    for modify in valid_modify_set():
        usage += '  %s\n' % modify
    return usage

def register_modify_options(parser):
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
            '--iroot_in',
            action='store',
            type='string',
            dest='iroot_in',
            default='iroot.db',
            metavar='PATH',
            help='the input iroot database path')
    parser.add_option(
            '--iroot_out',
            action='store',
            type='string',
            dest='iroot_out',
            default='iroot.db',
            metavar='PATH',
            help='the output iroot database path')
    parser.add_option(
            '--memo_in',
            action='store',
            type='string',
            dest='memo_in',
            default='memo.db',
            metavar='PATH',
            help='the input memoization database path')
    parser.add_option(
            '--memo_out',
            action='store',
            type='string',
            dest='memo_out',
            default='memo.db',
            metavar='PATH',
            help='the output memoization database path')
    parser.add_option(
            '--memo_merge_in',
            action='store',
            type='string',
            dest='memo_merge_in',
            default='memo_merge.db',
            metavar='PATH',
            help='the to-merge memoization database path')
    parser.add_option(
            '--no_memo_failed',
            action='store_false',
            dest='memo_failed',
            default=True,
            help='whether memorize fail-to-expose iroots')

def __command_modify(argv):
    parser = optparse.OptionParser(modify_usage())
    register_modify_options(parser)
    (options, args) = parser.parse_args(argv)
    if len(args) != 1 or not valid_modify(args[0]):
        parser.print_help()
        sys.exit(0)
    eval('__modify_%s(options)' % args[0])

def register_profile_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='runout',
            metavar='MODE',
            help='the profile mode: runout, timeout, stable')
    parser.add_option(
            '--%sthreshold' % prefix,
            action='store',
            type='int',
            dest='%sthreshold' % prefix,
            default=1,
            metavar='N',
            help='the threshold (depends on mode)')

def __command_profile(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_defaults['enable_observer_new'] = True
    profiler.knob_defaults['enable_predictor_new'] = True
    # parse cmdline options
    usage = 'usage: <script> profile [options] --- program'
    parser = optparse.OptionParser(usage)
    register_profile_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.ProfileTestCase(test,
                                             options.mode,
                                             options.threshold,
                                             profiler)
    testcase.run()

def register_active_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='runout',
            metavar='MODE',
            help='the active mode: runout, timeout, finish')
    parser.add_option(
            '--%sthreshold' % prefix,
            action='store',
            type='int',
            dest='%sthreshold' % prefix,
            default=1,
            metavar='N',
            help='the threshold (depends on mode)')

def __command_active(argv):
    pin = pintool.Pin(config.pin_home())
    scheduler = idiom_pintool.Scheduler()
    # parse cmdline options
    usage = 'usage: <script> active [options] --- program'
    parser = optparse.OptionParser(usage)
    register_active_cmdline_options(parser)
    scheduler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    scheduler.set_cmdline_options(options, args)
    # run active test
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, scheduler))
    testcase = idiom_testing.ActiveTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            scheduler)
    testcase.run()

def register_random_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='runout',
            metavar='MODE',
            help='the active mode: runout, timeout')
    parser.add_option(
            '--%sthreshold' % prefix,
            action='store',
            type='int',
            dest='%sthreshold' % prefix,
            default=1,
            metavar='N',
            help='the threshold (depends on mode)')

def __command_native(argv):
    # parse cmdline options
    usage = 'usage: <script> native [options] --- program'
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    testcase = idiom_testing.NativeTestCase(test,
                                            options.mode,
                                            options.threshold)
    testcase.run()

def __command_pinbase(argv):
    pin = pintool.Pin(config.pin_home())
    # parse cmdline options
    usage = 'usage: <script> pinbase [options] --- program'
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin))
    testcase = idiom_testing.NativeTestCase(test,
                                            options.mode,
                                            options.threshold)
    testcase.run()

def __command_pct(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_defaults['strict'] = True
    # parse cmdline options
    usage = 'usage: <script> pct [options] --- program'
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler)
    testcase.run()

def __command_pct_large(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    # parse cmdline options
    usage = 'usage: <script> pct_large [options] --- program'
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler)
    testcase.run()

def __command_rand_delay(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.RandSchedProfiler()
    profiler.knob_defaults['delay'] = True
    # parse cmdline options
    usage = 'usage: <script> rand_delay [options] --- program'
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    test = testing.InteractiveTest(prog_argv)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler)
    testcase.run()

def register_chess_cmdline_options(parser, prefix=''):
    parser.add_option(
            '--%smode' % prefix,
            action='store',
            type='string',
            dest='%smode' % prefix,
            default='finish',
            metavar='MODE',
            help='the active mode: finish, runout, timeout')
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
    controller = idiom_pintool.ChessProfiler()
    controller.knob_defaults['enable_chess_scheduler'] = True
    controller.knob_defaults['enable_observer_new'] = True
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
    testcase = idiom_testing.ChessTestCase(test,
                                           options.mode,
                                           options.threshold,
                                           controller)
    testcase.run()

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
    controller = idiom_pintool.ChessProfiler()
    controller.knob_prefix = 'chess_'
    controller.knob_defaults['enable_chess_scheduler'] = True
    controller.knob_defaults['enable_observer_new'] = True
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
    race_testcase = idiom_testing.RaceTestCase(race_test,
                                               options.race_mode,
                                               options.race_threshold,
                                               profiler)
    # create chess testcase
    chess_test = testing.InteractiveTest(prog_argv)
    chess_test.set_prefix(get_prefix(pin, controller))
    chess_testcase = idiom_testing.ChessTestCase(chess_test,
                                                 options.chess_mode,
                                                 options.chess_threshold,
                                                 controller)
    # run
    testcase = idiom_testing.ChessRaceTestCase(race_testcase,
                                               chess_testcase)
    testcase.run()

def __command_default(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_prefix = 'profile_'
    profiler.knob_defaults['enable_observer_new'] = True
    profiler.knob_defaults['enable_predictor_new'] = True
    scheduler = idiom_pintool.Scheduler()
    scheduler.knob_prefix = 'active_'
    # parse cmdline options
    usage = 'usage: <script> default [options] --- program'
    parser = optparse.OptionParser(usage)
    register_profile_cmdline_options(parser, 'profile_')
    profiler.register_cmdline_options(parser)
    register_active_cmdline_options(parser, 'active_')
    scheduler.register_cmdline_options(parser)
    parser.set_defaults(profile_mode='stable')
    parser.set_defaults(profile_threshold=3)
    parser.set_defaults(active_mode='finish')
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 0:
        parser.print_help()
        sys.exit(0)
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    scheduler.set_cmdline_options(options, args)
    # create profile testcase
    profile_test = testing.InteractiveTest(prog_argv)
    profile_test.set_prefix(get_prefix(pin, profiler))
    profile_testcase = idiom_testing.ProfileTestCase(profile_test,
                                                     options.profile_mode,
                                                     options.profile_threshold,
                                                     profiler)
    # create active testcase
    active_test = testing.InteractiveTest(prog_argv)
    active_test.set_prefix(get_prefix(pin, scheduler))
    active_testcase = idiom_testing.ActiveTestCase(active_test,
                                                   options.active_mode,
                                                   options.active_threshold,
                                                   scheduler)
    # run idiom testcase
    idiom_testcase = idiom_testing.IdiomTestCase(profile_testcase,
                                                 active_testcase)
    idiom_testcase.run()

def valid_benchmark_set():
    result = set()
    path = config.pkg_home() + '/script/maple/benchmark'
    for f in os.listdir(path):
        if f.endswith('.py'):
            if f != '__init__.py':
                result.add(f[:-3])
    return result

def valid_benchmark(bench):
    return bench in valid_benchmark_set()

def benchmark_usage():
    usage = 'valid benchmarks are:\n'
    for bench in valid_benchmark_set():
        usage += '  %s\n' % bench
    return usage

def __command_profile_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_defaults['enable_observer_new'] = True
    profiler.knob_defaults['enable_predictor_new'] = True
    # parse cmdline options
    usage = 'usage: <script> profile_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_profile_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.ProfileTestCase(test,
                                             options.mode,
                                             options.threshold,
                                             profiler) 
    testcase.run()

def __command_active_script(argv):
    pin = pintool.Pin(config.pin_home())
    scheduler = idiom_pintool.Scheduler()
    # parse cmdline options
    usage = 'usage: <script> active_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_active_cmdline_options(parser)
    scheduler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    scheduler.set_cmdline_options(options, args)
    # run active test
    __import__('maple.benchmark.%s' % bench_name)
    bench_mod = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench_mod.get_test(input_idx)
    test.set_prefix(get_prefix(pin, scheduler))
    testcase = idiom_testing.ActiveTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            scheduler)
    testcase.run()

def __command_native_script(argv):
    # parse cmdline options
    usage = 'usage: <script> native_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    testcase = idiom_testing.NativeTestCase(test,
                                            options.mode,
                                            options.threshold) 
    testcase.run()

def __command_pinbase_script(argv):
    pin = pintool.Pin(config.pin_home())
    # parse cmdline options
    usage = 'usage: <script> pinbase_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin))
    testcase = idiom_testing.NativeTestCase(test,
                                            options.mode,
                                            options.threshold) 
    testcase.run()

def __command_pct_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_defaults['strict'] = True
    # parse cmdline options
    usage = 'usage: <script> pct_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler) 
    testcase.run()

def __command_pct_large_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    # parse cmdline options
    usage = 'usage: <script> pct_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler) 
    testcase.run()

def __command_rand_delay_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.RandSchedProfiler()
    profiler.knob_defaults['delay'] = True
    # parse cmdline options
    usage = 'usage: <script> rand_delay_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_random_cmdline_options(parser)
    profiler.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    # run profile
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin, profiler))
    testcase = idiom_testing.RandomTestCase(test,
                                            options.mode,
                                            options.threshold,
                                            profiler) 
    testcase.run()

def __command_chess_script(argv):
    pin = pintool.Pin(config.pin_home())
    controller = idiom_pintool.ChessProfiler()
    controller.knob_defaults['enable_chess_scheduler'] = True
    controller.knob_defaults['enable_observer_new'] = True
    # parse cmdline options
    usage = 'usage: <script> chess_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_chess_cmdline_options(parser)
    controller.register_cmdline_options(parser)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    controller.set_cmdline_options(options, args)
    # run chess
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    test = bench.get_test(input_idx)
    test.set_prefix(get_prefix(pin, controller))
    testcase = idiom_testing.ChessTestCase(test,
                                           options.mode,
                                           options.threshold,
                                           controller) 
    testcase.run()

def __command_chess_race_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = race_pintool.PctProfiler()
    profiler.knob_prefix = 'race_'
    profiler.knob_defaults['enable_djit'] = True
    profiler.knob_defaults['ignore_lib'] = True
    profiler.knob_defaults['track_racy_inst'] = True
    controller = idiom_pintool.ChessProfiler()
    controller.knob_prefix = 'chess_'
    controller.knob_defaults['enable_chess_scheduler'] = True
    controller.knob_defaults['enable_observer_new'] = True
    controller.knob_defaults['sched_race'] = True
    # parse cmdline options
    usage = 'usage: <script> chess_race_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_race_cmdline_options(parser, 'race_')
    profiler.register_cmdline_options(parser)
    register_chess_cmdline_options(parser, 'chess_')
    controller.register_cmdline_options(parser)
    parser.set_defaults(race_mode='stable')
    parser.set_defaults(race_threshold=3)
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    controller.set_cmdline_options(options, args)
    # create race testcase
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    race_test = bench.get_test(input_idx)
    race_test.set_prefix(get_prefix(pin, profiler))
    race_testcase = idiom_testing.RaceTestCase(race_test,
                                               options.race_mode,
                                               options.race_threshold,
                                               profiler)
    # create chess testcase
    chess_test = bench.get_test(input_idx)
    chess_test.set_prefix(get_prefix(pin, controller))
    chess_testcase = idiom_testing.ChessTestCase(chess_test,
                                                 options.chess_mode,
                                                 options.chess_threshold,
                                                 controller)
    # run
    testcase = idiom_testing.ChessRaceTestCase(race_testcase,
                                               chess_testcase)
    testcase.run()

def __command_default_script(argv):
    pin = pintool.Pin(config.pin_home())
    profiler = idiom_pintool.PctProfiler()
    profiler.knob_prefix = 'profile_'
    profiler.knob_defaults['enable_observer_new'] = True
    profiler.knob_defaults['enable_predictor_new'] = True
    scheduler = idiom_pintool.Scheduler()
    scheduler.knob_prefix = 'active_'
    # parse cmdline options
    usage = 'usage: <script> default_script [options] --- <bench name> <input index>\n\n'
    usage += benchmark_usage()
    parser = optparse.OptionParser(usage)
    register_profile_cmdline_options(parser, 'profile_')
    profiler.register_cmdline_options(parser)
    register_active_cmdline_options(parser, 'active_')
    scheduler.register_cmdline_options(parser)
    parser.set_defaults(profile_mode='stable')
    parser.set_defaults(profile_threshold=3)
    parser.set_defaults(active_mode='finish')
    (opt_argv, prog_argv) = separate_opt_prog(argv)
    if len(prog_argv) == 1:
        bench_name = prog_argv[0]
        input_idx = 'default'
    elif len(prog_argv) == 2:
        bench_name = prog_argv[0]
        input_idx = prog_argv[1]
    else:
        parser.print_help()
        sys.exit(0)
    if not valid_benchmark(bench_name):
        logging.err('invalid benchmark name\n')
    (options, args) = parser.parse_args(opt_argv)
    profiler.set_cmdline_options(options, args)
    scheduler.set_cmdline_options(options, args)
    # create profile testcase
    __import__('maple.benchmark.%s' % bench_name)
    bench = sys.modules['maple.benchmark.%s' % bench_name]
    profile_test = bench.get_test(input_idx)
    profile_test.set_prefix(get_prefix(pin, profiler))
    profile_testcase = idiom_testing.ProfileTestCase(profile_test,
                                                     options.profile_mode,
                                                     options.profile_threshold,
                                                     profiler)
    # create active testcase
    active_test = bench.get_test(input_idx)
    active_test.set_prefix(get_prefix(pin, scheduler))
    active_testcase = idiom_testing.ActiveTestCase(active_test,
                                                   options.active_mode,
                                                   options.active_threshold,
                                                   scheduler)
    # run idiom testcase
    idiom_testcase = idiom_testing.IdiomTestCase(profile_testcase,
                                                 active_testcase)
    idiom_testcase.run()

def __command_memo_tool(argv):
    usage = 'usage: <script> memo_tool --operation=OP [options]'
    parser = optparse.OptionParser(usage)
    memo_tool = idiom_offline_tool.MemoTool()
    memo_tool.register_cmdline_options(parser)
    (options, args) = parser.parse_args(argv)
    memo_tool.set_cmdline_options(options, args)
    memo_tool.call()

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

