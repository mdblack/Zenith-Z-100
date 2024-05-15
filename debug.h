// debug.h
// Margaret Black
// breakpoints based on code contributed by PorkyPiggy64
// 2024

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation                                           *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

void handleDebug(Z100*);
void debug_init();
void debug_start(Z100*);
void bp_out_check(unsigned int address);
void bp_in_check(unsigned int address);
void bp_read_check(unsigned int address);
void bp_write_check(unsigned int address);
void bp_exec_check(unsigned int address);

#ifndef LIST_H
#define LIST_H

#define import_list(type, container) \
\
	typedef struct _##container \
	{ \
		size_t size; \
		type *buffer; \
		const char *exptr; \
		char exception; \
		size_t (*list_size)(struct _##container *list); \
		type (*list_get)(struct _##container* list, size_t index); \
		type (*list_set)(struct _##container* list, size_t index, type value); \
		void (*list_add)(struct _##container* list, type value); \
		void (*list_insert)(struct _##container* list, size_t index, type value); \
		type (*list_remove)(struct _##container* list, size_t index); \
		void (*list_clear)(struct _##container* list); \
		void (*list_free)(struct _##container* list); \
	} _##container; \
	\
	typedef _##container *container; \
	\
	static void container##_exception(container list) \
	{ \
		list->exception = *list->exptr; \
	} \
	static size_t container##_size(container list) \
	{ \
		return list->size; \
	} \
	\
	static type container##_get(container list, size_t index) \
	{ \
		if (!list || !list->buffer || list->size <= index) \
		{ \
			container##_exception(list); \
		} \
		return list->buffer[index]; \
	} \
	\
	static type container##_set(container list, size_t index, type value) \
	{ \
		if (!list || !list->buffer || list->size <= index) \
		{ \
			container##_exception(list); \
		} \
		type original = list->buffer[index]; \
		list->buffer[index] = value; \
		return original; \
	} \
	\
	static void container##_add(container list, type value) \
	{ \
		if (!list) \
		{ \
			container##_exception(list); \
		} \
		type *newbuff = (type *)realloc(list->buffer, (list->size + 1) * sizeof(type)); \
		if (!newbuff) \
		{ \
			container##_exception(list); \
		} \
		list->buffer = newbuff; \
		list->buffer[list->size] = value; \
		list->size++; \
	} \
	\
	static void container##_insert(container list, size_t index, type value) \
	{ \
		if (!list || list->size < index) \
		{ \
			container##_exception(list); \
		} \
		container##_add(list, value); \
		memmove(&list->buffer[index + 1], &list->buffer[index], (list->size - index) * sizeof(type)); \
		list->buffer[index] = value; \
	} \
	\
	static type container##_remove(container list, size_t index) \
	{ \
		if (!list || !list->buffer || list->size <= index) \
		{ \
			container##_exception(list); \
		} \
		type original = list->buffer[index]; \
		memmove(&list->buffer[index], &list->buffer[index + 1], (list->size - index - 1) * sizeof(type)); \
		list->size--; \
		if (!list->size) \
		{ \
			free(list->buffer); \
			list->buffer = NULL; \
		} \
		else \
		{ \
			type *newbuff = (type *)realloc(list->buffer, list->size * sizeof(type)); \
			if (!newbuff) \
			{ \
				container##_exception(list); \
			} \
			list->buffer = newbuff; \
		} \
		return original; \
	} \
	\
	static void container##_clear(container list) \
	{ \
		if (!list) \
		{ \
			container##_exception(list); \
		} \
		free(list->buffer); \
		list->size = 0; \
		list->buffer = NULL; \
	} \
	\
	static void container##_free(container list) \
	{ \
		if (list) \
		{ \
			container##_clear(list); \
			free(list); \
		} \
	} \
	\
	static container new_##container() \
	{ \
		container result = (container)malloc(sizeof(_##container)); \
		if (result) \
		{ \
			result->buffer = NULL; \
			result->size = 0; \
			result->exptr = NULL; \
			result->exception = 0; \
			result->list_size = &container##_size; \
			result->list_get = &container##_get; \
			result->list_set = &container##_set; \
			result->list_add = &container##_add; \
			result->list_insert = &container##_insert; \
			result->list_remove = &container##_remove; \
			result->list_clear = &container##_clear; \
			result->list_free = &container##_free; \
		} \
		return result; \
	}

#define new(container) new_##container()

#define list_size(list) list->list_size(list)
#define list_get(list, index) list->list_get(list, index)
#define list_set(list, index, value) list->list_set(list, index, value)
#define list_add(list, value) list->list_add(list, value)
#define list_insert(list, index, value) list->list_insert(list, index, value)
#define list_remove(list, index) list->list_remove(list, index)
#define list_clear(list) list->list_clear(list)
#define list_free(list) list->list_free(list)

#endif /* LIST_H */
