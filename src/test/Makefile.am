AM_LDFLAGS = $(extraLDFLAGS)
LDADD    = ../libaiori.a $(extraLDADD)

# Add test here
TESTS = testlib testexample testrand test-summary-output.sh test-ior-exit.sh test-directives.sh testdirectivevalues
check_PROGRAMS = testlib testexample testrand testsummary testdirectivevalues
testexample_SOURCES  = example.c
testlib_SOURCES  = lib.c
testrand_SOURCES = rand.c
testsummary_SOURCES = summary.c
testdirectivevalues_SOURCES = directive-values.c
EXTRA_DIST = test-summary-output.sh test-summary-output.py test-ior-exit.sh test-ior-exit.py test-directives.sh test-directives.py
