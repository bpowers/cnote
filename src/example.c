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

double cfunc_map(cfunc_closure_t f, cfunc_cons_t *coll)
{
  void *a = coll->car;
  cfunc_cons_t *c = coll;
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

