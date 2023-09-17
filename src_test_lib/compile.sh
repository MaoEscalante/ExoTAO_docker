#!/bin/bash

# cd lib

export LD_LIBRARY_PATH=lib/

cd lib
gcc -o mlib_so.so -fpic -shared lib.c
mv mlib_so.so mlibso.so
cd ..


gcc -c  program.c  -o program.o



gcc -o program program.o lib/mlibso.so -L. 

./program
