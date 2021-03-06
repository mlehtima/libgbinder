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

#include "gbinder_writer_p.h"
#include "gbinder_io.h"
#include "gbinder_log.h"

#include <gutil_intarray.h>
#include <gutil_macros.h>
#include <gutil_strv.h>

#include <stdint.h>

typedef struct gbinder_writer_priv {
    GBinderWriterData* data;
} GBinderWriterPriv;

G_STATIC_ASSERT(sizeof(GBinderWriter) >= sizeof(GBinderWriterPriv));

GBINDER_INLINE_FUNC GBinderWriterPriv* gbinder_writer_cast(GBinderWriter* pub)
    { return (GBinderWriterPriv*)pub; }
GBINDER_INLINE_FUNC GBinderWriterData* gbinder_writer_data(GBinderWriter* pub)
    { return G_LIKELY(pub) ? gbinder_writer_cast(pub)->data : NULL; }

static
void
gbinder_writer_data_record_offset(
    GBinderWriterData* data,
    guint offset)
{
    if (!data->offsets) {
        data->offsets = gutil_int_array_new();
    }
    gutil_int_array_append(data->offsets, offset);
}

static
void
gbinder_writer_data_write_buffer_object(
    GBinderWriterData* data,
    const void* ptr,
    gsize size,
    const GBinderParent* parent)
{
    GByteArray* dest = data->bytes;
    const guint offset = dest->len;
    guint n;

    /* Preallocate enough space */
    g_byte_array_set_size(dest, offset + GBINDER_MAX_BUFFER_OBJECT_SIZE);
    /* Write the object */
    n = data->io->encode_buffer_object(dest->data + offset, ptr, size, parent);
    /* Fix the data size */
    g_byte_array_set_size(dest, offset + n);
    /* Record the offset */
    gbinder_writer_data_record_offset(data, offset);
    /* The driver seems to require each buffer to be 8-byte aligned */
    data->buffers_size += G_ALIGN8(size);
}

void
gbinder_writer_init(
    GBinderWriter* self,
    GBinderWriterData* data)
{
    memset(self, 0, sizeof(*self));
    gbinder_writer_cast(self)->data = data;
}

void
gbinder_writer_append_int32(
    GBinderWriter* self,
    guint32 value)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_int32(data, value);
    }
}

void
gbinder_writer_data_append_int32(
    GBinderWriterData* data,
    guint32 value)
{
    guint32* ptr;

    g_byte_array_set_size(data->bytes, data->bytes->len + sizeof(*ptr));
    ptr = (void*)(data->bytes->data + (data->bytes->len - sizeof(*ptr)));
    *ptr = value;
}

void
gbinder_writer_append_int64(
    GBinderWriter* self,
    guint64 value)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_int64(data, value);
    }
}

void
gbinder_writer_data_append_int64(
    GBinderWriterData* data,
    guint64 value)
{
    guint64* ptr;

    g_byte_array_set_size(data->bytes, data->bytes->len + sizeof(*ptr));
    ptr = (void*)(data->bytes->data + (data->bytes->len - sizeof(*ptr)));
    *ptr = value;
}

void
gbinder_writer_append_string8(
    GBinderWriter* self,
    const char* str)
{
    gbinder_writer_append_string8_len(self, str, str ? strlen(str) : 0);
}

void
gbinder_writer_append_string8_len(
    GBinderWriter* self,
    const char* str,
    gsize len)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_string8_len(data, str, len);
    }
}

void
gbinder_writer_data_append_string8(
    GBinderWriterData* data,
    const char* str)
{
    gbinder_writer_data_append_string8_len(data, str, str ? strlen(str) : 0);
}

void
gbinder_writer_data_append_string8_len(
    GBinderWriterData* data,
    const char* str,
    gsize len)
{
    if (G_LIKELY(str)) {
        const gsize old_size = data->bytes->len;
        gsize padded_len = G_ALIGN4(len + 1);
        guint32* dest;

        /* Preallocate space */
        g_byte_array_set_size(data->bytes, old_size + padded_len);

        /* Zero the last word */
        dest = (guint32*)(data->bytes->data + old_size);
        dest[padded_len/4 - 1] = 0;

        /* Copy the data */
        memcpy(dest, str, len);
    }
}

void
gbinder_writer_append_string16(
    GBinderWriter* self,
    const char* utf8)
{
    gbinder_writer_append_string16_len(self, utf8, utf8 ? strlen(utf8) : 0);
}

void
gbinder_writer_append_string16_len(
    GBinderWriter* self,
    const char* utf8,
    gssize num_bytes)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_string16_len(data, utf8, num_bytes);
    }
}

void
gbinder_writer_data_append_string16(
    GBinderWriterData* data,
    const char* utf8)
{
    gbinder_writer_data_append_string16_len(data, utf8, utf8? strlen(utf8) : 0);
}

void
gbinder_writer_data_append_string16_len(
    GBinderWriterData* data,
    const char* utf8,
    gssize num_bytes)
{
    const gsize old_size = data->bytes->len;

    if (utf8) {
        const char* end = utf8;

        g_utf8_validate(utf8, num_bytes, &end);
        num_bytes = end - utf8;
    } else {
        num_bytes = 0;
    }

    if (num_bytes > 0) {
        glong len = g_utf8_strlen(utf8, num_bytes);
        gsize padded_len = G_ALIGN4((len+1)*2);
        guint32* len_ptr;
        gunichar2* utf16_ptr;

        /* Preallocate space */
        g_byte_array_set_size(data->bytes, old_size + padded_len + 4);
        len_ptr = (guint32*)(data->bytes->data + old_size);
        utf16_ptr = (gunichar2*)(len_ptr + 1);

        /* TODO: this could be optimized for ASCII strings, i.e. if
         * len equals num_bytes */
        if (len > 0) {
            glong utf16_len = 0;
            gunichar2* utf16 = g_utf8_to_utf16(utf8, num_bytes, NULL,
                &utf16_len, NULL);

            if (utf16) {
                len = utf16_len;
                padded_len = G_ALIGN4((len+1)*2);
                memcpy(utf16_ptr, utf16, (len+1)*2);
                g_free(utf16);
            }
        }

        /* Actual length */
        *len_ptr = len;

        /* Zero padding */
        if (padded_len - (len + 1)*2) {
            memset(utf16_ptr + (len + 1), 0, padded_len - (len + 1)*2);
        }

        /* Correct the packet size if necessaary */
        g_byte_array_set_size(data->bytes, old_size + padded_len + 4);
    } else if (utf8) {
        /* Empty string */
        guint16* ptr16;

        g_byte_array_set_size(data->bytes, old_size + 8);
        ptr16 = (guint16*)(data->bytes->data + old_size);
        ptr16[0] = ptr16[1] = ptr16[2] = 0; ptr16[3] = 0xffff;
    } else {
        /* NULL string */
        gbinder_writer_data_append_int32(data, -1);
    }
}

void
gbinder_writer_append_bool(
    GBinderWriter* self,
    gboolean value)
{
    guint8 padded[4];

    /* Boolean values are padded to 4-byte boundary */
    padded[0] = (value != FALSE);
    padded[1] = padded[2] = padded[3] = 0xff;
    gbinder_writer_append_bytes(self, padded, sizeof(padded));
}

void
gbinder_writer_append_bytes(
    GBinderWriter* self,
    const void* ptr,
    gsize size)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        g_byte_array_append(data->bytes, ptr, size);
    }
}

static
guint
gbinder_writer_data_prepare(
    GBinderWriterData* data)
{
    if (!data->offsets) {
        data->offsets = gutil_int_array_new();
    }
    return data->offsets->count;
}

guint
gbinder_writer_append_buffer_object_with_parent(
    GBinderWriter* self,
    const void* buf,
    gsize len,
    const GBinderParent* parent)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        return gbinder_writer_data_append_buffer_object(data, buf, len, parent);
    }
    return 0;
}

guint
gbinder_writer_append_buffer_object(
    GBinderWriter* self,
    const void* buf,
    gsize len)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        return gbinder_writer_data_append_buffer_object(data, buf, len, NULL);
    }
    return 0;
}

guint
gbinder_writer_data_append_buffer_object(
    GBinderWriterData* data,
    const void* ptr,
    gsize size,
    const GBinderParent* parent)
{
    guint index = gbinder_writer_data_prepare(data);

    gbinder_writer_data_write_buffer_object(data, ptr, size, parent);
    return index;
}

void
gbinder_writer_append_hidl_string(
    GBinderWriter* self,
    const char* str)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_hidl_string(data, str);
    }
}

void
gbinder_writer_data_append_hidl_string(
    GBinderWriterData* data,
    const char* str)
{
    GBinderParent str_parent;
    HidlString* hidl_string = g_new0(HidlString, 1);
    const gsize len = str ? strlen(str) : 0;

    /* Prepare parent descriptor for the string data */
    str_parent.index = gbinder_writer_data_prepare(data);
    str_parent.offset = HIDL_STRING_BUFFER_OFFSET;

    /* Fill in the string descriptor and store it */
    hidl_string->data.str = str;
    hidl_string->len = len;
    hidl_string->owns_buffer = TRUE;
    data->cleanup = gbinder_cleanup_add(data->cleanup, g_free, hidl_string);

    /* Write the buffer object pointing to the string descriptor */
    gbinder_writer_data_write_buffer_object(data, hidl_string,
        sizeof(*hidl_string), NULL);

    /* Not sure what's the right way to deal with NULL strings... */
    if (str) {
        /* Write the buffer pointing to the string data including the
         * NULL terminator, referencing string descriptor as a parent. */
        gbinder_writer_data_write_buffer_object(data, str, len+1, &str_parent);
        GVERBOSE_("\"%s\" %u %u %u", str, (guint)len, (guint)str_parent.index,
            (guint)data->buffers_size);
    }
}

void
gbinder_writer_append_hidl_string_vec(
    GBinderWriter* self,
    const char* strv[],
    gssize count)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_hidl_string_vec(data, strv, count);
    }
}

void
gbinder_writer_data_append_hidl_string_vec(
    GBinderWriterData* data,
    const char* strv[],
    gssize count)
{
    GBinderParent vec_parent;
    HidlVec* vec = g_new0(HidlVec, 1);
    HidlString* strings = NULL;
    int i;

    if (count < 0) {
        /* Assume NULL terminated array */
        count = gutil_strv_length((char**)strv);
    }

    /* Prepare parent descriptor for the vector data */
    vec_parent.index = gbinder_writer_data_prepare(data);
    vec_parent.offset = HIDL_VEC_BUFFER_OFFSET;

    /* Fill in the vector descriptor */
    if (count > 0) {
        strings = g_new0(HidlString, count);
        vec->data.ptr = strings;
        data->cleanup = gbinder_cleanup_add(data->cleanup, g_free, strings);
    }
    vec->count = count;
    vec->owns_buffer = TRUE;
    data->cleanup = gbinder_cleanup_add(data->cleanup, g_free, vec);

    /* Fill in string descriptors */
    for (i = 0; i < count; i++) {
        const char* str = strv[i];
        HidlString* hidl_str = strings + i;

        if ((hidl_str->data.str = str) != NULL) {
            hidl_str->len = strlen(str);
            hidl_str->owns_buffer = TRUE;
        }
    }

    /* Write the vector object */
    gbinder_writer_data_write_buffer_object(data, vec, sizeof(*vec), NULL);
    if (strings) {
        GBinderParent str_parent;

        /* Prepare parent descriptor for the string data */
        str_parent.index = data->offsets->count;
        str_parent.offset = HIDL_STRING_BUFFER_OFFSET;

        /* Write the vector data (it's parent for the string data) */
        gbinder_writer_data_write_buffer_object(data, strings,
            sizeof(*strings) * count, &vec_parent);

        /* Write the string data */
        for (i = 0; i < count; i++) {
            HidlString* hidl_str = strings + i;

            if (hidl_str->data.str) {
                gbinder_writer_data_write_buffer_object(data,
                    hidl_str->data.str, hidl_str->len + 1, &str_parent);
                GVERBOSE_("%d. \"%s\" %u %u %u", i + 1, hidl_str->data.str,
                    (guint)hidl_str->len, (guint)str_parent.index,
                    (guint)data->buffers_size);
            }
            str_parent.offset += sizeof(HidlString);
        }
    }
}

void
gbinder_writer_append_local_object(
    GBinderWriter* self,
    GBinderLocalObject* obj)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_local_object(data, obj);
    }
}

void
gbinder_writer_data_append_local_object(
    GBinderWriterData* data,
    GBinderLocalObject* obj)
{
    GByteArray* dest = data->bytes;
    const guint offset = dest->len;
    guint n;

    /* Preallocate enough space */
    g_byte_array_set_size(dest, offset + GBINDER_MAX_BINDER_OBJECT_SIZE);
    /* Write the object */
    n = data->io->encode_local_object(dest->data + offset, obj);
    /* Fix the data size */
    g_byte_array_set_size(dest, offset + n);
    /* Record the offset */
    gbinder_writer_data_record_offset(data, offset);
}

void
gbinder_writer_append_remote_object(
    GBinderWriter* self,
    GBinderRemoteObject* obj)
{
    GBinderWriterData* data = gbinder_writer_data(self);

    if (G_LIKELY(data)) {
        gbinder_writer_data_append_remote_object(data, obj);
    }
}

void
gbinder_writer_data_append_remote_object(
    GBinderWriterData* data,
    GBinderRemoteObject* obj)
{
    GByteArray* dest = data->bytes;
    const guint offset = dest->len;
    guint n;

    /* Preallocate enough space */
    g_byte_array_set_size(dest, offset + GBINDER_MAX_BINDER_OBJECT_SIZE);
    /* Write the object */
    n = data->io->encode_remote_object(dest->data + offset, obj);
    /* Fix the data size */
    g_byte_array_set_size(dest, offset + n);
    /* Record the offset */
    gbinder_writer_data_record_offset(data, offset);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
