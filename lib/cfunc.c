/*===--- cfunc.c - functional programming in C --------------------------===//
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
 * Source that implements the cfunc runtime.
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc/cfunc.h>
#include <cfunc/common.h>
#include <stdio.h>

#define check(coll)							\
	if (unlikely(!coll)) {						\
		fprintf(stderr, "%s: null collection.\n", __func__);	\
		return NULL;						\
	}
/*
	}								\
	if (unlikely(coll->magic != CFUNC_MAGIC)) {			\
		fprintf(stderr, "%s: not a cons.\n", __func__);		\
		return NULL;						\
*/

struct cfunc_cons *
cfunc_cons_new(void *a)
{
	struct cfunc_cons *ret;
	ret = xmalloc(sizeof(*ret));
	ret->magic = CFUNC_MAGIC;
	ret->car = a;
	ret->cdr = NULL;

	return ret;
}


struct cfunc_cons *
cfunc_cons(void *a, struct cfunc_cons *b)
{
	struct cfunc_cons *ret;
	ret = cfunc_cons_new(a);
	ret->cdr = b;
	return ret;
}


void *
cfunc_car(struct cfunc_cons *coll)
{
	check(coll);
	
	return coll->car;
}

struct cfunc_cons *
cfunc_cdr(struct cfunc_cons *coll)
{
	check(coll);

	return coll->cdr;
}

struct cfunc_cons *
cfunc_map(cfunc_closure_t f, struct cfunc_cons *coll)
{
	void *a;
	while ((a = cfunc_car(coll)))
	{
		f(a);
		coll = cfunc_cdr(coll);
	}
	return 0;
}
