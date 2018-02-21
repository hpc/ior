#include "gtest/gtest.h"
#include <mpi.h>
#pragma GCC diagnostic push
// permissive not working here therfore also set in the makefile
#pragma GCC diagnostic warning "-fpermissive"
#pragma GCC diagnostic ignored "-Wwrite-strings"


// Renames main() in ior.c to make room for the unit test main()
// Note: skatchy workaround for tow main functions
// right way would be to have the main of ior in a sperate file so everything
// else could be in a testable mobule/file
#define main IORMain
#include "../src/ior.c"

// This is also a scatchy workaround for not faking/mocking other modules
#include "../src/utilities.c"
#include "../src/parse_options.c"
#include "../src/aiori.c"
#include "../src/aiori-POSIX.c"
#include "../src/aiori-MPIIO.c"

#pragma GCC diagnostic pop

namespace {

    // The fixture for testing class Foo.
    class IORTest : public ::testing::Test {
     protected:
      // You can remove any or all of the following functions if its body
      // is empty.

      IORTest() {
        // You can do set-up work for each test here.
      }

      virtual ~IORTest() {
        // You can do clean-up work that doesn't throw exceptions here.
      }

      // If the constructor and destructor are not enough for setting up
      // and cleaning up each test, you can define the following methods:

      virtual void SetUp() {
        // Code here will be called immediately after the constructor (right
        // before each test).
      }

      virtual void TearDown() {
        // Code here will be called immediately after each test (right
        // before the destructor).
      }


      // Objects declared here can be used by all tests in the test case for Foo.
    };


    // Tests Sequential offsetarrays for single shared file
    // This test cheks the first, second and last offset of an block.
    // this is done with First and second rank as well as for the second rank
    // and 2 Segments
    TEST(GetOffsetArraySequentialTest, SingleFile) {

        IOR_offset_t *offsetArray;
        int pretendRank;
        int transfersPerBlock;
        int offsetLastTrasnfer;
        int rankOffset;
        int segmentOffset;
        int segments;
        int offsetIndex;
        // creat test with default initilized vaulues
        IOR_test_t test;
        init_IOR_Param_t(&test.params);

        transfersPerBlock = test.params.blockSize / test.params.transferSize;

        // Test for rank 0 in the first block
        /* initialize values */
        pretendRank = 0;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);
        // check that first offset is 0
        ASSERT_EQ(offsetArray[0], 0);
        // check second offset is one transferzize further
        ASSERT_EQ(offsetArray[1], test.params.transferSize);
        // check end of first block
        offsetLastTrasnfer = test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[transfersPerBlock-1] , offsetLastTrasnfer);

        free(offsetArray);


        // Test for rank 2 in the first block
        /* initialize values */
        pretendRank = 2;
        test.params.numTasks = 3;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);
        rankOffset = pretendRank * test.params.blockSize;
        // check that first offset is 0
        ASSERT_EQ(offsetArray[0], rankOffset + 0);
        // check second offset is one transferzize further
        ASSERT_EQ(offsetArray[1], rankOffset + test.params.transferSize);
        // check end of first block
        offsetLastTrasnfer = rankOffset +
                             test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[transfersPerBlock-1] , offsetLastTrasnfer);

        free(offsetArray);

        // Test for rank 2 in the first block in 3 segment
        /* initialize values */
        pretendRank = 2;
        test.params.numTasks = 3;
        segments = 2;
        test.params.segmentCount = segments;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);
        // callculate expected vvalues
        segmentOffset = (pretendRank + 1) * (segments - 1) * test.params.blockSize;
        rankOffset = pretendRank * test.params.blockSize;
        // check that first offset of rank 2 in  segment
        offsetIndex = ((segments - 1) * transfersPerBlock);
        ASSERT_EQ(offsetArray[offsetIndex], segmentOffset + rankOffset + 0);
        // check second offset is one transferzize further
        offsetIndex = ((segments - 1) * transfersPerBlock + 1);
        ASSERT_EQ(offsetArray[offsetIndex], segmentOffset + rankOffset + test.params.transferSize);
        // check end of first block
        offsetIndex = ((segments) * transfersPerBlock) -1 ;
        offsetLastTrasnfer = segmentOffset +
                             rankOffset +
                             test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[offsetIndex] , offsetLastTrasnfer);

        free(offsetArray);

    }

    // Tests Sequential offsetarrays for file per proc
    // This test cheks the first, second and last offset of an block.
    // this is done with First and second rank as well as for the second rank
    // and 2 Segments
    TEST(GetOffsetArraySequentialTest, FilePerProc) {

        IOR_offset_t *offsetArray;
        int pretendRank;
        int transfersPerBlock;
        int offsetLastTrasnfer;
        int rankOffset;
        int segmentOffset;
        int segments;
        int offsetIndex;
        // creat test with default initilized vaulues
        IOR_test_t test;
        init_IOR_Param_t(&test.params);
        test.params.filePerProc = 1;

        transfersPerBlock = test.params.blockSize / test.params.transferSize;

        // Test for rank 0 in the first block
        /* initialize values */
        pretendRank = 0;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);
        // check that first offset is 0
        ASSERT_EQ(offsetArray[0], 0);
        // check second offset is one transferzize further
        ASSERT_EQ(offsetArray[1], test.params.transferSize);
        // check end of first block
        offsetLastTrasnfer = test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[transfersPerBlock-1] , offsetLastTrasnfer);

        free(offsetArray);


        // Test for rank 2 in the first block
        /* initialize values */
        pretendRank = 2;
        test.params.numTasks = 3;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);
        // check that first offset is 0
        ASSERT_EQ(offsetArray[0], rankOffset + 0);
        // check second offset is one transferzize further
        ASSERT_EQ(offsetArray[1], test.params.transferSize);
        // check end of first block
        offsetLastTrasnfer = test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[transfersPerBlock-1] , offsetLastTrasnfer);

        free(offsetArray);

        // Test for rank 2 in the first block in 3 segment
        /* initialize values */
        pretendRank = 2;
        test.params.numTasks = 3;
        segments = 2;
        test.params.segmentCount = segments;
        // call funktion
        offsetArray = GetOffsetArraySequential(&test.params, pretendRank);

        // callculate expected vvalues
        segmentOffset = (segments - 1) * test.params.blockSize;
        // check that first offset of rank 2 in  segment
        offsetIndex = ((segments - 1) * transfersPerBlock);
        ASSERT_EQ(offsetArray[offsetIndex], segmentOffset + rankOffset + 0);
        // check second offset is one transferzize further
        offsetIndex = ((segments - 1) * transfersPerBlock + 1);
        ASSERT_EQ(offsetArray[offsetIndex], segmentOffset + test.params.transferSize);
        // check end of first block
        offsetIndex = ((segments) * transfersPerBlock) -1 ;
        offsetLastTrasnfer = segmentOffset +
                             test.params.blockSize - test.params.transferSize;
        ASSERT_EQ(offsetArray[offsetIndex] , offsetLastTrasnfer);

        free(offsetArray);

    }
}
