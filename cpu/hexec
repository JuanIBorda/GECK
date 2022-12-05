#!/bin/bash
FILE=cpu
make $FILE
if test -f "./$FILE"; then
    valgrind --tool=helgrind ./$FILE
fi