#!/bin/bash
# Show the error lines of a build or test log as GitHub annotations, so that a
# failure can be read from the run summary (and from the public check-run
# annotations API) without downloading the job log.
#   annotate-errors.sh <log file> <title>
# GitHub keeps about 4 KB of each annotation and 10 error annotations per step:
# compiler and linker command lines are left out, long lines are cut and the
# lines are split into several annotations.
log=$1
title=${2:-Errors}
[ -f "$log" ] || exit 0
pattern='error[: ]|Error[: ]|ERROR|FAILED|\*\*\*Failed|FAIL:|fatal|undefined reference|unresolved external|Traceback|Assertion|Segmentation|exit abnormally|No such file|exit code'
lines=$(grep -E -- "$pattern" "$log" \
  | grep -v -E -i '(cl|link|c\+\+|g\+\+|gcc|cc|clang\+\+|clang)(\.exe)?"? .*(-c |/c |-o |/out:|/nologo)|^LINK: command|cmd\.exe /C' \
  | cut -c1-300 | awk '!seen[$0]++' | head -n 120)
[ -n "$lines" ] || lines=$(tail -n 40 "$log" | cut -c1-300)

emit() {
  local text=$1
  text=${text//'%'/'%25'}
  text=${text//$'\r'/'%0D'}
  text=${text//$'\n'/'%0A'}
  echo "::error title=${title} ($2)::${text}"
}

part=1
chunk=""
while IFS= read -r line; do
  if [ $(( ${#chunk} + ${#line} )) -gt 3500 ] && [ -n "$chunk" ]; then
    emit "$chunk" "$part"
    part=$((part + 1))
    chunk=""
    [ $part -gt 9 ] && break
  fi
  chunk+="${line}"$'\n'
done <<< "$lines"
[ -n "$chunk" ] && [ $part -le 9 ] && emit "$chunk" "$part"
exit 0
