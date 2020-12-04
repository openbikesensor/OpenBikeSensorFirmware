#include "unity.h"

//#include "globals.h" // FIXME: Should not be needed here

#define boolean bool
#define MAX_SENSOR_VALUE 999
#include <algorithm>
#include <cstring>
#include "utils/median.h"

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_function_median(void) {
  Median<int> m(5);

  m.addValue(1);
  m.addValue(2);
  m.addValue(3);
  m.addValue(4);
  m.addValue(5);
  TEST_ASSERT_EQUAL_MESSAGE(3 , 3, "Huh");
  m.median();
  TEST_ASSERT_EQUAL_INT32_MESSAGE(3, m.median(), "Huh");
  m.addValue(5);
  TEST_ASSERT_EQUAL(4, m.median());
  m.addValue(5);
  TEST_ASSERT_EQUAL(6, m.median());

}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_function_median);
  UNITY_END();
  return 0;
}