#!/usr/bin/env bash
major=$(awk -v device="memctl" '$2==device{print $1}' /proc/devices)
if [[ -n "$major" ]]; then
  mknod /dev/memctl c "$major" 0
else
  echo "memctl device does not exist"
fi
