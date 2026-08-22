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

void operator delete(void* mem) {
    void *allocadr;
    ADB(kprintf("[fryingpan] %s(0x%p)\n", __func__, mem));

    Allocator** d = reinterpret_cast<Allocator**>(mem);
    --d;
#if defined(AROS_WORSTALIGN)
    void** tmpmem = reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mem) - sizeof(void *));
    allocadr = tmpmem[-1];
#else
    allocadr = reinterpret_cast<void*>(d);
#endif
    ADB(kprintf("[fryingpan] %s: allocator @ 0x%p, freeing 0x%p\n", __func__, d, allocadr));
    (*d)->free(*d, allocadr);
}

void operator delete[](void* mem) {
    ADB(kprintf("[fryingpan] %s(0x%p)\n", __func__, mem));
    operator delete(mem);
}

void operator delete(void* mem, std::size_t sz) {
    ADB(kprintf("[fryingpan] %s(0x%p, %ubytes)\n", __func__, mem, sz));
    operator delete(mem);
}

void operator delete(void* ptr, std::align_val_t align) {
    ADB(kprintf("[fryingpan] %s(0x%p, %u)\n", __func__, ptr, align));
    if (ptr) {
        void* raw = reinterpret_cast<void**>(ptr)[-1];
        Allocator* a = reinterpret_cast<Allocator**>(raw)[0]; // allocator is stored at raw
        a->free(a, raw);
    }
}

void operator delete[](void* ptr, std::align_val_t align) {
    ADB(kprintf("[fryingpan] %s(0x%p, %u)\n", __func__, ptr, align));
    operator delete(ptr, align);
}
