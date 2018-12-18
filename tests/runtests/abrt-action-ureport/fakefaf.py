#!/usr/bin/env python3
# Single purpose HTTP server
# - accepts POST of ureport JSON and dumps it to a file

import sys
import json
import cgi
import http.server

class Handler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        # parse form data
        form = cgi.FieldStorage(
            fp=self.rfile,
            headers=self.headers,
            environ={
                'REQUEST_METHOD': 'POST',
                'CONTENT_TYPE': self.headers['Content-Type'],
            }
        )

        self.send_response(202)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Connection', 'close')
        self.end_headers()

        if self.path == '/faf/reports/new/':
            ureport = json.load(form['file'].file)
            with open(self.save_ureport, 'w') as fh:
                json.dump(ureport, fh, indent=2)

            response = {
                'bthash': '691cf824e3e07457156125636e86c50279e29496',
                'message': 'https://retrace.fedoraproject.org/faf/reports/6437/\nhttps://bugzilla.redhat.com/show_bug.cgi?id=851210',
                'reported_to': [
                    {
                        'type': 'url',
                        'value': 'https://retrace.fedoraproject.org/faf/reports/6437/',
                        'reporter': 'ABRT Server'
                    },
                    {
                        'type': 'url',
                        'value': 'https://bugzilla.redhat.com/show_bug.cgi?id=851210',
                        'reporter': 'Bugzilla'
                    }
                ],
                'result': True
            }
        elif self.path == '/faf/reports/attach/':
            ureport = json.load(form['file'].file)
            with open(self.save_ureport, 'w') as fh:
                json.dump(ureport, fh, indent=2)

            response = {'result': True}

        else:
            with open(self.save_ureport, 'w') as fh:
                fh.write('{"invalid_request_path": "%s"}' % self.path)
            return

        self.wfile.write(json.dumps(response, indent=2).encode('utf-8'))

    def log_message(self, format, *args):
        super(Handler, self).log_message(format, *args)

        sys.stderr.flush()

PORT = 12345
print("Serving at port", PORT)

Handler.save_ureport = sys.argv[1] if len(sys.argv) > 1 else 'ureport.json'
httpd = http.server.HTTPServer(("", PORT), Handler)
httpd.serve_forever()
