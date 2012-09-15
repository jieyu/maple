# Maple User Guide

Maple is originally proposed as a testing tool for concurrent programs. During the development of Maple, we also built a dynamic analysis infrastructure as a foundation of Maple, which we believe will also be very useful for others to build their dynamic analysis tools for concurrent programs (e.g. data race detectors, atomicity violation detectors, etc.). In this user guide, we first show you how to use the Maple testing tool. After that, we discuss how to write dynamic analysis tools using our infrastructure. Maple is based on [PIN](http://www.pintool.org/) binary instrumentation tool, thus it can handle unmodified x86 binaries.

# 1. Testing Tool for Concurrent Programs

## 1.1. Build

### Supported OS

Currently, Maple is only supported on Linux platforms. We recommend to use 64-bit Linux machines. We have tested Maple on various Linux distributions, including the following.

* Redhat Enterprise Linux 5.4 (x86\_64)
* Ubuntu Desktop 10.10 (x86\_64)
* Ubuntu Desktop 11.04 (x86\_64)
* Ubuntu Desktop 12.04 (x86\_64)

### Software Dependencies

Maple depends on the following software.

* GNU make, version 3.81 or higher
* Python, version 2.4.3 or higher
* [PIN](http://www.pintool.org/), revision 45467 or higher
* [Google protobuf](http://code.google.com/p/protobuf/), version 2.4.1

### Make

First, you need to set two environment variables.

    $ export PIN_HOME=/path/to/pin/home
    $ export PROTOBUF_HOME=/path/to/protobuf/home

Then, you can build Maple by using make. By default, the debug version will be built. One can also choose to build the release version by specifying the compile type as follows.

    $ cd <maple_home>
    $ make
    $ make compiletype=release

Once the building finishes, two directories can be found in the source directory.

    $ cd <maple_home>
    $ ls
    build-debug build-release ...

## 1.2. Configure

Maple leverages the POSIX realtime priorities. In order to change realtime priorities on Linux platforms, the user needs to be granted appropriate permissions. To check that, you can use the `ulimit` command. Maple needs to set both the _scheduling priority_ and the _real-time priority_. The default values of the two limits are usually as follows.

    $ ulimit -e -r
    scheduling priority             (-e) 0
    real-time priority              (-r) 0

To set these two limits, you need the root permission. Add the following lines to the file `/etc/security/limits.conf` (Please replace `jieyu` with your user name).

    jieyu           hard    nice            -20
    jieyu           soft    nice            -20
    jieyu           hard    rtprio          99
    jieyu           soft    rtprio          99

Once you have done that, make sure to exit your shell and login again. Now, check the values of the two limits again.

    $ ulimit -e -r
    scheduling priority             (-e) 40
    real-time priority              (-r) 99

## 1.3. Target Programs

In theory, Maple can test any x86 binaries. Currently, however, Maple can only test those x86 binaries on Linux platforms that use pthread as threading functions. This is because we only monitor pthread functions in the current implementation.

Also, programmers should compile their test programs according to the following rules.

* Do NOT use static linking
* Preferably use `-fno-omit-frame-pointer` when compile, but not required
* Preferably use `-g` when compile, but not required

## 1.4. A Simple Buggy Program

Now, we use an example to quickly show you how to use Maple to test a concurrent program. Consider the following multithreaded program (the code can be found in `example/shared_counter/main.cc`).

    17 #include <stdio.h>
    18 #include <stdlib.h>
    19 #include <pthread.h>
    20 #include <assert.h>
    21
    22 unsigned NUM_THREADS = 1;
    23 unsigned global_count = 0;
    24 void *thread(void *);
    25
    26 int main(int argc, char *argv[]) {
    27   long i;
    28   pthread_t pthread_id[200];
    29   NUM_THREADS = atoi(argv[1]);
    30
    31   for(i = 0; i < NUM_THREADS; i++)
    32     pthread_create(&pthread_id[i], NULL, thread, (void *) i);
    33   for(i = 0; i < NUM_THREADS; i++)
    34     pthread_join(pthread_id[i], NULL);
    35
    36   assert(global_count==NUM_THREADS);
    37   return 0;
    38 }
    39
    40 void *thread(void * num) {
    41   unsigned temp = global_count;
    42   temp++;
    43   global_count = temp;
    44   return NULL;
    45 }

This program creates a few number of threads (specified by `NUM_THREADS`). Each thread will increment the shared variable `global_count` by `1`. In the end, the program verifies whether any update has been lost by using the assertion `assert(global_count==NUM_THREADS);`.

Obviously, this program has a data race on the shared variable `global_count`. When the bug is triggered, the assertion will fail. We want to see whether Maple can quickly find the bug and expose the buggy interleaving.

### Compile the Program

Compile the program using `g++` as you would do for any other programs. Remember to use `-g` and `-fno-omit-frame-pointer` as recommended. Suppose that the source file `main.cc` is in `~/example`.

    $ cd ~/example
    $ g++ -fno-omit-frame-pointer -g -pthread -o main main.cc

### Expose the Bug

Now, you can use Maple to test the program. Just run your program as you would do on a shell. The only difference is you need to add `maple ---` before the real command. This is similar to what [PIN](http://www.pintool.org/) does, but we use `---` as the separator while PIN uses `--` as the separator. Maple will then take care of everything and quickly produce an interleaving that triggers the bug.

    $ cd ~/example
    $ <maple_home>/maple --- ./main 2
    [MAPLE] === profile iteration 1 done === (1.618349) (/home/jieyu/example)
    [MAPLE] === profile iteration 2 done === (1.211759) (/home/jieyu/example)
    [MAPLE] === profile iteration 3 done === (1.212402) (/home/jieyu/example)
    [MAPLE] === profile iteration 4 done === (1.210453) (/home/jieyu/example)
    [MAPLE] === profile iteration 5 done === (1.211964) (/home/jieyu/example)
    [MAPLE] profile threshold reached
    main: main.cc:36: int main(int, char**): Assertion `global_count==NUM_THREADS' failed.
    [MAPLE] === active iteration 1 done === (0.807191) (/home/jieyu/example)
    [MAPLE] active fatal error detected
    [MAPLE]
    [MAPLE] ---------------------------
    [MAPLE] profile_runs    5
    [MAPLE] profile_time    6.530517
    [MAPLE] active_runs     1
    [MAPLE] active_time     0.814823

As can be seen, the bug is exposed by Maple in 5 profile runs and 1 active test run. Details about what have been done in profile runs and in active test runs can be found in our OOPSLA'12 [paper](http://www.eecs.umich.edu/~jieyu/papers/yu_oopsla12.pdf). Notice that a few files will be produced in the `~/example` directory. These files will be used by other components in Maple. So, you should not delete or modify them.

### Reproduce the Bug

Maple is able to reproduce the bug just exposed. To reproduce the bug, you need to know the iRoot (explained in our OOPSLA'12 paper) that exposes the bug. There is a script in the Maple package that can get the list of the iRoots that Maple has attempted.

    $ cd ~/example
    $ <maple_home>/script/idiom display test_history
    24    IDIOM_1 Success 1347667205

From the output, we know that only one active test run happens in the above example. The target iRoot number is `24` (first column), and the random seed (last column) is `1347667205`. Therefore, to reproduce the bug, we just need to feed the iRoot number and the random seed to Maple's active scheduler and rerun the program.

    $ cd ~/example
    $ <maple_home/script/idiom active --target_iroot=24 --random_seed=1347667205 --- ./main 2
    $ main: main.cc:36: int main(int, char**): Assertion `global_count==NUM_THREADS' failed.
    [MAPLE] === active iteration 1 done === (0.617391) (/home/jieyu/example)
    [MAPLE] active fatal error detected

### Control Maple's Behavior

In fact, the script `<maple_home>/maple` is a shortcut for `<maple_home>/script/idiom default`. `default` is a command which tells Maple to use the default mode to test the program (profiling + active testing). In the above example, `active` is another command which tells Maple to do active testing only. There are many other commands that can be used. To check that, please specify `--help` on the command line.

    $ <maple_home>/script/idiom --help
    [MAPLE] usage: <script> <command> [options] [args]

    valid commands are:
      ...
      profile
      active
      display
      default
      ...

Also, for each command, there are many options that can be used to control the behavior of Maple.

    $ <maple_home>/script/idiom active --help
    Usage: <script> active [options] --- program

    Options:
      -h, --help            show this help message and exit
      --mode=MODE           the active mode: runout, timeout, finish
      --threshold=N         the threshold (depends on mode)
      --ignore_lib          whether ignore accesses from common libraries
                            [default: False]
      --enable_debug        whether enable the debug analyzer [default: False]
      ...

### The Script Mode

Sometime, the program is not suitable for running directly from the command line (e.g. server programs). Also, it is likely that the program has side effects. Since Maple will rerun the program again and again, we need a way to specify the `setup()` and the `tear_down()` functions before and after each execution. Therefore, we introduce a script mode in Maple to test a program like this.

Let's still use the above example. To use the script mode, the programmer needs to create a simple Python script, and puts it into the directory `<maple_home>/script/maple/benchmark`. The content of the script is shown as follows.

    $ cat <maple_home>/script/maple/benchmark/shared_counter.py
    from maple.core import testing

    class Test(testing.CmdlineTest):
        def __init__(self, input_idx):
            testing.CmdlineTest.__init__(self, input_idx)
            self.add_input(([self.bin(), '2'], [None, None, None]))
        def setup(self):
            pass # do nothing
        def tear_down(self):
            pass # do nothing
        def bin(self):
            return '~/example/main'

    def get_test(input_idx='default'):
        return Test(input_idx)

Programmers can override the `setup()` and `tear_down()` functions if they want to do some initialization and cleanup jobs before and after each execution of the program.

After that, run the following command to verify that the script is available. The name of the benchmark should match the name of the script file.

    $ <maple_home>/maple_script --help
    Usage: <script> default_script [options] --- <bench name> <input index>

    valid benchmarks are:
      ...
      shared_counter
      ...

Now, to test the above program using Maple, simply run the following command. Remember to remove all the files previously produced by Maple in directory `~/example` first to prevent the current tests from being affected by the previous tests.

    $ cd ~/example
    $ <maple_home>/maple_script --- shared_counter
    [MAPLE] === profile iteration 1 done === (1.012269) (/home/jieyu/example)
    [MAPLE] === profile iteration 2 done === (1.005077) (/home/jieyu/example)
    [MAPLE] === profile iteration 3 done === (1.009388) (/home/jieyu/example)
    [MAPLE] === profile iteration 4 done === (1.011976) (/home/jieyu/example)
    [MAPLE] === profile iteration 5 done === (1.032873) (/home/jieyu/example)
    [MAPLE] profile threshold reached
    main: main.cc:36: int main(int, char**): Assertion `global_count==NUM_THREADS' failed.
    [MAPLE] === active iteration 1 done === (0.640238) (/home/jieyu/example)
    [MAPLE] active fatal error detected
    [MAPLE]
    [MAPLE] ---------------------------
    [MAPLE] profile_runs    5
    [MAPLE] profile_time    5.131157
    [MAPLE] active_runs     1
    [MAPLE] active_time     0.646243

Testing a server program is similar. Please refer to the scripts in `<maple_home>/script/maple/benchmark` for more information.

# 2. Dynamic Analysis Infrastructure

Before we developed Maple, we tried to find a PIN based dynamic analysis framework for concurrent programs, which provides some high level abstractions such as hooking pthread, malloc functions, etc., so that we can build Maple on top of that. The PIN interfaces are too low level for this purpose. But unfortunately, we could not find a framework that fits this need. Therefore, we decided to build our own infrastructure. Maple is built on top of our infrastructure.

Here, we use an example to illustrate how to use the infrastructure to write a dynamic analysis tool. The purpose of this guide is not to introduce every feature in our infrastructure, but rather show you some basic principles how to write dynamic analysis tools using the infrastructure so that you can look into the source code of the tools that we have written to understand all the interfaces.

## 2.1. A Simple Dynamic Analysis Tool

We want to build a tool that tells you whether all the child threads have been joined when program exits.

First, we create a directory in `<maple_home>/src`.

    $ cd <maple_home>/src
    $ mkdir example

Then, create a file `example.hpp` in the newly created directory. The content of this file is shown as follows.

    $ cd <maple_home>/src/example
    $ vi example.hpp
     1 #include "core/basictypes.h"
     2 #include "core/execution_control.hpp"
     3
     4 namespace example {
     5
     6 class ExampleAnalyzer : public Analyzer {
     7  public:
     8   ExampleAnalyzer() {}
     9   ~ExampleAnalyzer() {}
    10
    11   virtual void ProgramExit() {
    12     if (children_.empty()) {
    13       printf("All children are joined\n");
    14     } else {
    15       printf("Some children are not joined\n");
    16     }
    17   }
    18
    19   virtual void AfterPthreadCreate(thread_id_t curr_thd_id,
    20                                   timestamp_t curr_thd_clk,
    21                                   Inst *inst,
    22                                   thread_id_t child_thd_id) {
    23     children_.insert(child_thd_id);
    24   }
    25
    26   virtual void AfterPthreadJoin(thread_id_t curr_thd_id,
    27                                 timestamp_t curr_thd_clk,
    28                                 Inst *inst,
    29                                 thread_id_t child_thd_id) {
    30     children_.erase(child_thd_id);
    31   }
    32
    33  private:
    34   std::set<thread_id_t> children_;
    35 };
    36
    37 class Profiler : public ExecutionControl {
    38  public:
    39   Profiler() {}
    40   ~Profiler() {}
    41
    42   virtual void HandlePostSetup() {
    43     ExecutionControl::HandlePostSetup();
    44
    45     AddAnalyzer(new ExampleAnalyzer);
    46   }
    47 };
    48
    49 } // namespace example {

There are two classes defined in this file. Class `ExampleAnalyzer` (line 6) extends from the base class `Analyzer`. An instance of class `Analyzer` defines a set of hook functions which will be invoked when certain event happens. For example, the function defined in line 19 is a hook function which will be called right after `pthread_create` function returns. Remember that an _analyzer_ only observes the program execution. In other words, it never changes the execution of the program.

Class `Profiler` (line 37) extends from the base class `ExecutionControl` which controls the execution of the program. It is the core of our infrastructure. In this example, we only override one behavior by adding an analyzer `ExampleAnalyzer` before the program starts (line 45). Once the analyzer is added, the hook functions defined in it will be invoked when certain events happen during the execution. The `ExecutionControl` can be extended with other behaviors. We actually extend it to build a CHESS scheduler, which can be found in `<maple_home>/src/systematic`.

To make this simple analysis tool work, we need to add another source file shown as follows.

    $ cd <maple_home>/src/example
    $ vi example.cpp
    1 #include "example/example.hpp"
    2
    3 MAIN_ENTRY(example::Profiler);

The macro `MAIN_ENTRY` defines all the functions needed by PIN and connects them with the main `ExecutionControl` (in this example, it is `example::Profiler`).

Then, we need to create a build file `package.mk` so that the top level makefile can correctly build the analysis tool.

    $ cd <maple_home>/src/example
    $ vi package.mk
    1 srcs += example/example.cpp
    2
    3 pintools += example.so
    4
    5 example_objs := example/example.o $(core_objs)

Finally, we need to notify the top level makefile to build this analysis tool. We need to add an entry to the `packages` variable in the top level makefile.

    $ cd <maple_home>
    $ vi Makefile
    ...
    6 packages := core tracer sinst pct randsched race systematic idiom example
    ...

Now, we can build the analysis tool using make.

    $ cd <maple_home>
    $ make
    $ make compiletype=release

### Results

You can test the newly written analysis tool using the same example that we use throughout this guide.

    $ cd <maple_home>
    $ pin -t build-release/example.so -- ~/example/main 2
    All children are joined
