#!/bin/sh

device_ip='192.168.4.1'
port='80'
log_dir='/tmp/i2c-dumps'
log_path="$log_dir/$(date +'%Y%m%d-%H%M%S')"

[ -n "$1" ] && device_ip="$1"

mkdir -p "$log_dir"
ln -sf "$log_path" "$log_dir/recent"
echo "Writing captured data to ${log_path}..."
curl --http0.9 --no-buffer -o "$log_path" "${device_ip}:${port}"
