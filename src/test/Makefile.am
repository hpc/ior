AM_LDFLAGS = $(extraLDFLAGS)
LDADD    = ../libaiori.a $(extraLDADD)

# Add test here
TESTS = testlib testexample testrand test-summary-output.sh test-ior-exit.sh test-op-timer.sh
check_PROGRAMS = testlib testexample testrand testsummary
testexample_SOURCES  = example.c
testlib_SOURCES  = lib.c
testrand_SOURCES = rand.c
testsummary_SOURCES = summary.c
EXTRA_DIST = test-summary-output.sh test-summary-output.py test-ior-exit.sh test-ior-exit.py test-op-timer.sh test-op-timer.py
