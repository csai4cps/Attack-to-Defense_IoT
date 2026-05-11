#!/bin/sh
set -e

CERTS=/certs
CLIENTS=$CERTS/clients

[ -f "$CERTS/ca.crt" ] && echo "Certs already exist, skipping." && exit 0

mkdir -p "$CLIENTS"

openssl genrsa -out "$CERTS/ca.key" 2048 2>/dev/null

openssl req -new -x509 -days 3650 \
  -key "$CERTS/ca.key" \
  -out "$CERTS/ca.crt" \
  -subj "/C=BR/ST=SP/L=SP/O=SENAI/CN=IoT-Lab-CA" 2>/dev/null

openssl genrsa -out "$CERTS/server.key" 2048 2>/dev/null

openssl req -new \
  -key "$CERTS/server.key" \
  -out "$CERTS/server.csr" \
  -subj "/C=BR/ST=SP/L=SP/O=SENAI/CN=mosquitto" 2>/dev/null

printf "subjectAltName=DNS:mosquitto,DNS:localhost,IP:127.0.0.1" > /tmp/san.cnf

openssl x509 -req -days 3650 \
  -in  "$CERTS/server.csr" \
  -CA  "$CERTS/ca.crt" \
  -CAkey "$CERTS/ca.key" \
  -CAcreateserial \
  -out "$CERTS/server.crt" \
  -extfile /tmp/san.cnf 2>/dev/null

for DEVICE in espcam cardputer kali; do
  openssl genrsa -out "$CLIENTS/$DEVICE.key" 2048 2>/dev/null

  openssl req -new \
    -key "$CLIENTS/$DEVICE.key" \
    -out "$CLIENTS/$DEVICE.csr" \
    -subj "/C=BR/ST=SP/L=SP/O=SENAI/CN=$DEVICE" 2>/dev/null

  openssl x509 -req -days 3650 \
    -in    "$CLIENTS/$DEVICE.csr" \
    -CA    "$CERTS/ca.crt" \
    -CAkey "$CERTS/ca.key" \
    -CAcreateserial \
    -out   "$CLIENTS/$DEVICE.crt" 2>/dev/null

  rm "$CLIENTS/$DEVICE.csr"
  echo "Generated: $DEVICE"
done

rm "$CERTS/server.csr"
chmod 644 "$CERTS/ca.crt" "$CERTS/server.crt"
chmod 600 "$CERTS/ca.key" "$CERTS/server.key"
chmod 644 "$CLIENTS/"*.crt
chmod 600 "$CLIENTS/"*.key

echo "All certificates generated successfully."
