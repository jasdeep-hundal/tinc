/*
    msgbuf.h -- header for msgbuf.c
    Copyright (C) 2014 Derek Chiang 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __TINC_MSGBUF_H__
#define __TINC_MSGBUF_H__

#include "stddef.h"

#include "net.h"

// A msgbuf is a collection of packets that are meant to be sent at once.  The idea is
// to use sendmmsg to send multiple messages using one system call, therefore reducing the
// amount of context-switching we need to do.
typedef struct msgbuf *msgbuf_t;

// Create a msgbuf
extern msgbuf_t msgbuf_create(void);

// Destroy a msgbuf
extern void msgbuf_destroy(msgbuf_t);

// Add a message to the msgbuf.  The message is copied by the callee.
extern void msgbuf_add(msgbuf_t, int sock, const sockaddr_t *sa, char *data, size_t len); 

// Send all messages in this msgbuf
extern void msgbuf_flush(msgbuf_t);

#endif
