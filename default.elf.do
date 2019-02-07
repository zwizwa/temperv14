#!/bin/bash
redo-ifchange $2.c
gcc -MD -MF $2.d -o $3 $2.c -Os -lusb -Wall -Werror
read DEPS <$2.d
redo-ifchange ${DEPS#*:}

