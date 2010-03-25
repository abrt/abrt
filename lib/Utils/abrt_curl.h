/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#ifndef ABRT_CURL_H_
#define ABRT_CURL_H_

#include <curl/curl.h>

CURL* xcurl_easy_init();

typedef struct curl_post_state {
    int      flags;
    int      http_resp_code;
    unsigned header_cnt;
    char     **headers;
    char     *curl_error_msg;
    char     *body;
    size_t   body_size;
} curl_post_state_t;
enum {
    ABRT_CURL_POST_WANT_HEADERS   = (1 << 0),
    ABRT_CURL_POST_WANT_ERROR_MSG = (1 << 1),
    ABRT_CURL_POST_WANT_BODY      = (1 << 2),
};
curl_post_state_t *new_curl_post_state(int flags);
void free_curl_post_state(curl_post_state_t *state);
int curl_post(curl_post_state_t* state, const char* url, const char* data);
char *find_header_in_curl_post_state(curl_post_state_t *state, const char *str);

#endif
