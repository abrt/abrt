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
#include "Python.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "Python_hash.h"

using namespace std;

string CAnalyzerPython::GetLocalUUID(const char *pDebugDumpDir)
{
	CDebugDump dd;
	dd.Open(pDebugDumpDir);
	string bt;
	dd.LoadText(FILENAME_BACKTRACE, bt);

	const char *bt_str = bt.c_str();
	const char *bt_end = strchrnul(bt_str, '\n');

	char hash_str[MD5_RESULT_LEN*2 + 1];
	unsigned char hash2[MD5_RESULT_LEN];
	md5_ctx_t md5ctx;
	md5_begin(&md5ctx);
	// Better:
	// "example.py:1:<module>:ZeroDivisionError: integer division or modulo by zero"
	//md5_hash(bt_str, bt_end - bt_str, &md5ctx);
	// For now using compat version:
	{
		char *copy = xstrndup(bt_str, bt_end - bt_str);
		char *s = copy;
		char *d = copy;
		unsigned colon_cnt = 0;
		while (*s && colon_cnt < 3) {
			if (*s != ':')
				*d++ = *s;
			else
				colon_cnt++;
			s++;
		}
		// "example.py1<module>"
		md5_hash(copy, d - copy, &md5ctx);
//*d = '\0'; log("str:'%s'", copy);
		free(copy);
	}
	md5_end(hash2, &md5ctx);

	// Hash is MD5_RESULT_LEN bytes long, but we use only first 4
	// (I don't know why old Python code was using only 4, I mimic that)
	unsigned len = 4;
	char *d = hash_str;
	unsigned char *s = hash2;
	while (len) {
		*d++ = "0123456789abcdef"[*s >> 4];
		*d++ = "0123456789abcdef"[*s & 0xf];
		s++;
		len--;
	}
	*d = '\0';
//log("hash2:%s str:'%.*s'", hash_str, (int)(bt_end - bt_str), bt_str);

	return hash_str;
}
string CAnalyzerPython::GetGlobalUUID(const char *pDebugDumpDir)
{
	return GetLocalUUID(pDebugDumpDir);
}

void CAnalyzerPython::Init()
{
}

void CAnalyzerPython::DeInit()
{
}

PLUGIN_INFO(ANALYZER,
            CAnalyzerPython,
            "Python",
            "0.0.1",
            _("Analyzes crashes in Python programs"),
            "zprikryl@redhat.com, jmoskovc@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            "");
