// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#ifndef _LIST_H_
#define _LIST_H_

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

#define list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

// XXX: not thread safe.
static inline void
list_add(struct list_head *curr, struct list_head *new)
{
	struct list_head *last;
	last = curr->prev;
	last->next = new;
	new->prev = last;
	new->next = curr;
	curr->prev = new;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

// from linux kernel
#define container_of(ptr, type, member) ({		     \
	const typeof(((type *)0)->member) *__mptr = (ptr);   \
	(type *)((char *)__mptr - offsetof(type,member) );})

struct info;
struct json_ops {
	int (*length)(struct info *self);
	int (*jsonify)(struct info *self, char *buf);
};

enum INFO_TYPE {
	STRING,
	SONG
};

struct song {
	char *title;
	char *artist;
	char *album;
	char *track;
	char *path;
};

struct info {
	struct json_ops *ops;
	struct list_head list;
	enum INFO_TYPE type;
	union {
		char *name;
		struct song *song;
	} data;
};

int list_length(struct list_head *self);
int info_length(struct info *self);
int jsonify(struct info *self, char *buf);

int list_length(struct list_head *self);
int list_jsonify(struct list_head *self, char *buf);

#endif // _LIST_H_
