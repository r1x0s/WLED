// Minimal assertion helpers for WLED-LightMusic host tests (plain g++, no framework).
#ifndef LIGHTMUSIC_TEST_COMMON_H
#define LIGHTMUSIC_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>

static int g_lm_failures = 0;
static int g_lm_checks = 0;

#define LM_CHECK(cond) do { \
  g_lm_checks++; \
  if (!(cond)) { g_lm_failures++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define LM_CHECK_EQ(a, b) do { \
  g_lm_checks++; \
  if (!((a) == (b))) { g_lm_failures++; \
    fprintf(stderr, "FAIL %s:%d: %s == %s (got %ld vs %ld)\n", __FILE__, __LINE__, #a, #b, (long)(a), (long)(b)); } \
} while (0)

static int lm_test_summary(const char* name) {
  if (g_lm_failures) { fprintf(stderr, "%s: %d of %d checks FAILED\n", name, g_lm_failures, g_lm_checks); return 1; }
  printf("%s: %d checks passed\n", name, g_lm_checks);
  return 0;
}

#endif
