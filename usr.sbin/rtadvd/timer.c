/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/time.h>

#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#ifdef __NetBSD__
#include <search.h>
#endif
#include "timer.h"

static struct rtadvd_timer timer_head;

#define	MILLION 1000000

static struct timeval tm_max = {0x7fffffff, 0x7fffffff};

void
rtadvd_timer_init()
{
	memset(&timer_head, 0, sizeof(timer_head));

	timer_head.next = timer_head.prev = &timer_head;
	timer_head.tm = tm_max;
}

struct rtadvd_timer *
rtadvd_add_timer(void (*timeout) __P((void *)),
		void (*update) __P((void *, struct timeval *)),
		 void *timeodata, void *updatedata)
{
	struct rtadvd_timer *newtimer;

	if ((newtimer = malloc(sizeof(*newtimer))) == NULL) {
		syslog(LOG_ERR,
		       "<%s> can't allocate memory", __FUNCTION__);
		exit(1);
	}

	memset(newtimer, 0, sizeof(*newtimer));

	if (timeout == NULL) {
		syslog(LOG_ERR,
		       "<%s> timeout function unspecfied", __FUNCTION__);
		exit(1);
	}
	if (update == NULL) {
		syslog(LOG_ERR,
		       "<%s> update function unspecfied", __FUNCTION__);
		exit(1);
	}
	newtimer->expire = timeout;
	newtimer->update = update;
	newtimer->expire_data = timeodata;
	newtimer->update_data = updatedata;
	newtimer->tm = tm_max;

	/* link into chain */
	insque(newtimer, &timer_head);

	return(newtimer);
}

void
rtadvd_set_timer(struct timeval *tm, struct rtadvd_timer *timer)
{
	struct timeval now;

	/* reset the timer */
	gettimeofday(&now, NULL);

	TIMEVAL_ADD(&now, tm, &timer->tm);

	/* update the next expiration time */
	if (TIMEVAL_LT(timer->tm, timer_head.tm))
		timer_head.tm = timer->tm;

	return;
}

/*
 * Check expiration for each timer. If a timer is expired,
 * call the expire function for the timer and update the timer.
 * Return the next interval for select() call.
 */
struct timeval *
rtadvd_check_timer()
{
	static struct timeval returnval;
	struct timeval now;
	struct rtadvd_timer *tm = timer_head.next;

	gettimeofday(&now, NULL);

	timer_head.tm = tm_max;

	while(tm != &timer_head) {
		if (TIMEVAL_LEQ(tm->tm, now)) {
			(*tm->expire)(tm->expire_data);
			(*tm->update)(tm->update_data, &tm->tm);
			TIMEVAL_ADD(&tm->tm, &now, &tm->tm);
		}

		if (TIMEVAL_LT(tm->tm, timer_head.tm))
			timer_head.tm = tm->tm;

		tm = tm->next;
	}

	if (TIMEVAL_LT(timer_head.tm, now)) {
		/* this may occur when the interval is too small */
		returnval.tv_sec = returnval.tv_usec = 0;
	}
	else
		TIMEVAL_SUB(&timer_head.tm, &now, &returnval);
	return(&returnval);
}

struct timeval *
rtadvd_timer_rest(struct rtadvd_timer *timer)
{
	static struct timeval returnval, now;

	gettimeofday(&now, NULL);
	if (TIMEVAL_LEQ(timer->tm, now)) {
		syslog(LOG_DEBUG,
		       "<%s> a timer must be expired, but not yet",
		       __FUNCTION__);
		returnval.tv_sec = returnval.tv_usec = 0;
	}
	else
		TIMEVAL_SUB(&timer->tm, &now, &returnval);

	return(&returnval);
}

/* result = a + b */
void
TIMEVAL_ADD(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec + b->tv_usec) < MILLION) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec + b->tv_sec;
	}
	else {
		result->tv_usec = l - MILLION;
		result->tv_sec = a->tv_sec + b->tv_sec + 1;
	}
}

/*
 * result = a - b
 * XXX: this function assumes that a >= b.
 */
void
TIMEVAL_SUB(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec - b->tv_usec) >= 0) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec - b->tv_sec;
	}
	else {
		result->tv_usec = MILLION + l;
		result->tv_sec = a->tv_sec - b->tv_sec - 1;
	}
}
