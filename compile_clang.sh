#!/usr/bin/env bash

clang -dynamiclib -std=c99 -fPIC -O2 src/jc/memory.c -o libmemprofile.dylib

clang src/test/main.c