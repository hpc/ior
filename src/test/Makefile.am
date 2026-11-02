AM_LDFLAGS = $(extraLDFLAGS)
LDADD    = ../libaiori.a $(extraLDADD)

# Add test here
TESTS = testlib testexample testrand test-summary-output.sh test-ior-exit.sh testtestcopy test-script-inheritance.sh
check_PROGRAMS = testlib testexample testrand testsummary testtestcopy
testexample_SOURCES  = example.c
testlib_SOURCES  = lib.c
testrand_SOURCES = rand.c
testsummary_SOURCES = summary.c
testtestcopy_SOURCES = test-copy.c
EXTRA_DIST = test-summary-output.sh test-summary-output.py test-ior-exit.sh test-ior-exit.py test-script-inheritance.sh test-script-inheritance.py
