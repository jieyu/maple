#!/bin/bash
if [ -z $MAPLE ]; then
 echo "Variable MAPLE not set. Should point to your Maple kit root."
 exit
fi
mkdir -p $1.tmp
pushd $1.tmp
rm -rf *
ln -sf ../$1 .
$MAPLE/maple_script --- $1 2>&1 > ../$1.maple.out
popd
