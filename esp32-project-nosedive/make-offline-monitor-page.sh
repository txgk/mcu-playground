#!/bin/sh
cd "$(dirname "$0")" || exit 1
monitor_file='monitor.html'
monitor_offline_file='monitor-offline.html'
monitor_offline_file_compressed="${monitor_offline_file}.gz"
plotly_file='plotly-latest.min.js'
plotly_file_source='https://cdn.plot.ly/plotly-latest.min.js'
[ -e "$plotly_file" ] || wget -O "$plotly_file" "$plotly_file_source"
[ -e "$plotly_file" ] || exit 1
head -n -2 "$monitor_file" > "$monitor_offline_file"
sed -i '/plotly-latest.min.js/d' "$monitor_offline_file"
echo '<script>' >> "$monitor_offline_file"
cat "$plotly_file" >> "$monitor_offline_file"
echo '</script></body></html>' >> "$monitor_offline_file"
gzip --best -c "$monitor_offline_file" > main/"$monitor_offline_file_compressed"
