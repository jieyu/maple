#!/bin/bash
if [ -z $MAPLE ]; then
 echo "Variable MAPLE not set. Should point to your Maple kit root."
 exit
fi
pushd $1.tmp
iroot=`$MAPLE/script/idiom display test_history | grep Success | head -2 | tail -1 | awk '{print $1}'`
echo iroot = $iroot
rseed=`$MAPLE/script/idiom display test_history | grep Success | head -2 | tail -1 | awk '{print $NF}'`
echo rseed = $rseed
$MAPLE/script/idiom active --log --target_iroot=$iroot --random_seed=$rseed --- $1/main 2
mkdir -p failing.pinball
mv log* failing.pinball
popd
