#!/bin/bash
FILE=kernel
make $FILE
if test -f "./$FILE"; then
    valgrind --tool=helgrind ./$FILE
fi