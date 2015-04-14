#!/usr/bin/env python
# Single purpose HTTP server
# - accepts POST of ureport JSON and dumps it to a file

import sys
import json
import BaseHTTPServer, cgi

class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
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

        if self.path == '/reports/new/':
            ureport = json.load(form['file'].file)
            with open(self.save_ureport, 'w') as fh:
                json.dump(ureport, fh, indent=2)

            response = {
                'bthash': '691cf824e3e07457156125636e86c50279e29496',
                'reported_to': [
                    {}
                ],
                'result': False
            }
        else:
            with open(self.save_ureport, 'w') as fh:
                fh.write('{"invalid_request_path": "%s"}' % self.path)
            return

        json.dump(response, self.wfile, indent=2)

PORT = 12345
print "Serving at port", PORT

Handler.save_ureport = sys.argv[1] if len(sys.argv) > 1 else 'ureport.json'
httpd = BaseHTTPServer.HTTPServer(("127.0.0.1", PORT), Handler)
httpd.serve_forever()
