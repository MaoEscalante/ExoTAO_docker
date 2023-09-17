#!/bin/bash

# cd lib

# export LD_LIBRARY_PATH=lib/

cd lib
gcc -o mlib.o -c lib.c
ar rcs mlib.a mlib.o
cd ..


gcc -c  program.c  -o program.o



gcc -o build/program program.o lib/mlib.a 

cd build

./program
