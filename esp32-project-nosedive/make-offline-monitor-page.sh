#!/bin/sh
cd "$(dirname "$0")" || exit 1
monitor_file='monitor.html'
monitor_offline_file='monitor-offline.html'
monitor_offline_file_compressed="${monitor_offline_file}.gz"
chartjs_file='chart.umd.js'
chartjs_file_source='https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.3.0/chart.umd.js'
[ -e "$chartjs_file" ] || wget -O "$chartjs_file" "$chartjs_file_source"
[ -e "$chartjs_file" ] || exit 1
head -n -2 "$monitor_file" > "$monitor_offline_file"
sed -i '/jsdelivr.net/d' "$monitor_offline_file"
echo '<script>' >> "$monitor_offline_file"
cat "$chartjs_file" >> "$monitor_offline_file"
echo '</script></body></html>' >> "$monitor_offline_file"
gzip --best -c "$monitor_offline_file" > main/"$monitor_offline_file_compressed"
