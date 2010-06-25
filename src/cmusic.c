/*===--- exaample.c - Example cfunc program ------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * This file is part of cfunc.
 *
 * cfunc is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * cfunc is distributed in the hope that it will be useful, but WITHOUT
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cfunc.  If not, see <http://www.gnu.org/licenses/>.
 *
 *===--------------------------------------------------------------------===//
 *
 * Just an example of how to use cfunc.
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc.h>

#include <stddef.h>


void *
cfunc_car(struct cfunc_cons *coll)
{
	return coll->ops->car(coll);
}

struct cfunc_cons *
cfunc_cdr(struct cfunc_cons *coll)
{
	return coll->ops->cdr(coll);
}

struct cfunc_cons *
cfunc_map(cfunc_closure_t f, struct cfunc_cons *coll)
{
	void *a = cfunc_car(coll);
	struct cfunc_cons *c = coll;
	for (; a != NULL; a = c->car, c=c->cdr)
	{
		f(a);
	}
	return 0;
}

int main(int argc, const char *argv[])
{
	


	return 0;
}

