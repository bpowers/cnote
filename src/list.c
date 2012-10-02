// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#include "common.h"
#include "list.h"

#include <string.h>


// opening and closing square brackets
static const int list_overhead = 2;

int
list_length(const struct list_head *self)
{
	struct list_head *curr;
	int len;

	len = list_overhead;

	list_for_each(curr, self) {
		const struct info *data = container_of(curr, struct info, list);
		// the +1 is for the trailing comma
		len += data->ops->length(data) + 1;
	}

	// FIXME: This will also behave weirdly in the 0-length case.
	// remove the last trailing comma, as per the json.org spec
	if (len > list_overhead)
		len -= 1;

	return len;
}


// opening and closing '{' and '}'
static const int dict_overhead = 2;
// quotes around key and value, ':' and ','
static const int str_overhead = 6;
#define member_len(self, field) \
	(strlen(#field) + strlen(self->data.song->field) + str_overhead)

int
info_length(const struct info *self)
{
	int len;
	// the added two are for matching '"'.
	if (self->type == STRING)
		return strlen(self->data.name) + 2;

	// otherwise we're a dict, or object
	len = dict_overhead;

	// otherwise we're a song;
	len += member_len(self, title);
	len += member_len(self, artist);
	len += member_len(self, album);
	len += member_len(self, track);
	len += member_len(self, path);

	// subtract one, because the last member doesn't have a trailing
	// comma.
	return len - 1;
}

#define member_jsonify(self, field) {				\
	*buf++ = '"';                                           \
	len = strlen(#field);					\
	strncpy(buf, #field, len);				\
	buf += len;						\
	*buf++ = '"';						\
	*buf++ = ':';						\
	*buf++ = '"';						\
	len = strlen(self->data.song->field);			\
	strncpy(buf, self->data.song->field, len);		\
	buf += len;						\
	*buf++ = '"';						\
	*buf++ = ',';						\
}
int
info_jsonify(const struct info *self, char *buf)
{
	int len;
	if (self->type == STRING) {
		*buf++ = '"';
		len = strlen(self->data.name);
		strncpy(buf, self->data.name, len);
		buf[len] = '"';
		return 0;
	}

	*buf++ = '{';

	member_jsonify(self, title);
	member_jsonify(self, artist);
	member_jsonify(self, album);
	member_jsonify(self, track);
	member_jsonify(self, path);

	*--buf = '}';

	return 0;
}

int
list_jsonify(const struct list_head *self, char *buf)
{
	int len;
	struct list_head *curr;

	*buf++ = '[';

	list_for_each(curr, self) {
		struct info *data = container_of(curr, struct info, list);
		// the +1 is for the trailing comma
		len = data->ops->length(data);
		data->ops->jsonify(data, buf);
		buf += len;
		*buf++ = ',';
	}

	// remove the last trailing comma, replacing it with the closing
	// bracket
	// FIXME: what if we're an empty list?  this fails.
	*--buf = ']';

	return len;
}
