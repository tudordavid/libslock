/*
 * File: gl_lock.h
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Implementation of a global read-write lock;
 *      Not used in any of the tests
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013 Tudor David
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */




#ifndef _GLLOCK_H_
#define _GLLOCK_H_

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "utils.h"
#include "atomic_ops.h"

typedef struct glock{
    volatile unsigned char local_read;
    volatile unsigned char local_write;
    volatile unsigned char global_read;
    volatile unsigned char global_write;
} glock;

typedef struct glock_2{
    volatile unsigned short local_lock;
    volatile unsigned short global_lock;
} glock_2;

typedef struct global_lock {
    union {
        volatile unsigned int lock_data;
        glock_2 lock_short;
        glock lock;
        volatile unsigned char padding[CACHE_LINE_SIZE];
    };
} global_lock;


void local_lock_write(global_lock* gl);

void local_unlock_write(global_lock* gl);

void local_lock_read(global_lock* gl);

void local_unlock_read(global_lock* gl);

void global_acquire_write(global_lock* gl);

void global_acquire_read(global_lock* gl);

void global_unlock_write(global_lock* gl);

void global_unlock_read(global_lock* gl);

#endif
