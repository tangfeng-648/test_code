#ifndef _LIST_H_
#define _LIST_H_

#include <stdbool.h>
#include <stddef.h>
#include <assert.h>

#define list_assert(cond, msg)  assert(cond && msg)

struct list_head
{
    struct list_head *prev;
    struct list_head *next;
};

static inline void list_inithead(struct list_head *item)
{
    item->prev = item;
    item->next = item;
}

static inline void list_add(struct list_head *item, struct list_head *list)
{
    item->prev = list;
    item->next = list->next;
    list->next->prev = item;
    list->next = item;
}

static inline void list_addtail(struct list_head *item, struct list_head *list)
{
    item->next = list;
    item->prev = list->prev;
    list->prev->next = item;
    list->prev = item;
}

static inline bool list_is_empty(const struct list_head *list)
{
   return list->next == list;
}

static inline void list_replace(struct list_head *from, struct list_head *to)
{
    if (list_is_empty(from)) {
        list_inithead(to);
    } else {
        to->prev = from->prev;
        to->next = from->next;
        from->next->prev = to;
        from->prev->next = to;
    }
}

static inline void list_del(struct list_head *item)
{
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->prev = item->next = NULL;
}

static inline void list_delinit(struct list_head *item)
{
    item->prev->next = item->next;
    item->next->prev = item->prev;
    item->next = item;
    item->prev = item;
}

static inline bool list_is_linked(const struct list_head *list)
{
   assert((list->prev != NULL) == (list->next != NULL));

   return list->next != NULL;
}
static inline bool list_is_singular(const struct list_head *list)
{
   return list_is_linked(list) && !list_is_empty(list) && list->next->next == list;
}

static inline unsigned list_length(const struct list_head *list)
{
   struct list_head *node;
   unsigned length = 0;
   for (node = list->next; node != list; node = node->next)
      length++;
   return length;
}

static inline void list_splice(struct list_head *src, struct list_head *dst)
{
   if (list_is_empty(src))
      return;

   src->next->prev = dst;
   src->prev->next = dst->next;
   dst->next->prev = src->prev;
   dst->next = src->next;
}

static inline void list_splicetail(struct list_head *src, struct list_head *dst)
{
   if (list_is_empty(src))
      return;

   src->prev->next = dst;
   src->next->prev = dst->prev;
   dst->prev->next = src->next;
   dst->prev = src->prev;
}

static inline void list_validate(const struct list_head *list)
{
   struct list_head *node;
   assert(list_is_linked(list));
   assert(list->next->prev == list && list->prev->next == list);
   for (node = list->next; node != list; node = node->next)
      assert(node->next->prev == node && node->prev->next == node);
}

static inline void list_move_to(struct list_head *item, struct list_head *loc) {
   list_del(item);
   list_add(item, loc);
}

#define list_entry(__item, __type, __field)   \
    ((__type *)(((char *)(__item)) - offsetof(__type, __field)))

#define list_container_of(ptr, sample, member)                          \
    (void *)((char *)(ptr)                                              \
             - ((char *)&(sample)->member - (char *)(sample)))

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) \
        list_entry((ptr)->prev, type, member)

#define LIST_FOR_EACH_ENTRY(pos, head, member)                          \
   for (pos = NULL, pos = list_container_of((head)->next, pos, member); \
        &pos->member != (head);                                         \
        pos = list_container_of(pos->member.next, pos, member))

#define LIST_FOR_EACH_ENTRY_SAFE(pos, storage, head, member)    \
   for (pos = NULL, pos = list_container_of((head)->next, pos, member), \
        storage = list_container_of(pos->member.next, pos, member);     \
        &pos->member != (head);                                         \
        pos = storage, storage = list_container_of(storage->member.next, storage, member))

#define LIST_FOR_EACH_ENTRY_SAFE_REV(pos, storage, head, member)        \
   for (pos = NULL, pos = list_container_of((head)->prev, pos, member), \
        storage = list_container_of(pos->member.prev, pos, member);             \
        &pos->member != (head);                                         \
        pos = storage, storage = list_container_of(storage->member.prev, storage, member))

#define LIST_FOR_EACH_ENTRY_FROM(pos, start, head, member)              \
   for (pos = NULL, pos = list_container_of((start), pos, member);              \
        &pos->member != (head);                                         \
        pos = list_container_of(pos->member.next, pos, member))

#define LIST_FOR_EACH_ENTRY_FROM_REV(pos, start, head, member)          \
   for (pos = NULL, pos = list_container_of((start), pos, member);              \
        &pos->member != (head);                                         \
        pos = list_container_of(pos->member.prev, pos, member))
#define list_for_each_entry(type, pos, head, member)                    \
   for (type *pos = list_entry((head)->next, type, member),        \
             *__next = list_entry(pos->member.next, type, member); \
        &pos->member != (head);                                         \
        pos = list_entry(pos->member.next, type, member),          \
        list_assert(pos == __next, "use _safe iterator"),               \
        __next = list_entry(__next->member.next, type, member))

#define list_for_each_entry_safe(type, pos, head, member)               \
   for (type *pos = list_entry((head)->next, type, member),        \
             *__next = list_entry(pos->member.next, type, member); \
        &pos->member != (head);                                         \
        pos = __next,                                                   \
        __next = list_entry(__next->member.next, type, member))

#define list_for_each_entry_rev(type, pos, head, member)                \
   for (type *pos = list_entry((head)->prev, type, member),        \
             *__prev = list_entry(pos->member.prev, type, member); \
        &pos->member != (head);                                         \
        pos = list_entry(pos->member.prev, type, member),          \
        list_assert(pos == __prev, "use _safe iterator"),               \
        __prev = list_entry(__prev->member.prev, type, member))

#define list_for_each_entry_safe_rev(type, pos, head, member)           \
   for (type *pos = list_entry((head)->prev, type, member),        \
             *__prev = list_entry(pos->member.prev, type, member); \
        &pos->member != (head);                                         \
        pos = __prev,                                                   \
        __prev = list_entry(__prev->member.prev, type, member))

#define list_for_each_entry_from(type, pos, start, head, member)        \
   for (type *pos = list_entry((start), type, member);             \
        &pos->member != (head);                                         \
        pos = list_entry(pos->member.next, type, member))

#define list_for_each_entry_from_safe(type, pos, start, head, member)   \
   for (type *pos = list_entry((start), type, member),             \
             *__next = list_entry(pos->member.next, type, member); \
        &pos->member != (head);                                         \
        pos = __next,                                                   \
        __next = list_entry(__next->member.next, type, member))

#define list_for_each_entry_from_rev(type, pos, start, head, member)    \
   for (type *pos = list_entry((start), type, member);             \
        &pos->member != (head);                                         \
        pos = list_entry(pos->member.prev, type, member))
#endif
