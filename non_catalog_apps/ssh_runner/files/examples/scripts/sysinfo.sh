#!/bin/sh
# sysinfo.sh - one-screen status of an Ubuntu host, sized for the Flipper.
# Read-only: changes nothing on the host.

echo "== $(hostname)"
date '+%F %T'

echo
echo "-- os"
. /etc/os-release
echo "$PRETTY_NAME"
uname -rm

echo
echo "-- up / load"
awk '{s=$1; printf "%dd %dh %dm\n", s/86400, s%86400/3600, s%3600/60}' /proc/uptime
cut -d' ' -f1-3 /proc/loadavg

echo
echo "-- ip"
ip -4 -o addr show | awk '$2 !~ /^(lo|docker|br-|veth)/ {
  split($4, a, "/"); p = a[2]; m = ""
  for (i = 0; i < 4; i++) {
    b = p > 8 ? 8 : (p > 0 ? p : 0)
    m = m (i ? "." : "") 256 - 2^(8 - b)
    p -= 8
  }
  print $2; print " " a[1]; print " " m
}'

echo
echo "-- wifi"
if command -v nmcli >/dev/null 2>&1; then
  nmcli -t -f ACTIVE,SSID,SIGNAL dev wifi | awk -F: '$1 == "yes" {print $2 " " $3 "%"}'
else
  echo "no nmcli"
fi

echo
echo "-- disk"
df -h / /dev/shm | awk 'NR > 1 {print $6, $5; print " used " $3 "/" $2; print " free " $4}'

echo
echo "-- mem"
free -h | awk 'NR == 2 {print "mem " $3 "/" $2} NR == 3 {print "swap " $3 "/" $2}'

echo
echo "-- ping 8.8.8.8"
ping -c 4 -q -W 2 8.8.8.8 | awk -F/ '
  /loss/ {split($0, x, ", "); print x[3]}
  /rtt/  {print "avg " $5 " ms"}'
