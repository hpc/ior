
DEFAULT_INCLUDES =

# We define the global AM_CPPFLAGS as everything we compile includes from these
# directories.
AM_CPPFLAGS = \
          -I$(top_srcdir)/libs/googletest/googletest/include \
          -I$(top_srcdir)/libs/googletest/googletest

AM_CXXFLAGS = -pthread
AM_LIBS = -pthread


check_LTLIBRARIES = libgtest.la libgtest_main.la
libgtest_la_SOURCES = ../libs/googletest/googletest/src/gtest-all.cc
libgtest_main_la_SOURCES = ../libs/googletest/googletest/src/gtest_main.cc

check_PROGRAMS =  iorTest


# aioriTest_SOURCES = gTest.cpp
# aioriTest_LDADD = libgtest.la libgtest_main.la

iorTest_SOURCES = iorTest.cpp
iorTest_CPPFLAGS = $(AM_CPPFLAGS) -fpermissive
iorTest_LDADD = libgtest.la
# iorTest_LDADD = libgtest.la libgtest_main.la


TESTS =   iorTest
