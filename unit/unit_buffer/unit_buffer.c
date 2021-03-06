/*
 * Copyright (C) 2018 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of Jolla Ltd nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_common.h"

#include "gbinder_driver.h"
#include "gbinder_buffer_p.h"

static TestOpt test_opt;

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    GBinderDriver* driver = gbinder_driver_new(GBINDER_DEFAULT_BINDER);
    GBinderBuffer* buf = gbinder_buffer_new(NULL, NULL, 0);
    GBinderBuffer* buf2;

    gbinder_buffer_free(buf);

    /* No need to reference the driver if there's no data */
    buf = gbinder_buffer_new(driver, NULL, 0);
    g_assert(!gbinder_buffer_driver(buf));
    gbinder_buffer_free(buf);

    buf = gbinder_buffer_new_with_parent(NULL, NULL, 0);
    buf2 = gbinder_buffer_new_with_parent(buf, NULL, 0);
    g_assert(!gbinder_buffer_driver(buf));
    g_assert(!gbinder_buffer_driver(buf2));
    gbinder_buffer_free(buf);
    gbinder_buffer_free(buf2);

    gbinder_buffer_free(NULL);
    g_assert(!gbinder_buffer_driver(NULL));
    gbinder_driver_unref(driver);
}

/*==========================================================================*
 * parent
 *==========================================================================*/

static
void
test_parent(
    void)
{
    static const guint8 data[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    void* ptr = g_memdup(data, sizeof(data));
    GBinderDriver* driver = gbinder_driver_new(GBINDER_DEFAULT_BINDER);
    GBinderBuffer* parent = gbinder_buffer_new(driver, ptr, sizeof(data));
    GBinderBuffer* buf = gbinder_buffer_new_with_parent
        (parent, ptr, sizeof(data));

    g_assert(gbinder_buffer_driver(buf) == driver);
    gbinder_buffer_free(buf);
    gbinder_buffer_free(parent);
    gbinder_driver_unref(driver);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/buffer/"

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_PREFIX "null", test_null);
    g_test_add_func(TEST_PREFIX "parent", test_parent);
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
