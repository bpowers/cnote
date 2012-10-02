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

static inline void
__list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void
list_del(struct list_head *curr)
{
	__list_del(curr->prev, curr->next);
	curr->prev = NULL;
	curr->next = NULL;
}

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

// from linux kernel
#define container_of(ptr, type, member) ({			\
	/*const*/ typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct info;
struct json_ops {
	int (*length)(const struct info *self);
	int (*jsonify)(const struct info *self, char *buf);
};

enum INFO_TYPE {
	INVALID,
	STRING,
	SONG,
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
#define to_info(n) container_of(n, struct info, list)

int list_length(const struct list_head *self);
int list_jsonify(const struct list_head *self, char *buf);
int info_length(const struct info *self);
int info_jsonify(const struct info *self, char *buf);

#endif // _LIST_H_
