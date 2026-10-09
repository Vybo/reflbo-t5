#include <stdio.h>
#include <string.h>

#include "golden_io.h"
#include "unity.h"

/* golden_io.h (T3a): the T5's goldens are gzip-compressed PGMs, the RLCD's plain PBMs; one pair of calls for both. */

static uint8_t s_data[300000];
static uint8_t s_back[300000 + 16];

void setUp(void) {}
void tearDown(void) {}

static void fill(void)
{
    for (size_t i = 0; i < sizeof(s_data); i++) {
        s_data[i] = (uint8_t)((i / 960) % 3 == 0 ? 0x00 : (i % 7 == 0 ? 0x77 : 0xFF));
    }
}

static long file_size(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return n;
}

static void test_a_gz_path_round_trips_compressed(void)
{
    fill();
    TEST_ASSERT_TRUE(golden_write("golden_io_test.pgm.gz", s_data, sizeof(s_data)));
    TEST_ASSERT_EQUAL_size_t(sizeof(s_data), golden_read("golden_io_test.pgm.gz", s_back, sizeof(s_back)));
    TEST_ASSERT_EQUAL_MEMORY(s_data, s_back, sizeof(s_data));
    TEST_ASSERT_TRUE(file_size("golden_io_test.pgm.gz") < 10000);
}

static void test_a_plain_path_round_trips_uncompressed(void)
{
    fill();
    TEST_ASSERT_TRUE(golden_write("golden_io_test.pbm", s_data, 1000));
    TEST_ASSERT_EQUAL_size_t(1000, golden_read("golden_io_test.pbm", s_back, sizeof(s_back)));
    TEST_ASSERT_EQUAL_MEMORY(s_data, s_back, 1000);
    TEST_ASSERT_EQUAL_INT(1000, (int)file_size("golden_io_test.pbm"));
}

static void test_a_missing_file_reads_nothing(void)
{
    TEST_ASSERT_EQUAL_size_t(0, golden_read("golden_io_missing.pgm.gz", s_back, sizeof(s_back)));
    TEST_ASSERT_EQUAL_size_t(0, golden_read("golden_io_missing.pbm", s_back, sizeof(s_back)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_gz_path_round_trips_compressed);
    RUN_TEST(test_a_plain_path_round_trips_uncompressed);
    RUN_TEST(test_a_missing_file_reads_nothing);
    return UNITY_END();
}
