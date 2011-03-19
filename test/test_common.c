/*===--- common tests ---------------------------------------------------===//
 *
 * Copyright 2011 Bobby Powers
 * Licensed under the GPLv2 or GPLv3, see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include "test/test.h"
#include "utils.h"

#include <wordexp.h>
#include <stdio.h>
#include <stdlib.h>

void
setup(void)
{
}

void
teardown(void)
{
}


START_TEST(test_wordexp)
{
	wordexp_t p;
	char **w;
	size_t i;

	wordexp("~", &p, 0);
	w = p.we_wordv;
	for (i = 0; i < p.we_wordc; i++)
		printf("%s\n", w[i]);
	wordfree(&p);
}
END_TEST

Suite *
common_suite(void)
{
	Suite *s = suite_create("Common");
	TCase *tc_core = tcase_create("all");
	tcase_add_checked_fixture (tc_core, setup, teardown);
	tcase_add_test(tc_core, test_wordexp);
	suite_add_tcase(s, tc_core);

	return s;
}

RUN_TESTS(common_suite);
