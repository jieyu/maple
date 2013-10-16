#!/bin/bash
pushd $1.tmp
$PIN_ROOT/pin -t $PIN_ROOT/extras/pinplay/bin/intel64/pinplay-driver.so -replay -replay:addr_trans -replay:basename failing.pinball/log -- /bin/true
popd
