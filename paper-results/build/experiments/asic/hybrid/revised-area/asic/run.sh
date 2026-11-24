#!/bin/bash
root="$1"
shift
for family in "$@"; do
  cd "$root/$family" || exit 1
  date -u +%FT%TZ >started
  /tmp/cgra-area-20261030/dc.sh -f synth.tcl >dc.log 2>&1
  status=$?
  echo "$status" >exit
  date -u +%FT%TZ >finished
done
