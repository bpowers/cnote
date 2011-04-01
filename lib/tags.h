/*===--- tags.h - id3 tag manipulation functions ------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _TAGS_H_
#define _TAGS_H_

#include <cfunc/dirwatch.h>


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
