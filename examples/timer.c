/*
 * timer-example.c
 *
 * Copyright 2016 yubo. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "libubox/list.h"
#include "libubox/uloop.h"


struct timer {
	struct uloop_timeout timeout;
	int n;
};

static struct timer main_timer;

static void timer_cb(struct uloop_timeout *timeout)
{
	struct timer *t;
	t = container_of(timeout, struct timer, timeout);
	printf("%d\n", t->n++);
	uloop_timeout_set(&t->timeout, 1000);
}

int main(int argc, char **argv)
{
	main_timer.timeout.cb = timer_cb;

	uloop_init();
	uloop_timeout_set(&main_timer.timeout, 1000);
	uloop_run();
	uloop_done();

	return 0;
}
