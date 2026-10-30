AM_CPPFLAGS = -I$(top_srcdir)/src
AM_LDFLAGS = $(extraLDFLAGS)
LDADD    = ../libaiori.a $(extraLDADD)

# Add test here
TESTS = testlib testexample testrand testmdtestoutput test-summary-output.sh test-ior-exit.sh test-mdtest-summary.sh
check_PROGRAMS = testlib testexample testrand testsummary testmdtestoutput
testexample_SOURCES  = example.c
testlib_SOURCES  = lib.c
testrand_SOURCES = rand.c
testsummary_SOURCES = summary.c
testmdtestoutput_SOURCES = mdtest-output.c
EXTRA_DIST = test-summary-output.sh test-summary-output.py test-ior-exit.sh test-ior-exit.py test-mdtest-summary.sh
