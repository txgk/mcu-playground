#!/bin/sh
cd "$(dirname "$0")" || exit 1
gzip --best -c panel.html > main/panel.html.gz
