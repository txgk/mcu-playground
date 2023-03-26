#!/bin/sh

device_path=
serial_speed='115200'
log_dir='/tmp/i2c-dumps'
log_path="$log_dir/$(date +'%Y%m%d-%H%M%S')"

[ -n "$1" ] && device_path="$1"
[ -z "$device_path" ] && [ -e /dev/ttyUSB0 ] && device_path='/dev/ttyUSB0'
[ -z "$device_path" ] && [ -e /dev/ttyACM0 ] && device_path='/dev/ttyACM0'
[ -z "$device_path" ] && echo 'Failed to find device!' && exit 1

mkdir -p "$log_dir"
ln -sf "$log_path" "$log_dir/recent"
stty -F "$device_path" "$serial_speed" raw crtscts -echo
echo "Writing captured data to ${log_path}..."
cat "$device_path" > "$log_path"
