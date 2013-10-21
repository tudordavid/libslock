/*
 * File: gl_lock.c
 * Author: Tudor David <tudor.david@epfl.ch>
 *
 * Description: 
 *      Global read-write lock implementation
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



#include "gl_lock.h"

void local_lock_write(global_lock* gl) {
    while(1) {
        while (gl->lock_short.global_lock != 0) {}
        unsigned int aux = (unsigned int) gl->lock_short.local_lock;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+0x100) == (aux)) {
            return;
        }
    }
}

void local_unlock_write(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.local_write),1);
}

void local_lock_read(global_lock* gl) {
    while(1) {
        while (gl->lock.global_write != 0) {}
        unsigned int aux = (unsigned int) gl->lock_data & 0x00ffffff;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+1) == (aux)) {
            return;
        }
    }
}

void local_unlock_read(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.local_read),1);
}


void global_acquire_write(global_lock* gl) {
    while(1) {
        while (gl->lock_data != 0) {}
        unsigned short aux = (unsigned short) 0x1000000;
        if (__sync_val_compare_and_swap(&gl->lock_data, 0, aux) == 0) {
            return;
        }
    }
}


void global_unlock_write(global_lock* gl) {
    COMPILER_BARRIER;
#ifdef __tile__
    MEM_BARRIER;
#endif
    gl->lock_data = 0;
}

void global_acquire_read(global_lock* gl) {
    while(1) {
        while ((gl->lock.global_write != 0) || (gl->lock.local_write != 0)) {}
        unsigned int aux = (unsigned int) gl->lock_data & 0x00ff00ff;
        if (__sync_val_compare_and_swap(&gl->lock_data, aux,aux+0x10000) == (aux)) {
            return;
        }
    }
}

void global_unlock_read(global_lock* gl){
    __sync_sub_and_fetch(&(gl->lock.global_read),1);
}


