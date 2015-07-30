/******************************************************************************
 * test.c
 * 
 * Test code for all the various frontends; split from kernel.c
 * 
 * Copyright (c) 2002-2003, K A Fraser & R Neugebauer
 * Copyright (c) 2005, Grzegorz Milos, Intel Research Cambridge
 * Copyright (c) 2006, Robert Kaiser, FH Wiesbaden
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/os.h>
#include <mini-os/hypervisor.h>
#include <mini-os/mm.h>
#include <mini-os/events.h>
#include <mini-os/time.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/sched.h>
#include <mini-os/xenbus.h>
#include <mini-os/gnttab.h>
#include <mini-os/netfront.h>
#include <mini-os/xmalloc.h>
#include <fcntl.h>
#include <xen/features.h>
#include <xen/version.h>

#ifdef CONFIG_XENBUS
static unsigned int do_shutdown = 0;
static unsigned int shutdown_reason;
static DECLARE_WAIT_QUEUE_HEAD(shutdown_queue);
#endif

#ifdef CONFIG_XENBUS
void test_xenbus(void);

static void xenbus_tester(void *p)
{
    test_xenbus();
}
#endif

/* Should be random enough for our uses */
int rand(void)
{
    static unsigned int previous;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    previous += tv.tv_sec + tv.tv_usec;
    previous *= RAND_MIX;
    return previous;
}

static void periodic_thread(void *p)
{
    struct timeval tv;
    printk("Periodic thread started.\n");
    for(;;)
    {
        gettimeofday(&tv, NULL);
        printk("T(s=%ld us=%ld)\n", tv.tv_sec, tv.tv_usec);
        msleep(1000);
    }
}

void shutdown_frontends(void)
{
}

#ifdef CONFIG_XENBUS
void app_shutdown(unsigned reason)
{
    shutdown_reason = reason;
    wmb();
    do_shutdown = 1;
    wmb();
    wake_up(&shutdown_queue);
}

static void shutdown_thread(void *p)
{
    DEFINE_WAIT(w);

    while (1) {
        add_waiter(w, shutdown_queue);
        rmb();
        if (do_shutdown) {
            rmb();
            break;
        }
        schedule();
        remove_waiter(w, shutdown_queue);
    }

    shutdown_frontends();

    HYPERVISOR_shutdown(shutdown_reason);
}
#endif

int app_main(start_info_t *si)
{
    printk("Test main: start_info=%p\n", si);
#ifdef CONFIG_XENBUS
    create_thread("xenbus_tester", xenbus_tester, si);
#endif
    create_thread("periodic_thread", periodic_thread, si);
#ifdef CONFIG_XENBUS
    create_thread("shutdown", shutdown_thread, si);
#endif
    return 0;
}
