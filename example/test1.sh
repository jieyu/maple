#!/bin/bash
mkdir -p $1.tmp
pushd $1.tmp
rm -rf *
ln -sf ../$1 .
$MAPLE/maple_script --- $1 2>&1 > ../$1.maple.out
popd
