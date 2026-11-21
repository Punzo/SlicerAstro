#!/bin/bash
# Virtual X displays of the GUI tests (CTest fixture SlicerAstroDisplays): each
# test runs on its own display, so that the tests run in parallel without
# taking the windows, the focus or the input events of each other.
#   SlicerAstroTestDisplays.sh start <Xvfb> <first display> <count> <state dir>
#   SlicerAstroTestDisplays.sh stop <state dir>
set -u
action=$1
if [ "$action" = "start" ]; then
  xvfb=$2; first=$3; count=$4; state=$5
  mkdir -p "$state"
  # displays of an interrupted run
  "$0" stop "$state"
  for ((i = 0; i < count; i++)); do
    display=$((first + i))
    if [ -e "/tmp/.X${display}-lock" ]; then
      echo "Display :${display} is in use (/tmp/.X${display}-lock): choose another SLICERASTRO_TEST_FIRST_DISPLAY" >&2
      exit 1
    fi
    # detached (CTest waits for the output of the processes of a test)
    setsid "$xvfb" ":${display}" -screen 0 1920x1200x24 -nolisten tcp </dev/null >/dev/null 2>&1 &
    echo $! >> "$state/pids"
  done
  for ((i = 0; i < count; i++)); do
    display=$((first + i))
    for ((wait = 0; wait < 100; wait++)); do
      [ -S "/tmp/.X11-unix/X${display}" ] && break
      sleep 0.1
    done
    if [ ! -S "/tmp/.X11-unix/X${display}" ]; then
      echo "Display :${display} did not start" >&2
      exit 1
    fi
  done
  echo "Started ${count} test displays from :${first}"
elif [ "$action" = "stop" ]; then
  state=$2
  if [ -f "$state/pids" ]; then
    while read -r pid; do
      # only the Xvfb started here (the number may be reused meanwhile)
      if [ "$(cat "/proc/${pid}/comm" 2>/dev/null)" = "Xvfb" ]; then
        kill "$pid" 2>/dev/null
      fi
    done < "$state/pids"
    rm -f "$state/pids"
  fi
else
  echo "usage: $0 start <Xvfb> <first display> <count> <state dir> | stop <state dir>" >&2
  exit 2
fi
