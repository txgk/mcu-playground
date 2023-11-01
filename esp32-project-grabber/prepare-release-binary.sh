#!/bin/sh
project_name='grabber'
file_name="${project_name}-release-$(date +'%Y-%m-%d').bin"
cd "$(dirname "$0")"/build || exit 1
flash_args="$(cat flash_args)"
esptool.py --chip esp32 merge_bin -o ../"$file_name" $flash_args
