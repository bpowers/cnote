#include <cfunc/cfunc.h>
#include <stdio.h>
#include <stdlib.h>


int
main(int argc, const char *argv[])
{
	char *x = "x";
	char *y = "y";

	struct cfunc_cons *c1 = cfunc_cons(x, cfunc_cons(y, NULL));

	cfunc_map(^(void *a) {
			char *str = a;
			fprintf(stdout, "map: %s\n", str);
			return NULL;
		}, c1);
	

	return 0;
}
