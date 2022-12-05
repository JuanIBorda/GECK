#!/bin/bash
FILE=memoria
make $FILE
if test -f "./$FILE"; then
    valgrind --tool=helgrind ./$FILE
fi