/*===--- common tests ---------------------------------------------------===//
 *
 * Copyright 2011 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _TEST_H_
#define _TEST_H_

#include <check.h>


#define RUN_TESTS(suite) int main(int argc __unused__, char *argv[] __unused__) {	\
	int number_failed;						\
									\
	verbosity += 2;							\
									\
	mkdir("test-data", S_755);					\
									\
	Suite *s = suite();						\
	SRunner *sr = srunner_create(s);				\
	srunner_set_fork_status(sr, CK_FORK);				\
	srunner_run_all(sr, CK_NORMAL);					\
									\
	number_failed = srunner_ntests_failed(sr);			\
	srunner_free(sr);						\
	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;	\
}


#endif // _TEST_H_
