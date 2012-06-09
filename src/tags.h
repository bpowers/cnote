// Copyright 2011 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#ifndef _TAGS_H_
#define _TAGS_H_

#include "dirwatch.h"


void *tags_init(sqlite3 *db);

bool is_valid_cb(struct dirwatch *self,
		 const char *path,
		 const char *dir,
		 const char *file);
bool is_modified_cb(struct dirwatch *self,
		    const char *path,
		    const char *dir,
		    const char *file);

void delete_cb(struct dirwatch *self,
	       const char *path,
	       const char *dir,
	       const char *file);
void change_cb(struct dirwatch *self,
	       const char *path,
	       const char *dir,
	       const char *file);

void cleanup_cb(struct dirwatch *self);

#endif // _TAGS_H_
