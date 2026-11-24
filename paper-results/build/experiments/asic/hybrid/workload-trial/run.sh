#!/bin/bash
cd "$1" || exit 1
sha256sum all.v synth.tcl > inputs.sha256
date -u +%FT%TZ > started
/tmp/cgra-area-20261030/dc.sh -f synth.tcl > dc.log 2>&1
status=$?
echo "$status" > exit
date -u +%FT%TZ > finished
