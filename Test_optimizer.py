import re, sys
import z3
from z3 import *
import timeit


TOTAL_TESTS = 1000

start_time = timeit.default_timer()

print ("***********************************")

print ("Reading the gcov file for each test")
print ("***********************************")
print

# Reading the gcov file for each test and indexing the lines executed by each test and how many times
def process_file (fname):
    lines =[]
    f=open(fname ,"r")

    while True:
        l=f.readline ().rstrip ()
        m = re.search('(\d+):\s*(\d+):.*', l)
        if m!= None:
            lines.append(int(m.group (2)))
        if len(l)==0:
            break
    f.close ()
    return lines


#Indexing the lines with how many times they are executed across all test cases, and which test cases execute the line
stat = {}
for test in range(TOTAL_TESTS):
    stat[test]= process_file("compression.c.gcov."+str(test))


# list of all lines in all tests:
all_lines=set()
# k=line , v=list of tests , which trigger that line:
tests_for_line ={}

print ("*********************************************")

print ("Indexing test cases with their executed lines")
print ("*********************************************")
print
for test in stat:
    all_lines |=set(stat[test])
    for line in stat[test]:
        tests_for_line[line]= tests_for_line.get(line , []) + [test]


#Creating variables for each test case
tests =[Int('test_%d' % (t)) for t in range(TOTAL_TESTS)]


print ("******************")

print ("Starting optimizer")
print ("******************")
print


# Starting optmizer all test variables
opt = Optimize ()


# test variable value is either 0 (included) or 1 (not included):
for t in tests:
    opt.add(Or(t==0, t==1))


# SAT FORMULATIONS
# we have the lines that are triggered by each test
# so for each line of code we enumerate all tests
# expression is of the form "test_1 ==1 OR test_2 ==1 OR ..." for each line:
for line in list(all_lines):
    expression =[ tests[s]==1 for s in tests_for_line[line ]]
    # expression itself is a list of list which we tie up using the OR operator
    # expression turns out to be "Or(test_1 ==1, test_2 ==1, ...)"
    # This is the new constraint:
    opt.add(Or(*expression))


# The optimum solution is the minimal number of all "test_X" variables which have 1
# Thus, for each combination of tests (e.g. ars like [test_1 , test_2 , test_3 ,...]), the Sum(* tests) gives us the summation of all tests that hit at least one line
# The goal is that the sum of all "test_X" variables should be as minimum:
h=opt.minimize(Sum(* tests))


print ("*********************")
print ("Optimizer model used:")

print (opt.check ())
m=opt.model ()

print ("*********************")
print


print ("****************************************")

print ("Test cases that lead to maximum coverage:")

# print all variables set to 1:
for t in tests:
    if m[t]. as_long ()==1:
        print (t)


print ("****************************************")

print

print ("*****************************")

print ("Total time taken to optimize:")


elapsed = timeit.default_timer() - start_time
print(elapsed)
print ("*****************************")