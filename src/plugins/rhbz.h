/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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

#ifndef RHBZ_H
#define RHBZ_H

/* include/stdint.h: typedef int int32_t;
 * include/xmlrpc-c/base.h: typedef int32_t xmlrpc_int32;
 */

#include "abrt_xmlrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RHBZ_MANDATORY_MEMB = (1 << 0),
    RHBZ_READ_STR       = (1 << 1),
    RHBZ_READ_INT       = (1 << 2),
    RHBZ_NOMAIL_NOTIFY  = (1 << 3),
    RHBZ_PRIVATE        = (1 << 4),
};

#define IS_MANDATORY(flags) ((flags) & RHBZ_MANDATORY_MEMB)
#define IS_READ_STR(flags) ((flags) & RHBZ_READ_STR)
#define IS_READ_INT(flags) ((flags) & RHBZ_READ_INT)
#define IS_NOMAIL_NOTIFY(flags) ((flags) & RHBZ_NOMAIL_NOTIFY)
#define IS_PRIVATE(flags) ((flags) & RHBZ_PRIVATE)

struct bug_info {
    int bi_id;
    int bi_dup_id;

    char *bi_status;
    char *bi_resolution;
    char *bi_reporter;
    char *bi_product;

    GList *bi_cc_list;
};

struct bug_info *new_bug_info();
void free_bug_info(struct bug_info *bz);

void rhbz_login(struct abrt_xmlrpc *ax, const char *login, const char *passwd);

void rhbz_mail_to_cc(struct abrt_xmlrpc *ax, int bug_id, const char *mail, int flags);

void rhbz_add_comment(struct abrt_xmlrpc *ax, int bug_id, const char *comment,
                      int flags);

void *rhbz_bug_read_item(const char *memb, xmlrpc_value *xml, int flags);

void rhbz_logout(struct abrt_xmlrpc *ax);

xmlrpc_value *rhbz_search_duphash(struct abrt_xmlrpc *ax, const char *component,
                                  const char *release, const char *duphash);

xmlrpc_value *rhbz_get_member(const char *member, xmlrpc_value *xml);

int rhbz_array_size(xmlrpc_value *xml);

int rhbz_bug_id(xmlrpc_value *xml);

int rhbz_new_bug(struct abrt_xmlrpc *ax, problem_data_t *problem_data,
                 int depend_on_bug);

int rhbz_attachments(struct abrt_xmlrpc *ax, const char *bug_id,
                     problem_data_t *problem_data, int flags);

int rhbz_attachment(struct abrt_xmlrpc *ax, const char *filename,
                    const char *bug_id, const char *data, int flags);

GList *rhbz_bug_cc(xmlrpc_value *result_xml);

struct bug_info *rhbz_bug_info(struct abrt_xmlrpc *ax, int bug_id);


struct bug_info *rhbz_find_origin_bug_closed_duplicate(struct abrt_xmlrpc *ax,
                                                       struct bug_info *bi);

#ifdef __cplusplus
}
#endif

#endif
