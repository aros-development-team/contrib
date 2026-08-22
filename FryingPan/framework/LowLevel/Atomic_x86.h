/*
 * Amiga Generic Set - set of libraries and includes to ease sw development for all Amiga platforms
 * Copyright (C) 2001-2011 Tomasz Wiszkowski Tomasz.Wiszkowski at gmail.com.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ATOMIC_X86_H
#define ATOMIC_X86_H

#ifdef __AROS__
#include <aros/atomic.h>
#endif


/* The stack is guarded by the 'available' byte, used as a simple spin lock.
   This used to be hand-written 32-bit x86 assembly, which could not work on
   x86_64 (it addressed 'available' at offset 4, valid only while head is four
   bytes) and is not x86 at all on the other targets that reach this header.
   The compiler's atomic builtins express the same thing portably.  */

static inline void _atomic_stack_push(struct _atomic_stack* list, struct _atomic_item* item)
{
   while (__atomic_test_and_set(&list->available, __ATOMIC_ACQUIRE))
      ;

   item->next = list->head;
   list->head = item;

   __atomic_clear(&list->available, __ATOMIC_RELEASE);
}

static inline struct _atomic_item* _atomic_stack_pop(struct _atomic_stack* list)
{
   struct _atomic_item *item;

   while (__atomic_test_and_set(&list->available, __ATOMIC_ACQUIRE))
      ;

   item = list->head;
   if (0 != item)
      list->head = item->next;

   __atomic_clear(&list->available, __ATOMIC_RELEASE);

   return item;
}

static inline void _atomic_cnt_inc(struct _atomic_cnt* cnt)
{
#ifdef __AROS__
   AROS_ATOMIC_INC(cnt->counter);
#else
   __atomic_add_fetch(&cnt->counter, 1, __ATOMIC_SEQ_CST);
#endif
}

static inline void _atomic_cnt_dec(struct _atomic_cnt* cnt)
{
#ifdef __AROS__
   AROS_ATOMIC_DEC(cnt->counter);
#else
   __atomic_sub_fetch(&cnt->counter, 1, __ATOMIC_SEQ_CST);
#endif
}

/* status: 0 = unlocked, ~0 = held for writing, otherwise the reader count.
   The bodies here were commented-out m68k assembly, so every one of these
   returned an uninitialised value.  Implemented with atomic builtins.  */

#define _ATOMIC_SLOCK_WRITER   ((uint32)~0U)

static inline int32 _atomic_slock_trywrite(struct _atomic_slock* lock)
{
   uint32 expected = 0;

   /* only a completely unlocked lock can be taken for writing */
   if (__atomic_compare_exchange_n(&lock->status, &expected,
                                   _ATOMIC_SLOCK_WRITER, 0,
                                   __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
      return -1;

   return 0;
}

static inline void _atomic_slock_unlockwrite(struct _atomic_slock* lock)
{
   uint32 expected = _ATOMIC_SLOCK_WRITER;

   __atomic_compare_exchange_n(&lock->status, &expected, 0, 0,
                               __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}

static inline int32 _atomic_slock_tryread(struct _atomic_slock* lock)
{
   uint32 status = __atomic_load_n(&lock->status, __ATOMIC_RELAXED);

   do
   {
      if (_ATOMIC_SLOCK_WRITER == status)
         return -1;                     /* held for writing */
   }
   while (!__atomic_compare_exchange_n(&lock->status, &status, status + 1, 1,
                                       __ATOMIC_ACQUIRE, __ATOMIC_RELAXED));

   return (int32)status;                /* readers before this one */
}

static inline int32 _atomic_slock_unlockread(struct _atomic_slock* lock)
{
   return (int32)__atomic_sub_fetch(&lock->status, 1, __ATOMIC_RELEASE);
}

#endif

