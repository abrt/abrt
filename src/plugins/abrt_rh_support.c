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
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "abrt_curl.h"
#include "abrt_rh_support.h"

struct reportfile {
    xmlTextWriterPtr writer;
    xmlBufferPtr     buf;
};

static void __attribute__((__noreturn__))
die_xml_oom(void)
{
    error_msg_and_die("can't create XML attribute (out of memory?)");
}

static xmlBufferPtr
xxmlBufferCreate(void)
{
    xmlBufferPtr r = xmlBufferCreate();
    if (!r)
        die_xml_oom();
    return r;
}

static xmlTextWriterPtr
xxmlNewTextWriterMemory(xmlBufferPtr buf /*, int compression*/)
{
    xmlTextWriterPtr r = xmlNewTextWriterMemory(buf, /*compression:*/ 0);
    if (!r)
        die_xml_oom();
    return r;
}

static void
xxmlTextWriterStartDocument(xmlTextWriterPtr writer,
    const char * version,
    const char * encoding,
    const char * standalone)
{
    if (xmlTextWriterStartDocument(writer, version, encoding, standalone) < 0)
        die_xml_oom();
}

static void
xxmlTextWriterEndDocument(xmlTextWriterPtr writer)
{
    if (xmlTextWriterEndDocument(writer) < 0)
        die_xml_oom();
}

static void
xxmlTextWriterStartElement(xmlTextWriterPtr writer, const char *name)
{
    // these bright guys REDEFINED CHAR (!) to unsigned char...
    if (xmlTextWriterStartElement(writer, (unsigned char*)name) < 0)
        die_xml_oom();
}

static void
xxmlTextWriterEndElement(xmlTextWriterPtr writer)
{
    if (xmlTextWriterEndElement(writer) < 0)
        die_xml_oom();
}

static void
xxmlTextWriterWriteElement(xmlTextWriterPtr writer, const char *name, const char *content)
{
    if (xmlTextWriterWriteElement(writer, (unsigned char*)name, (unsigned char*)content) < 0)
        die_xml_oom();
}

static void
xxmlTextWriterWriteAttribute(xmlTextWriterPtr writer, const char *name, const char *content)
{
    if (xmlTextWriterWriteAttribute(writer, (unsigned char*)name, (unsigned char*)content) < 0)
        die_xml_oom();
}

#if 0 //unused
static void
xxmlTextWriterWriteString(xmlTextWriterPtr writer, const char *content)
{
    if (xmlTextWriterWriteString(writer, (unsigned char*)content) < 0)
        die_xml_oom();
}
#endif

//
// End the reportfile, and prepare it for delivery.
// No more bindings can be added after this.
//
static void
close_writer(reportfile_t* file)
{
    if (!file->writer)
        return;

    // close off the end of the xml file
    xxmlTextWriterEndDocument(file->writer);
    xmlFreeTextWriter(file->writer);
    file->writer = NULL;
}

//
// This allocates a reportfile_t structure and initializes it.
//
reportfile_t*
new_reportfile(void)
{
    // create a new reportfile_t
    reportfile_t* file = (reportfile_t*)xmalloc(sizeof(*file));

    // set up a libxml 'buffer' and 'writer' to that buffer
    file->buf = xxmlBufferCreate();
    file->writer = xxmlNewTextWriterMemory(file->buf);

    // start a new xml document:
    // <report xmlns="http://www.redhat.com/gss/strata">...
    xxmlTextWriterStartDocument(file->writer, /*version:*/ NULL, /*encoding:*/ NULL, /*standalone:*/ NULL);
    xxmlTextWriterStartElement(file->writer, "report");
    xxmlTextWriterWriteAttribute(file->writer, "xmlns", "http://www.redhat.com/gss/strata");

    return file;
}

static void
internal_reportfile_start_binding(reportfile_t* file, const char* name, int isbinary, const char* filename)
{
    // <binding name=NAME [fileName=FILENAME] type=text/binary...
    xxmlTextWriterStartElement(file->writer, "binding");
    xxmlTextWriterWriteAttribute(file->writer, "name", name);
    if (filename)
        xxmlTextWriterWriteAttribute(file->writer, "fileName", filename);
    if (isbinary)
        xxmlTextWriterWriteAttribute(file->writer, "type", "binary");
    else
        xxmlTextWriterWriteAttribute(file->writer, "type", "text");
}

//
// Add a new text binding
//
void
reportfile_add_binding_from_string(reportfile_t* file, const char* name, const char* value)
{
    // <binding name=NAME type=text value=VALUE>
    internal_reportfile_start_binding(file, name, /*isbinary:*/ 0, /*filename:*/ NULL);
    xxmlTextWriterWriteAttribute(file->writer, "value", value);
    xxmlTextWriterEndElement(file->writer);
}

//
// Add a new binding to a report whose value is represented as a file.
//
void
reportfile_add_binding_from_namedfile(reportfile_t* file,
                const char* on_disk_filename, /* unused so far */
                const char* binding_name,
                const char* recorded_filename,
                int isbinary)
{
    // <binding name=NAME fileName=FILENAME type=text/binary...
    internal_reportfile_start_binding(file, binding_name, isbinary, recorded_filename);
    // ... href=content/NAME>
    char *href_name = concat_path_file("content", binding_name);
    xxmlTextWriterWriteAttribute(file->writer, "href", href_name);
    free(href_name);
}

//
// Return the contents of the reportfile as a string.
//
const char*
reportfile_as_string(reportfile_t* file)
{
    close_writer(file);
    // unsigned char -> char
    return (char*)file->buf->content;
}

void
reportfile_free(reportfile_t* file)
{
    if (!file)
        return;
    close_writer(file);
    xmlBufferFree(file->buf);
    free(file);
}


//
// send_report_to_new_case()
//

static char*
make_case_data(const char* summary, const char* description,
               const char* product, const char* version,
               const char* component)
{
    char* retval;
    xmlTextWriterPtr writer;
    xmlBufferPtr buf;

    buf = xxmlBufferCreate();
    writer = xxmlNewTextWriterMemory(buf);

    xxmlTextWriterStartDocument(writer, NULL, "UTF-8", "yes");
    xxmlTextWriterStartElement(writer, "case");
    xxmlTextWriterWriteAttribute(writer, "xmlns",
                                   "http://www.redhat.com/gss/strata");

    xxmlTextWriterWriteElement(writer, "summary", summary);
    xxmlTextWriterWriteElement(writer, "description", description);
    if (product) {
        xxmlTextWriterWriteElement(writer, "product", product);
    }
    if (version) {
        xxmlTextWriterWriteElement(writer, "version", version);
    }
    if (component) {
        xxmlTextWriterWriteElement(writer, "component", component);
    }

    xxmlTextWriterEndDocument(writer);
    retval = xstrdup((const char*)buf->content);
    xmlFreeTextWriter(writer);
    xmlBufferFree(buf);
    return retval;
}

#if 0 //unused
static char*
make_response(const char* title, const char* body,
              const char* actualURL, const char* displayURL)
{
    char* retval;
    xmlTextWriterPtr writer;
    xmlBufferPtr buf;

    buf = xxmlBufferCreate();
    writer = xxmlNewTextWriterMemory(buf);

    xxmlTextWriterStartDocument(writer, NULL, "UTF-8", "yes");
    xxmlTextWriterStartElement(writer, "response");
    if (title) {
        xxmlTextWriterWriteElement(writer, "title", title);
    }
    if (body) {
        xxmlTextWriterWriteElement(writer, "body", body);
    }
    if (actualURL || displayURL) {
        xxmlTextWriterStartElement(writer, "URL");
        if (actualURL) {
            xxmlTextWriterWriteAttribute(writer, "href", actualURL);
        }
        if (displayURL) {
            xxmlTextWriterWriteString(writer, displayURL);
        }
    }

    xxmlTextWriterEndDocument(writer);
    retval = xstrdup((const char*)buf->content);
    xmlFreeTextWriter(writer);
    xmlBufferFree(buf);
    return retval;
}
//Example:
//<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
//<response><title>Case Created and Report Attached</title><body></body><URL href="http://support-services-devel.gss.redhat.com:8080/Strata/cases/00005129/attachments/ccbf3e65-b941-3db7-a016-6a3831691a32">New Case URL</URL></response>
#endif

char*
send_report_to_new_case(const char* baseURL,
                const char* username,
                const char* password,
                bool ssl_verify,
                const char* summary,
                const char* description,
                const char* component,
                const char* report_file_name)
{
    char *case_url = concat_path_file(baseURL, "/cases");

    char *case_data = make_case_data(summary, description,
                                         "Red Hat Enterprise Linux", "6.0",
                                         component);

    int redirect_count = 0;
    char *errmsg;
    char *allocated = NULL;
    char* retval = NULL;
    abrt_post_state_t *case_state;

 redirect_case:
    case_state = new_abrt_post_state(0
            + ABRT_POST_WANT_HEADERS
            + ABRT_POST_WANT_BODY
            + ABRT_POST_WANT_ERROR_MSG
            + (ssl_verify ? ABRT_POST_WANT_SSL_VERIFY : 0)
    );
    case_state->username = username;
    case_state->password = password;
    abrt_post_string(case_state, case_url, "application/xml", case_data);

    char *case_location = find_header_in_abrt_post_state(case_state, "Location:");
    switch (case_state->http_resp_code)
    {
    case 404:
        /* Not strictly necessary (default branch would deal with it too),
         * but makes this typical error less cryptic:
         * instead of returning html-encoded body, we show short concise message,
         * and show offending URL (typos in which is a typical cause) */
        retval = xasprintf("error in case creation, "
                        "HTTP code: 404 (Not found), URL:'%s'", case_url);
        break;

    case 301: /* "301 Moved Permanently" (for example, used to move http:// to https://) */
    case 302: /* "302 Found" (just in case) */
    case 305: /* "305 Use Proxy" */
        if (++redirect_count < 10 && case_location)
        {
            free(case_url);
            case_url = xstrdup(case_location);
            free_abrt_post_state(case_state);
            goto redirect_case;
        }
        /* fall through */

    default:
        errmsg = case_state->curl_error_msg;
        if (errmsg && errmsg[0])
            retval = xasprintf("error in case creation: %s", errmsg);
        else
        {
            errmsg = case_state->body;
            if (errmsg && errmsg[0])
                retval = xasprintf("error in case creation, HTTP code: %d, server says: '%s'",
                        case_state->http_resp_code, errmsg);
            else
                retval = xasprintf("error in case creation, HTTP code: %d",
                        case_state->http_resp_code);
        }
        break;

    case 200:
    case 201: {
        if (!case_location) {
            /* Case Creation returned valid code, but no location */
            retval = xasprintf("error in case creation: no Location URL, HTTP code: %d",
                    case_state->http_resp_code);
            break;
        }

        char *atch_url = concat_path_file(case_location, "/attachments");
        abrt_post_state_t *atch_state;
 redirect_attach:
        atch_state = new_abrt_post_state(0
                + ABRT_POST_WANT_HEADERS
                + ABRT_POST_WANT_BODY
                + ABRT_POST_WANT_ERROR_MSG
                + (ssl_verify ? ABRT_POST_WANT_SSL_VERIFY : 0)
        );
        atch_state->username = username;
        atch_state->password = password;
        abrt_post_file_as_form(atch_state, atch_url, "application/binary", report_file_name);

        char *atch_location = find_header_in_abrt_post_state(atch_state, "Location:");
        switch (atch_state->http_resp_code)
        {
        case 305: /* "305 Use Proxy" */
            if (++redirect_count < 10 && atch_location)
            {
                free(atch_url);
                atch_url = xstrdup(atch_location);
                free_abrt_post_state(atch_state);
                goto redirect_attach;
            }
            /* fall through */

        default:
            /* Case Creation Succeeded, attachement FAILED */
            errmsg = atch_state->curl_error_msg;
            if (atch_state->body && atch_state->body[0])
            {
                if (errmsg && errmsg[0]
                 && strcmp(errmsg, atch_state->body) != 0
                ) /* both strata/curl error and body are present (and aren't the same) */
                    allocated = errmsg = xasprintf("%s. %s",
                            atch_state->body,
                            errmsg);
                else /* only body exists */
                    errmsg = atch_state->body;
            }
            /* Note: to prevent URL misparsing, make sure to delimit
             * case_location only using spaces */
            retval = xasprintf("Case created: %s but report attachment failed (HTTP code %d)%s%s",
                    case_location,
                    atch_state->http_resp_code,
                    errmsg ? ": " : "",
                    errmsg ? errmsg : ""
	    );
            break;

        case 200:
        case 201:
            // unused
            //char *body = atch_state->body;
            //if (case_state->body && case_state->body[0])
            //{
            //    body = case_state->body;
            //    if (atch_state->body && atch_state->body[0])
            //        allocated = body = xasprintf("%s\n%s",
            //                case_state->body,
            //                atch_state->body);
            //}
            retval = xasprintf("Case created: %s", /*body,*/ case_location);
        } /* switch (attach HTTP code) */

        free_abrt_post_state(atch_state);
        free(atch_url);
    } /* case 200/201 */

    } /* switch (case HTTP code) */

    free_abrt_post_state(case_state);
    free(allocated);
    free(case_url);
    return retval;
}
