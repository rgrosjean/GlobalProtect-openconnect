#!/bin/sh -e
# Generate throwaway certificates for the request helper tests.
# Usage: gen-certs.sh <openssl> <output dir>

OPENSSL="$1"
DIR="$2"
SAN="subjectAltName=IP:127.0.0.1,DNS:localhost"

# Test CA
"$OPENSSL" req -x509 -newkey rsa:2048 -nodes -days 3650 \
	-keyout "$DIR/ca.key" -out "$DIR/ca.pem" \
	-subj "/CN=gpclient test CA" \
	-addext "basicConstraints=critical,CA:TRUE" \
	-addext "keyUsage=critical,keyCertSign,cRLSign" 2>/dev/null

# Server certificate signed by the test CA
"$OPENSSL" req -newkey rsa:2048 -nodes \
	-keyout "$DIR/server.key" -out "$DIR/server.csr" \
	-subj "/CN=localhost" 2>/dev/null
printf '%s\nbasicConstraints=CA:FALSE\nextendedKeyUsage=serverAuth\n' "$SAN" > "$DIR/server.ext"
"$OPENSSL" x509 -req -days 3650 -in "$DIR/server.csr" \
	-CA "$DIR/ca.pem" -CAkey "$DIR/ca.key" -CAcreateserial \
	-extfile "$DIR/server.ext" -out "$DIR/server.pem" 2>/dev/null

# Self-signed server certificate
"$OPENSSL" req -x509 -newkey rsa:2048 -nodes -days 3650 \
	-keyout "$DIR/selfsigned.key" -out "$DIR/selfsigned.pem" \
	-subj "/CN=localhost" \
	-addext "$SAN" 2>/dev/null

rm -f "$DIR/server.csr" "$DIR/server.ext" "$DIR/ca.srl"
