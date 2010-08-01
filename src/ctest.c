#define CFUNC_CLOBBER
#include <cfunc/cfunc.h>
#include <stdio.h>
#include <stdlib.h>


int
main(int argc, const char *argv[])
{
	char *x = "x";

	struct cfunc_cons *c1 = cons("z", list("x", "y"));

	map((cfunc_closure_t)^(char *str) {
			fprintf(stdout, "map: %s\n", str);
			return NULL;
		}, c1);
	

	return 0;
}
