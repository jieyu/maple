#!/bin/bash
# Usage "replay1.sh benchmark [-appdebug]
# Optional "-appdebug" will prompt you for connecting to gdb
pushd $1.tmp
$PIN_HOME/pin $2 -t $PIN_HOME/extras/pinplay/bin/intel64/pinplay-driver.so -replay -replay:addr_trans -replay:basename failing.pinball/log -- /bin/true
popd
