AM_LDFLAGS = $(extraLDFLAGS)
LDADD    = ../libaiori.a $(extraLDADD)

# Add test here
TESTS = testlib testexample test-summary-output.sh
check_PROGRAMS = testlib testexample testsummary
testexample_SOURCES  = example.c
testlib_SOURCES  = lib.c
testsummary_SOURCES = summary.c
EXTRA_DIST = test-summary-output.sh test-summary-output.py
