#include <cfunc/cfunc.h>
#include <stdio.h>
#include <stdlib.h>


int
main(int argc, const char *argv[])
{
	char *x = "x";
	char *y = "y";

	struct cfunc_cons *c1 = cfunc_cons_new(y)->cons(x);

	c1->map((cfunc_closure_t)^(char *str) {
			fprintf(stdout, "map: %s\n", str);
			return NULL;
		});
	

	return 0;
}
