#!/bin/bash
cd "$1" || exit 1
/tmp/cgra-area-20261030/dc.sh -f synth.tcl >dc.log 2>&1
status=$?
echo "$status" >exit
exit "$status"
