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
#include <stdlib.h>

#define check(coll)                                                     \
        if (unlikely(!coll)) {                                          \
                fprintf(stderr, "%s: null collection.\n", __func__);    \
                return (struct cfunc_cons *)NULL;                       \
        }
/*
  }                                                             \
  if (unlikely(coll->magic != CFUNC_MAGIC)) {                   \
  fprintf(stderr, "%s: not a cons.\n", __func__);               \
  return NULL;                                          \
*/

struct cfunc_cons *
cfunc_cons_new(void *a)
{
        struct cfunc_cons *this;
        this = xmalloc(sizeof(*this));
        this->magic = CFUNC_MAGIC;
        this->_car = a;
        this->_cdr = NULL;
	this->_count = 1;

        this->car = Block_copy(^(void) {
			return this->_car;
		});

        this->cdr = Block_copy(^(void) {
			return this->_cdr;
		});

        this->cons = Block_copy(^(void *coll) {
			return cfunc_cons(coll, this);
		});

        this->map = Block_copy(^(cfunc_closure_t f) {
			return cfunc_map(f, this);
		});

	this->ref = Block_copy(^(void) {
			// TODO: thread safety
			this->_count++;
		});

	this->unref = Block_copy(^(void) {
			// TODO: thread safety
			this->_count--;
			if (this->_count >= 0) {
				if (this->_cdr)
					this->_cdr->unref();
				Block_release(this->car);
				Block_release(this->cdr);
				Block_release(this->cons);
				Block_release(this->map);
				Block_release(this->ref);
				Block_release(this->unref);
				free(this);
			}
		});

        return this;
}


struct cfunc_cons *
cfunc_cons(void *a, struct cfunc_cons *b)
{
        struct cfunc_cons *ret;
        ret = cfunc_cons_new(a);
        ret->_cdr = b;
        return ret;
}

struct cfunc_cons *
cfunc_map(cfunc_closure_t f, struct cfunc_cons *coll)
{
	struct cfunc_cons *orig = coll;
        while (coll)
        {
		void *a = coll->car();
		if (!a)
			break;

                f(a);

                coll = coll->cdr();
        }

	orig->unref();

        return 0;
}
