#!/usr/bin/env python

import os
import sys
import time
import json
import ssl
import cgi
import BaseHTTPServer

class Handler(BaseHTTPServer.BaseHTTPRequestHandler):

    def do_POST(self):
        path = self.path

        if path == '/create':
            self.send_response(201)
            self.send_header("Content-type", "text/html")
            self.send_header("X-Task-Id", "582841017")
            self.send_header("X-Task-Password", "OXWE0DJg65NUsR9RGE1zgzG7pBbCYmh9")
            self.send_header("Connection", "close")
            self.end_headers()

            content_len = int(self.headers.getheader('content-length', 0))
            post_body = self.rfile.read(content_len)
        else:
            self.send_response(400)
            self.send_header("Content-type", "text/html")
            self.end_headers()

    def do_GET(self):

        path = self.path

        if path == '/create':
            self.send_response(201)
            self.send_header("Content-type", "text/html")
            self.send_header("X-Task-Id", "582841017")
            self.send_header("X-Task-Password", "OXWE0DJg65NUsR9RGE1zgzG7pBbCYmh9")
            self.end_headers()

            self.wfile.write("create message")
        # retrace server settings
        elif path == '/settings':
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()

            response = ''
            response += 'running_tasks 0\n'
            response += 'max_running_tasks 12\n'
            response += 'max_packed_size 1024\n'
            response += 'max_unpacked_size 1280\n'
            response += 'supported_formats application/x-tar application/x-xz-compressed-tar application/x-gzip\n'
            response += 'supported_releases CentOS-7-x86_64 centos-7-x86_64 fedora-20-armhfp fedora-20-armv7hl fedora-20-armv7l fedora-20-i386 fedora-20-x86_64 fedora-21-armhfp fedora-21-armv7hl fedora-21-armv7l fedora-21-i386 fedora-21-x86_64 fedora-22-armhfp fedora-22-i386 fedora-22-x86_64 fedora-rawhide-armhfp fedora-rawhide-armv7hl fedora-rawhide-armv7l fedora-rawhide-i386 fedora-rawhide-x86_64\n'

            self.wfile.write(response)
        elif path == '/checkpackage':
            self.send_response(302)
            self.send_header("Content-type", "text/html")
            self.send_header("Connection", "close")
            self.end_headers()
        elif path == '/582841017':
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.send_header("X-Task-Status", "FINISHED_SUCCESS")
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write("Preparing environment for backtrace generation")
        elif path == '/582841017/backtrace':
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.send_header("Connection", "close")
            self.end_headers()

            with open ("backtrace", "r") as myfile:
                response=myfile.read()

            self.wfile.write(response)
        elif path == '/582841017/exploitable':
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.send_header("Connection", "close")
            self.end_headers()

            self.wfile.write("backtrace raiting: 4")
        else:
            self.send_response(400)
            self.send_header("Content-type", "text/html")
            self.end_headers()


if __name__ == '__main__':

    HOST = '127.0.0.1'
    PORT = 12346
    print "Serving at port", PORT

    httpd = BaseHTTPServer.HTTPServer((HOST, PORT), Handler)
    httpd.socket = ssl.wrap_socket (httpd.socket,
                                    certfile='cert/server_cert.pem',
                                    keyfile='cert/server_key.pem',
                                    ca_certs='cert/ca_cert.pem',
                                    cert_reqs=ssl.CERT_NONE,
                                    server_side=True)

    httpd.serve_forever()
