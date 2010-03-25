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
#include "abrtlib.h"
#include "abrt_curl.h"
#include "CommLayerInner.h"

using namespace std;

/*
 * Utility function
 */
CURL* xcurl_easy_init()
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        error_msg_and_die("Can't create curl handle");
    }
    return curl;
}


/*
 * curl_post: perform HTTP POST transaction
 */
static char*
check_curl_error(CURLcode err, const char* msg)
{
    if (err)
        return xasprintf("%s: %s", msg, curl_easy_strerror(err));
    return NULL;
}

static void
die_if_curl_error(CURLcode err)
{
    char *msg = check_curl_error(err, "curl");
    if (msg)
        error_msg_and_die("%s", msg);
}

/* "save headers" callback */
static size_t
save_headers(void *buffer_pv, size_t count, size_t nmemb, void *ptr)
{
    curl_post_state_t* state = (curl_post_state_t*)ptr;
    size_t size = count * nmemb;


    unsigned cnt = state->header_cnt;
    state->headers = (char**)xrealloc(state->headers, (cnt+2) * sizeof(state->headers[0]));

    char *h = xstrndup((char*)buffer_pv, size);
    strchrnul(h, '\r')[0] = '\0';
    strchrnul(h, '\n')[0] = '\0';
    VERB3 log("save_headers: state:%p header %d: '%s'", state, cnt, h);
    state->headers[cnt] = h;
    state->header_cnt = ++cnt;
    state->headers[cnt] = NULL;

    return size;
}

int
curl_post(curl_post_state_t* state, const char* url, const char* data)
{
    CURLcode curl_err;
    struct curl_slist *httpheader_list = NULL;
    FILE* body_stream = NULL;
    long response_code;
    curl_post_state_t localstate;

    VERB3 log("curl_post('%s','%s')", url, data);

    if (!state)
    {
        memset(&localstate, 0, sizeof(localstate));
        state = &localstate;
    }

    state->http_resp_code = response_code = -1;
    state->curl_error_msg = NULL;

    CURL *handle = xcurl_easy_init();

    curl_err = curl_easy_setopt(handle, CURLOPT_VERBOSE, 0);
    die_if_curl_error(curl_err);
    curl_err = curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 1);
    die_if_curl_error(curl_err);
    curl_err = curl_easy_setopt(handle, CURLOPT_URL, url);
    die_if_curl_error(curl_err);
    curl_err = curl_easy_setopt(handle, CURLOPT_POST, 1);
    die_if_curl_error(curl_err);
    curl_err = curl_easy_setopt(handle, CURLOPT_POSTFIELDS, data);
    die_if_curl_error(curl_err);

    httpheader_list = curl_slist_append(httpheader_list, "Content-Type: application/xml");
    curl_err = curl_easy_setopt(handle, CURLOPT_HTTPHEADER, httpheader_list);
    die_if_curl_error(curl_err);

    if (state->flags & ABRT_CURL_POST_WANT_HEADERS)
    {
        curl_err = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, save_headers);
        die_if_curl_error(curl_err);
        curl_err = curl_easy_setopt(handle, CURLOPT_WRITEHEADER, state);
        die_if_curl_error(curl_err);
    }
    if (state->flags & ABRT_CURL_POST_WANT_BODY)
    {
        body_stream = open_memstream(&state->body, &state->body_size);
        if (!body_stream)
            error_msg_and_die("out of memory");
        curl_err = curl_easy_setopt(handle, CURLOPT_WRITEDATA, body_stream);
        die_if_curl_error(curl_err);
    }

    /* This is the place where everything happens. Here errors
     * are not limited to "out of memory", can't just die.
     */
    curl_err = curl_easy_perform(handle);
    if (curl_err)
    {
        if (state->flags & ABRT_CURL_POST_WANT_ERROR_MSG)
            state->curl_error_msg = check_curl_error(curl_err, "curl_easy_perform");
        goto ret;
    }

    curl_err = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response_code);
    die_if_curl_error(curl_err);
    state->http_resp_code = response_code;

 ret:
    curl_easy_cleanup(handle);
    curl_slist_free_all(httpheader_list);
    if (body_stream)
        fclose(body_stream);

    return response_code;
}

curl_post_state_t *new_curl_post_state(int flags)
{
    curl_post_state_t *state = (curl_post_state_t *)xzalloc(sizeof(*state));
    state->flags = flags;
    return state;
}

void free_curl_post_state(curl_post_state_t *state)
{
    char **headers = state->headers;
    if (headers)
    {
        while (*headers)
            free(*headers++);
        free(state->headers);
    }
    free(state->curl_error_msg);
    free(state->body);
    free(state);

}

char *find_header_in_curl_post_state(curl_post_state_t *state, const char *str)
{
    char **headers = state->headers;
    if (headers)
    {
        unsigned len = strlen(str);
        while (*headers)
        {
            if (strncmp(*headers, str, len) == 0)
                return skip_whitespace(*headers + len);
            headers++;
        }
    }
    return NULL;
}
