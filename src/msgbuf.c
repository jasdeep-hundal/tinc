/*
    msg_buf.c -- Implements a message buffer that supports sending multiple
                 messages in one system call
    Copyright (C) 1998-2005 Ivo Timmermans,
                  2000-2013 Guus Sliepen <guus@tinc-vpn.org>
                  2010      Timothy Redaelli <timothy@redaelli.eu>
                  2010      Brandon Black <blblack@gmail.com>

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

#include "system.h"
#include "xalloc.h"
#include "msgbuf.h"
#include "net.h"
#include "logger.h"
#include "utils.h"

#define MSGBUF_SZ 100

// A msg state stores the packets that are going toward a particular node
typedef struct msg_state {
    int sock;  // the connection
    int num_msg;  // the number of packets
    struct mmsghdr msgbuf[MSGBUF_SZ];  // the buffer
} *msg_state_t;

// A msgbuf is a collection of packets that are meant to be sent at once.  The idea is
// to use sendmmsg to send multiple messages using one system call, therefore reducing the
// amount of context-switching we need to do.
struct msgbuf {
    size_t num_msg_states;
    struct msg_state msg_states[MSGBUF_SZ];
    // The total number of packets in all the buffers
    size_t total_msg;
};

// Create a msgbuf
msgbuf_t
msgbuf_create(void)
{
    msgbuf_t self = xzalloc(sizeof(struct msgbuf));
    self->num_msg_states = 0;
    self->total_msg = 0;
    return self;
}

// Destroy a msgbuf
void
msgbuf_destroy(msgbuf_t self)
{
    free(self);
}

// Add a message to the msgbuf.  The message is copied by the callee.
void
msgbuf_add(msgbuf_t self, int sock, const sockaddr_t *sa, char *data, size_t len)
{
    struct msghdr *hdr = NULL;
    for (int i = 0; i < self->num_msg_states; i++) {
        if (self->msg_states[i].sock == sock) {
            hdr = &self->msg_states[i].msgbuf[self->msg_states[i].num_msg++].msg_hdr;
        }
    }

    if (!hdr) {
        msg_state_t ms = &self->msg_states[self->num_msg_states++];
        ms->num_msg = 1;
        ms->sock = sock;
        hdr = &ms->msgbuf[0].msg_hdr;
    }

    if (sa) {
        hdr->msg_name = (void *) &sa->sa;
        hdr->msg_namelen = SALEN(sa->sa);
    } else {
        hdr->msg_name = NULL;
        hdr->msg_namelen = 0;
    }

    hdr->msg_iovlen = 1;
    hdr->msg_iov = malloc(sizeof(struct iovec));
    hdr->msg_iov->iov_base = strndup((char *) data, len);
    hdr->msg_iov->iov_len = len;
    hdr->msg_control = NULL;
    hdr->msg_controllen = 0;
    hdr->msg_flags = 0;

	logger(DEBUG_ALWAYS, LOG_DEBUG, "msg_iovlen %lu; iov_len %lu",
           hdr->msg_iovlen, hdr->msg_iov->iov_len);

    self->total_msg++;
    if (self->total_msg == MSGBUF_SZ) {
        msgbuf_flush(self);
    }
}

// Send all messages in this msgbuf
void
msgbuf_flush(msgbuf_t self) {
	logger(DEBUG_ALWAYS, LOG_DEBUG, "flushing msgbuf!");
    for (int i = 0; i < self->num_msg_states; i++) {
        msg_state_t ms = &self->msg_states[i];
        if (sendmmsg(listen_socket[ms->sock].udp.fd, ms->msgbuf, ms->num_msg, 0) < 0 &&
            !sockwouldblock(sockerrno)) {
            logger(DEBUG_ALWAYS, LOG_WARNING,
                   "Error with sendmmsg: %s",
                   sockstrerror(sockerrno));
        }
        // Free packets 
        for (int i = 0; i < ms->num_msg; i++) {
            free(ms->msgbuf[i].msg_hdr.msg_iov->iov_base);
            free(ms->msgbuf[i].msg_hdr.msg_iov);
        }
        // Reset the counter
        self->total_msg = 0;
    }

    self->num_msg_states = 0;
    self->total_msg = 0;
}
