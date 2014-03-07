#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "gtest/gtest.h"
#include "vector_clock.h"

class VectorClockTest: public ::testing::Test
{
protected:
    VectorClockTest()
    {
    }
    // void SetUp() { }
    // void TearDown() {  }
};


TEST_F(VectorClockTest, Increment)
{
    VectorClock vc1, vc2;

    ASSERT_EQ(vc1, vc2);
    ASSERT_LE(vc1, vc2);
    ASSERT_GE(vc1, vc2);

    ++vc1;

    ASSERT_NE(vc1, vc2);
    ASSERT_LT(vc2, vc1);
    ASSERT_LE(vc2, vc1);
    ASSERT_GT(vc1, vc2);
    ASSERT_GE(vc1, vc2);

    vc2 = vc1;

    ASSERT_EQ(vc1, vc2);
    ASSERT_LE(vc1, vc2);
    ASSERT_GE(vc1, vc2);

    ++vc1;
    ++vc2;

    ASSERT_NE(vc1, vc2);
    ASSERT_FALSE( vc1 < vc2 );
    ASSERT_FALSE( vc1 > vc2 );
}

TEST_F(VectorClockTest, Serialize)
{
    VectorClock vc1;
    for(int i=0; i<10; i++)
        ++vc1;

    std::string serialized = vc1.serialize();
    VectorClock vc2(vc1);
    VectorClock vc3(serialized);

    ASSERT_EQ(vc1, vc2);
    ASSERT_EQ(vc1, vc3);
}
