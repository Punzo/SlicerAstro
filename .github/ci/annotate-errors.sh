#!/bin/bash
# Show the error lines of a build or test log as a GitHub annotation, so that a
# failure can be read from the run summary (and from the public check-run
# annotations API) without downloading the job log.
#   annotate-errors.sh <log file> <title>
log=$1
title=${2:-Errors}
[ -f "$log" ] || exit 0
lines=$(grep -E -A2 -- \
  'error[: ]|Error[: ]|ERROR|FAILED|\*\*\*Failed|FAIL:|fatal|undefined reference|unresolved external|Traceback|Assertion|Segmentation|exit abnormally|No such file' \
  "$log" | grep -v -E '^\s*$' | head -n 200)
[ -n "$lines" ] || lines=$(tail -n 60 "$log")
# workflow command encoding: %, CR and LF
lines=${lines//'%'/'%25'}
lines=${lines//$'\r'/'%0D'}
lines=${lines//$'\n'/'%0A'}
echo "::error title=${title}::${lines}"
