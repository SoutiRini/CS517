#!/bin/bash

gcc -fprofile-arcs -ftest-coverage decompress.c -g -o decompress

for i in `seq 0 999`;
do
	rm *gcda*
	./decompress $i
	gcov decompress
	mv decompress.c.gcov decompress.c.gcov.$i
done    