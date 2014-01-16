#!/bin/bash

# This script is intended to regenerate the certificates and keys, possibly
# with different parameters if needed. It is not required to generate new
# certificates for every test run; in fact it can block for a really long time
# if run inside a virtual machine that is short on entropy.

set -e
set -x

NEWKEY="rsa:2048"
DAYS="3650"

rm ca/*
echo -n "" > ca/index.txt
echo 01 > ca/serial.txt

# Generate self-signed CA certificate.
openssl req \
        -config ssl_ca.conf \
        -new \
        -x509 \
        -days $DAYS \
        -newkey $NEWKEY \
        -nodes \
        -keyout ca_key.pem \
        -out ca_cert.pem

# Generate self-signed server certificate.
openssl req \
        -config ssl_server.conf \
        -new \
        -x509 \
        -days $DAYS \
        -newkey $NEWKEY \
        -nodes \
        -keyout server_key.pem \
        -out server_cert.pem

# Generate client certificate request to be signed by CA.
openssl req \
        -config ssl_client.conf \
        -newkey $NEWKEY \
        -nodes \
        -keyout client_key.pem \
        -out client_req.pem

# Sign the client certificate request to obtain the client certificate.
openssl ca \
        -config ssl_ca.conf \
        -batch \
        -days $DAYS \
        -cert ca_cert.pem \
        -keyfile ca_key.pem \
        -in client_req.pem \
        -outdir ca \
        -out client_cert.pem
rm client_req.pem

# Convert the client certificate to PKCS#12 for testing with web browser.
# Empty password.
openssl pkcs12 \
        -export \
        -name "ABRT ureport-auth test cert" \
        -in client_cert.pem \
        -inkey client_key.pem \
        -passout "pass:" \
        -out client_cert.p12
