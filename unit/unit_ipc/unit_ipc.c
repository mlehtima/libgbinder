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

#include "test_binder.h"

#include "gbinder_ipc.h"
#include "gbinder_driver.h"
#include "gbinder_local_object.h"
#include "gbinder_local_reply_p.h"
#include "gbinder_local_request_p.h"
#include "gbinder_object_registry.h"
#include "gbinder_output_data.h"
#include "gbinder_remote_reply.h"
#include "gbinder_remote_request.h"
#include "gbinder_rpc_protocol.h"
#include "gbinder_writer.h"

#include <gutil_log.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

static TestOpt test_opt;

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    GBinderIpc* null = NULL;
    int status = INT_MAX;

    g_assert(!gbinder_ipc_ref(null));
    gbinder_ipc_unref(null);
    g_assert(!gbinder_ipc_transact_sync_reply(null, 0, 0, NULL, NULL));
    g_assert(!gbinder_ipc_transact_sync_reply(null, 0, 0, NULL, &status));
    g_assert(status == (-EINVAL));
    g_assert(gbinder_ipc_transact_sync_oneway(null, 0, 0, NULL) == (-EINVAL));
    g_assert(!gbinder_ipc_transact(null, 0, 0, 0, NULL, NULL, NULL, NULL));
    g_assert(!gbinder_ipc_transact_custom(null, NULL, NULL, NULL, NULL));
    g_assert(!gbinder_ipc_object_registry(null));
    gbinder_ipc_looper_check(null);
    gbinder_ipc_cancel(null, 0);

    g_assert(!gbinder_object_registry_ref(NULL));
    gbinder_object_registry_unref(NULL);
    g_assert(!gbinder_object_registry_get_local(NULL, NULL));
    g_assert(!gbinder_object_registry_get_remote(NULL, 0));
}

/*==========================================================================*
 * basic
 *==========================================================================*/

static
void
test_basic(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    GBinderIpc* ipc2 = gbinder_ipc_new(GBINDER_DEFAULT_HWBINDER);

    g_assert(ipc);
    g_assert(ipc2);
    g_assert(ipc != ipc2);
    gbinder_ipc_cancel(ipc2, 0); /* not a valid transaction */
    gbinder_ipc_unref(ipc2);

    /* Second gbinder_ipc_new returns the same (default) object */
    g_assert(gbinder_ipc_new(NULL) == ipc);
    g_assert(gbinder_ipc_new("") == ipc);
    gbinder_ipc_unref(ipc);
    gbinder_ipc_unref(ipc);
    gbinder_ipc_unref(ipc);

    /* Invalid path */
    g_assert(!gbinder_ipc_new("invalid path"));
}

/*==========================================================================*
 * sync_oneway
 *==========================================================================*/

static
void
test_sync_oneway(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);

    g_assert(test_binder_br_transaction_complete(fd));
    g_assert(gbinder_ipc_transact_sync_oneway(ipc, 0, 1, req) ==
        GBINDER_STATUS_OK);
    gbinder_local_request_unref(req);
    gbinder_ipc_unref(ipc);
}

/*==========================================================================*
 * sync_reply_ok
 *==========================================================================*/

static
void
test_sync_reply_ok_status(
    int* status)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GBinderLocalReply* reply = gbinder_local_reply_new(io);
    GBinderRemoteReply* tx_reply;
    GBinderOutputData* data;
    const guint32 handle = 0;
    const guint32 code = 1;
    const char* result_in = "foo";
    char* result_out;

    g_assert(gbinder_local_reply_append_string16(reply, result_in));
    data = gbinder_local_reply_data(reply);
    g_assert(data);

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_transaction_complete(fd));
    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_reply(fd, handle, code, data->bytes));

    tx_reply = gbinder_ipc_transact_sync_reply(ipc, handle, code, req, status);
    g_assert(tx_reply);

    result_out = gbinder_remote_reply_read_string16(tx_reply);
    g_assert(!g_strcmp0(result_out, result_in));
    g_free(result_out);

    gbinder_remote_reply_unref(tx_reply);
    gbinder_local_request_unref(req);
    gbinder_local_reply_unref(reply);
    gbinder_ipc_unref(ipc);
}

static
void
test_sync_reply_ok(
    void)
{
    int status = -1;

    test_sync_reply_ok_status(NULL);
    test_sync_reply_ok_status(&status);
    g_assert(status == GBINDER_STATUS_OK);
}

/*==========================================================================*
 * sync_reply_error
 *==========================================================================*/

static
void
test_sync_reply_error(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    const guint32 handle = 0;
    const guint32 code = 1;
    const gint expected_status = GBINDER_STATUS_FAILED;
    int status = INT_MAX;

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_transaction_complete(fd));
    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_reply_status(fd, expected_status));

    g_assert(!gbinder_ipc_transact_sync_reply(ipc, handle, code, req, &status));
    g_assert(status == expected_status);

    gbinder_local_request_unref(req);
    gbinder_ipc_unref(ipc);
}

/*==========================================================================*
 * transact_ok
 *==========================================================================*/

#define TEST_REQ_PARAM_STR "foo"

static
void
test_transact_ok_destroy(
    void* user_data)
{
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_ok_done(
    GBinderIpc* ipc,
    GBinderRemoteReply* reply,
    int status,
    void* user_data)
{
    char* result;

    GVERBOSE_("");
    result = gbinder_remote_reply_read_string16(reply);
    g_assert(!g_strcmp0(result, TEST_REQ_PARAM_STR));
    g_free(result);
    g_assert(status == GBINDER_STATUS_OK);
}

static
void
test_transact_ok(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GBinderLocalReply* reply = gbinder_local_reply_new(io);
    GBinderOutputData* data;
    const guint32 handle = 0;
    const guint32 code = 1;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id;

    g_assert(gbinder_local_reply_append_string16(reply, TEST_REQ_PARAM_STR));
    data = gbinder_local_reply_data(reply);
    g_assert(data);

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_transaction_complete(fd));
    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_reply(fd, handle, code, data->bytes));

    id = gbinder_ipc_transact(ipc, handle, code, 0, req,
        test_transact_ok_done, test_transact_ok_destroy, loop);
    g_assert(id);

    test_run(&test_opt, loop);

    /* Transaction id is not valid anymore: */
    gbinder_ipc_cancel(ipc, id);
    gbinder_local_request_unref(req);
    gbinder_local_reply_unref(reply);
    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_dead
 *==========================================================================*/

static
void
test_transact_dead_done(
    GBinderIpc* ipc,
    GBinderRemoteReply* reply,
    int status,
    void* user_data)
{
    GVERBOSE_("%d", status);
    g_assert(!reply);
    g_assert(status == GBINDER_STATUS_DEAD_OBJECT);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_dead(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id;

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_dead_reply(fd));

    id = gbinder_ipc_transact(ipc, 1, 2, 0, req, test_transact_dead_done,
        NULL, loop);
    g_assert(id);

    test_run(&test_opt, loop);

    /* Transaction id is not valid anymore: */
    gbinder_ipc_cancel(ipc, id);
    gbinder_local_request_unref(req);
    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_failed
 *==========================================================================*/

static
void
test_transact_failed_done(
    GBinderIpc* ipc,
    GBinderRemoteReply* reply,
    int status,
    void* user_data)
{
    GVERBOSE_("%d", status);
    g_assert(!reply);
    g_assert(status == GBINDER_STATUS_FAILED);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_failed(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id;

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_failed_reply(fd));

    id = gbinder_ipc_transact(ipc, 1, 2, 0, req, test_transact_failed_done,
        NULL, loop);
    g_assert(id);

    test_run(&test_opt, loop);

    /* Transaction id is not valid anymore: */
    gbinder_ipc_cancel(ipc, id);
    gbinder_local_request_unref(req);
    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_status
 *==========================================================================*/

#define EXPECTED_STATUS (0x42424242)

static
void
test_transact_status_done(
    GBinderIpc* ipc,
    GBinderRemoteReply* reply,
    int status,
    void* user_data)
{
    GVERBOSE_("%d", status);
    g_assert(!reply);
    g_assert(status == EXPECTED_STATUS);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_status(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id;

    g_assert(test_binder_br_noop(fd));
    g_assert(test_binder_br_reply_status(fd, EXPECTED_STATUS));

    id = gbinder_ipc_transact(ipc, 1, 2, 0, req, test_transact_status_done,
        NULL, loop);
    g_assert(id);

    test_run(&test_opt, loop);

    /* Transaction id is not valid anymore: */
    gbinder_ipc_cancel(ipc, id);
    gbinder_local_request_unref(req);
    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_custom
 *==========================================================================*/

static
void
test_transact_custom_done(
    const GBinderIpcTx* tx)
{
    GVERBOSE_("");
    test_quit_later((GMainLoop*)tx->user_data);
}

static
void
test_transact_custom(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id = gbinder_ipc_transact_custom(ipc, NULL,
        test_transact_custom_done, NULL, loop);

    g_assert(id);
    test_run(&test_opt, loop);

    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_custom2
 *==========================================================================*/

static
void
test_transact_custom_destroy(
    void* user_data)
{
    GVERBOSE_("");
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_custom2(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id = gbinder_ipc_transact_custom(ipc, NULL, NULL,
        test_transact_custom_destroy, loop);

    g_assert(id);
    test_run(&test_opt, loop);

    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_cancel
 *==========================================================================*/

static
void
test_transact_cancel_destroy(
    void* user_data)
{
    GVERBOSE_("");
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_transact_cancel_exec(
    const GBinderIpcTx* tx)
{
    GVERBOSE_("");
}

static
void
test_transact_cancel_done(
    const GBinderIpcTx* tx)
{
    GVERBOSE_("");
    g_assert(tx->cancelled);
}

static
void
test_transact_cancel(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    gulong id = gbinder_ipc_transact_custom(ipc, test_transact_cancel_exec,
        test_transact_cancel_done, test_transact_cancel_destroy, loop);

    g_assert(id);
    gbinder_ipc_cancel(ipc, id);
    test_run(&test_opt, loop);

    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_cancel2
 *==========================================================================*/

static
gboolean
test_transact_cancel2_cancel(
    gpointer data)
{
    const GBinderIpcTx* tx = data;

    GVERBOSE_("");
    gbinder_ipc_cancel(tx->ipc, tx->id);
    return G_SOURCE_REMOVE;
}

static
void
test_transact_cancel2_exec(
    const GBinderIpcTx* tx)
{
    GVERBOSE_("");
    g_assert(!tx->cancelled);
    g_main_context_invoke(NULL, test_transact_cancel2_cancel, (void*)tx);
}

static
void
test_transact_cancel2(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    /* Reusing test_transact_cancel_done and test_transact_cancel_destroy */
    gulong id = gbinder_ipc_transact_custom(ipc, test_transact_cancel2_exec,
        test_transact_cancel_done, test_transact_cancel_destroy, loop);

    g_assert(id);
    test_run(&test_opt, loop);

    gbinder_ipc_unref(ipc);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_incoming
 *==========================================================================*/

static
GBinderLocalReply*
test_transact_incoming_proc(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    GVERBOSE_("\"%s\" %u", gbinder_remote_request_interface(req), code);
    g_assert(!flags);
    g_assert(gbinder_remote_request_sender_pid(req) == getpid());
    g_assert(gbinder_remote_request_sender_euid(req) == geteuid());
    g_assert(!g_strcmp0(gbinder_remote_request_interface(req), "test"));
    g_assert(!g_strcmp0(gbinder_remote_request_read_string8(req), "message"));
    g_assert(code == 1);
    test_quit_later((GMainLoop*)user_data);

    *status = GBINDER_STATUS_OK;
    return gbinder_local_object_new_reply(obj);
}

static
gboolean
test_transact_unref_ipc(
    gpointer ipc)
{
    gbinder_ipc_unref(ipc);
    return G_SOURCE_REMOVE;
}

static
void
test_transact_done(
    gpointer loop,
    GObject* ipc)
{
    test_quit_later((GMainLoop*)loop);
}

static
void
test_transact_incoming(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    const char* dev = gbinder_driver_dev(ipc->driver);
    const GBinderRpcProtocol* prot = gbinder_rpc_protocol_for_device(dev);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    GBinderLocalObject* obj = gbinder_ipc_new_local_object
        (ipc, "test", test_transact_incoming_proc, loop);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GBinderOutputData* data;
    GBinderWriter writer;

    gbinder_local_request_init_writer(req, &writer);
    prot->write_rpc_header(&writer, "test");
    gbinder_writer_append_string8(&writer, "message");
    data = gbinder_local_request_data(req);

    test_binder_br_transaction(fd, obj, 1, data->bytes);
    test_run(&test_opt, loop);

    /* Now we need to wait until GBinderIpc is destroyed */
    GDEBUG("waiting for GBinderIpc to get destroyed");
    g_object_weak_ref(G_OBJECT(ipc), test_transact_done, loop);
    gbinder_local_object_unref(obj);
    gbinder_local_request_unref(req);
    g_idle_add(test_transact_unref_ipc, ipc);
    test_run(&test_opt, loop);

    g_main_loop_unref(loop);
}

/*==========================================================================*
 * transact_incoming_status
 *==========================================================================*/

static
GBinderLocalReply*
test_transact_status_reply_proc(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    GVERBOSE_("\"%s\" %u", gbinder_remote_request_interface(req), code);
    g_assert(!flags);
    g_assert(!g_strcmp0(gbinder_remote_request_interface(req), "test"));
    g_assert(!g_strcmp0(gbinder_remote_request_read_string8(req), "message"));
    g_assert(code == 1);
    test_quit_later((GMainLoop*)user_data);

    *status = EXPECTED_STATUS;
    return NULL;
}

static
void
test_transact_status_reply(
    void)
{
    GBinderIpc* ipc = gbinder_ipc_new(GBINDER_DEFAULT_BINDER);
    const GBinderIo* io = gbinder_driver_io(ipc->driver);
    const int fd = gbinder_driver_fd(ipc->driver);
    const char* dev = gbinder_driver_dev(ipc->driver);
    const GBinderRpcProtocol* prot = gbinder_rpc_protocol_for_device(dev);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    GBinderLocalObject* obj = gbinder_ipc_new_local_object
        (ipc, "test", test_transact_status_reply_proc, loop);
    GBinderLocalRequest* req = gbinder_local_request_new(io, NULL);
    GBinderOutputData* data;
    GBinderWriter writer;

    gbinder_local_request_init_writer(req, &writer);
    prot->write_rpc_header(&writer, "test");
    gbinder_writer_append_string8(&writer, "message");
    data = gbinder_local_request_data(req);

    test_binder_br_transaction(fd, obj, 1, data->bytes);
    test_run(&test_opt, loop);

    /* Now we need to wait until GBinderIpc is destroyed */
    GDEBUG("waiting for GBinderIpc to get destroyed");
    g_object_weak_ref(G_OBJECT(ipc), test_transact_done, loop);
    gbinder_local_object_unref(obj);
    gbinder_local_request_unref(req);
    g_idle_add(test_transact_unref_ipc, ipc);
    test_run(&test_opt, loop);

    g_main_loop_unref(loop);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/ipc/"

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_PREFIX "null", test_null);
    g_test_add_func(TEST_PREFIX "basic", test_basic);
    g_test_add_func(TEST_PREFIX "sync_oneway", test_sync_oneway);
    g_test_add_func(TEST_PREFIX "sync_reply_ok", test_sync_reply_ok);
    g_test_add_func(TEST_PREFIX "sync_reply_error", test_sync_reply_error);
    g_test_add_func(TEST_PREFIX "transact_ok", test_transact_ok);
    g_test_add_func(TEST_PREFIX "transact_dead", test_transact_dead);
    g_test_add_func(TEST_PREFIX "transact_failed", test_transact_failed);
    g_test_add_func(TEST_PREFIX "transact_status", test_transact_status);
    g_test_add_func(TEST_PREFIX "transact_custom", test_transact_custom);
    g_test_add_func(TEST_PREFIX "transact_custom2", test_transact_custom2);
    g_test_add_func(TEST_PREFIX "transact_cancel", test_transact_cancel);
    g_test_add_func(TEST_PREFIX "transact_cancel2", test_transact_cancel2);
    g_test_add_func(TEST_PREFIX "transact_incoming", test_transact_incoming);
    g_test_add_func(TEST_PREFIX "transact_status_reply",
        test_transact_status_reply);
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
