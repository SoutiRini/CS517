#!/bin/bash

gcc -fprofile-arcs -ftest-coverage compression.c -g -o compression

for i in `seq 0 999`;
do
	rm *gcda*
	./compression $i
	gcov compression
	mv compression.c.gcov compression.c.gcov.$i
done    