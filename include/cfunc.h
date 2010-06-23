/*===--- cfunc.h - API for functional programming in C -------------------===//
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
 * This header defines the API for interacting with the cfunc runtime.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef __CFUNC_H__
#define __CFUNC_H__

#include <Block.h>

#include <stdbool.h>
#include <inttypes.h>


#ifdef __cplusplus
extern "C" {
#endif 

struct cfunc_cons;

struct cfunc_ops
{
  void *(*car)(struct cfunc_cons *);
  struct cfunc_cons *(*cdr)(struct cfunc_cons *);
};

struct cfunc_cons
{
  struct cfunc_ops *ops;
  void *car;
  struct cfunc_cons *cdr;
};

typedef void* (^cfunc_closure_t)(void*);


#ifdef __cplusplus
}
#endif 

#endif // __CFUNC_H__
