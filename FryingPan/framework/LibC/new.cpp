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

#include "LibC.h"

static inline void* alloc_proxy(const struct Allocator*a, size_t size) {
    ADB(kprintf("[fryingpan] %s(0x%p, %ubytes)\n", __func__, a, size));
    return malloc(size);
}

static inline void free_proxy(const struct Allocator*a, void* mem) {
    ADB(kprintf("[fryingpan] %s(0x%p, 0x%p)\n", __func__, a, mem));
    free(mem);
}

const struct Allocator DEFAULT_ALLOCATOR = { &alloc_proxy, &free_proxy };
static const struct Allocator* current_allocator = &DEFAULT_ALLOCATOR;

const struct Allocator* set_default_allocator(const struct Allocator *a) {
    ADB(kprintf("[fryingpan] %s(0x%p)\n", __func__, a));
    const struct Allocator *b = current_allocator;
    current_allocator = a;

    return b;
}

static inline void** alloc_proxy_aligned(const struct Allocator* a,
                                        size_t size, size_t priv)
{
    ADB(kprintf("[fryingpan] %s(0x%p, %u, %u)\n", __func__, a, size, priv));
    size_t allocsize = size + priv;

#if defined(AROS_WORSTALIGN)
    allocsize = (allocsize + sizeof(void*) + (AROS_WORSTALIGN - 1)) & ~(AROS_WORSTALIGN - 1);
#endif

    ADB(kprintf("[fryingpan] %s: allocating %zu bytes\n", __func__, allocsize));
    void* membase = a->alloc(a, allocsize);

#if defined(AROS_WORSTALIGN)
    uintptr_t raw = reinterpret_cast<uintptr_t>(membase) + sizeof(void*) + priv;
    uintptr_t aligned = (raw + (AROS_WORSTALIGN - 1)) & ~(AROS_WORSTALIGN - 1);
    void** mem = reinterpret_cast<void**>(aligned - priv);
    mem[-1] = membase;
#else
    void** mem = reinterpret_cast<void**>(membase);
#endif

    ADB(kprintf("[fryingpan] %s: allocated @ 0x%p (returning @ 0x%p)\n", __func__, membase, mem));
    return mem;
}

void* operator new(size_t lSize) {
    ADB(kprintf("[fryingpan] %s(%ubytes)\n", __func__, lSize));
    return operator new(lSize, current_allocator);
}

void* operator new[](size_t lSize) {
    ADB(kprintf("[fryingpan] %s(%ubytes)\n", __func__, lSize));
    return operator new[](lSize, current_allocator);
}

void* operator new(size_t lSize, const struct Allocator *a) {
    ADB(kprintf("[fryingpan] %s(%ubytes, 0x%p)\n", __func__, lSize, a));
    void **mem = alloc_proxy_aligned(a, lSize, sizeof(void *));
    *mem++ = const_cast<Allocator*>(a);
    ADB(kprintf("[fryingpan] %s: allocated @ 0x%p\n", __func__, mem));
    return mem;
}

void* operator new[](size_t lSize, const struct Allocator *a) {
    ADB(kprintf("[fryingpan] %s(%ubytes, 0x%p)\n", __func__, lSize, a));
    void **mem = alloc_proxy_aligned(a, lSize, sizeof(void *));
    *mem++ = const_cast<Allocator*>(a);
    ADB(kprintf("[fryingpan] %s: allocated @ 0x%p\n", __func__, mem));
    return mem;
}

void* operator new(size_t lSize, const Allocator* a, std::align_val_t align) noexcept {

    ADB(kprintf("[fryingpan] %s(%ubytes, 0x%p, %u)\n", __func__, lSize, a, align));

    std::size_t alignment = static_cast<std::size_t>(align);
    std::size_t extra = alignment - 1 + sizeof(void*);

    ADB(kprintf("[fryingpan] %s: extra = %ubytes\n", __func__, extra));

    void* raw = a->alloc(a, lSize + extra);
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    void** mem = reinterpret_cast<void**>(aligned);
    mem[-1] = raw;  // store original pointer for deallocation
 
    ADB(kprintf("[fryingpan] %s: returning 0x%p\n", __func__, mem));

    return mem;
}

void* operator new(size_t lSize, std::align_val_t align) {
    ADB(kprintf("[fryingpan] %s(%ubytes, %u)\n", __func__, lSize, align));

    return operator new(lSize, current_allocator, align);
}

void* operator new[](size_t lSize, const Allocator* a, std::align_val_t align) {
    ADB(kprintf("[fryingpan] %s(%ubytes, %u)\n", __func__, lSize, align));

    return operator new(lSize, a, align);
}

void* operator new[](size_t lSize, std::align_val_t align) {

    ADB(kprintf("[fryingpan] %s(%ubytes, %u)\n", __func__, lSize, align));

    return operator new[](lSize, current_allocator, align);
}
