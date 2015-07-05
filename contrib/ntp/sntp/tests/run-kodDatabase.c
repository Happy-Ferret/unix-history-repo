/* AUTOGENERATED FILE. DO NOT EDIT. */

//=======Test Runner Used To Run Each Test Below=====
#define RUN_TEST(TestFunc, TestLineNum) \
{ \
  Unity.CurrentTestName = #TestFunc; \
  Unity.CurrentTestLineNumber = TestLineNum; \
  Unity.NumberOfTests++; \
  if (TEST_PROTECT()) \
  { \
      setUp(); \
      TestFunc(); \
  } \
  if (TEST_PROTECT() && !TEST_IS_IGNORED) \
  { \
    tearDown(); \
  } \
  UnityConcludeTest(); \
}

//=======Automagically Detected Files To Include=====
#include "unity.h"
#include <setjmp.h>
#include <stdio.h>

//=======External Functions This Runner Calls=====
extern void setUp(void);
extern void tearDown(void);
extern void test_SingleEntryHandling();
extern void test_MultipleEntryHandling();
extern void test_NoMatchInSearch();
extern void test_AddDuplicate();
extern void test_DeleteEntry();


//=======Test Reset Option=====
void resetTest()
{
  tearDown();
  setUp();
}

char *progname;


//=======MAIN=====
int main(int argc, char *argv[])
{
  progname = argv[0];
  Unity.TestFile = "kodDatabase.c";
  UnityBegin("kodDatabase.c");
  RUN_TEST(test_SingleEntryHandling, 22);
  RUN_TEST(test_MultipleEntryHandling, 35);
  RUN_TEST(test_NoMatchInSearch, 66);
  RUN_TEST(test_AddDuplicate, 79);
  RUN_TEST(test_DeleteEntry, 104);

  return (UnityEnd());
}
