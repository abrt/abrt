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
//#define _GNU_SOURCE

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "abrt_curl.h"
#include "abrt_xmlrpc.h"
#include "ABRTException.h"
#include "abrt_rh_support.h"

using namespace std;

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

#if 0 //unused
static void
xxmlTextWriterWriteElement(xmlTextWriterPtr writer, const char *name, const char *content)
{
    if (xmlTextWriterWriteElement(writer, (unsigned char*)name, (unsigned char*)content) < 0)
        die_xml_oom();
}
#endif

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
    string href_name = concat_path_file("content", binding_name);
    xxmlTextWriterWriteAttribute(file->writer, "href", href_name.c_str());
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


char*
post_signature(const char* baseURL, const char* signature)
{
    string URL = concat_path_file(baseURL, "/signatures");

    abrt_post_state *state = new_abrt_post_state(0
                + ABRT_POST_WANT_HEADERS
                + ABRT_POST_WANT_BODY
                + ABRT_POST_WANT_ERROR_MSG);
    int http_resp_code = abrt_post_string(state, URL.c_str(), "application/xml", signature);

    char *retval;
    const char *strata_msg;
    switch (http_resp_code)
    {
    case 200:
    case 201:
        if (state->body)
        {
            retval = state->body;
            state->body = NULL;
            break;
        }
        strata_msg = find_header_in_abrt_post_state(state, "Strata-Message:");
        if (strata_msg && strcmp(strata_msg, "CREATED") != 0) {
            retval = xstrdup(strata_msg);
            break;
        }
        retval = xstrdup("Signature submitted successfully");
        break;

    default:
        strata_msg = find_header_in_abrt_post_state(state, "Strata-Message:");
        if (strata_msg)
        {
            retval = xasprintf("Error (HTTP response %d): %s",
                        http_resp_code,
                        strata_msg);
            break;
        }
        if (state->curl_error_msg)
        {
            if (http_resp_code >= 0)
                retval = xasprintf("Error (HTTP response %d): %s", http_resp_code, state->curl_error_msg);
            else
                retval = xasprintf("Error in HTTP transaction: %s", state->curl_error_msg);
            break;
        }
        retval = xasprintf("Error (HTTP response %d), body:\n%s", http_resp_code, state->body);
        break;
    }

    free_abrt_post_state(state);
    return retval;
}

#if 0
const char*
create_case(const char* baseURL, const char* description)
{
  const char* URL = concat_path_file(baseURL, "/cases");
  const char* retval;

  response_data_t* response_data = post(URL, description);
  if (!response_data)
    return NULL;

  switch (response_data->code) {
  case 200:
  case 201:
    if (response_data->body && strlen(response_data->body) > 0) {
      retval = response_data->body;
      response_data->body = NULL;
    }
    else
      retval = ssprintf("Case Created: %s\n", response_data->location);
    break;
  default:
    if (response_data->strata_message)
      retval = ssprintf("Error: %s (http code %ld)",
                        response_data->strata_message,
                        response_data->code );
    else
      retval = ssprintf("Error: Response Code: %ld\nBody:\n%s", response_data->code, response_data->body);
  }
    free_abrt_post_state(state);

  free((void*)response_data->strata_message);
  free((void*)response_data->body);
  free((void*)response_data->location);
  free((void *)response_data);
  free((void*)URL);
  return retval;
}
#endif
