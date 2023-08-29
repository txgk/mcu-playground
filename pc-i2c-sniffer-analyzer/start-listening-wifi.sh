#!/bin/sh

device_ip='192.168.102.41'
port='80'
log_dir='/tmp/i2c-dumps'
#log_dir='/mnt/c/LabVIEWProj'
log_path="$log_dir/$(date +'%Y%m%d-%H%M%S').txt"

[ -n "$1" ] && device_ip="$1"

mkdir -p "$log_dir"
ln -sf "$log_path" "$log_dir/recent"

echo "Writing captured data to ${log_path}..."
#curl --http0.9 --no-buffer -o "$log_path" "${device_ip}:${port}"
if [ "$(uname)" != 'OpenBSD' ]; then
	nc -o "$log_path" "$device_ip" "$port"
else
	nc "$device_ip" "$port" | tee "$log_path"
fi
