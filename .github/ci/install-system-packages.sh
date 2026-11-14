#!/bin/bash
# System packages to build 3D Slicer and SlicerAstro on Ubuntu 24.04 (the
# GitHub runners and the ubuntu:24.04 container), and Qt 6 with aqtinstall.
#   install-system-packages.sh <Qt install dir>
set -euo pipefail
SUDO=$([ "$(id -u)" = 0 ] && echo "" || echo sudo)
export DEBIAN_FRONTEND=noninteractive
$SUDO apt-get update -q
$SUDO apt-get install -y -q --no-install-recommends \
  build-essential cmake git ca-certificates ccache curl patch perl python3 python3-venv python3-pip \
  libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev libgl1-mesa-dri mesa-utils \
  libx11-dev libx11-xcb-dev libxext-dev libxrender-dev libxi-dev libxt-dev libxkbcommon-dev libxkbcommon-x11-0 \
  libxcb1-dev libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 \
  libxcb-shape0 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 libfontconfig1-dev libfreetype-dev libdbus-1-3 \
  libssl-dev libffi-dev libbz2-dev liblzma-dev libsqlite3-dev libreadline-dev zlib1g-dev uuid-dev \
  xvfb xauth
QT_ROOT=${1:-$HOME/Qt}
python3 -m venv "$HOME/aqt-venv"
"$HOME/aqt-venv/bin/pip" install -q aqtinstall==3.3.0
"$HOME/aqt-venv/bin/aqt" install-qt linux desktop 6.10.1 linux_gcc_64 -m qt5compat qtscxml --outputdir "$QT_ROOT"
echo "Qt6_DIR=$QT_ROOT/6.10.1/gcc_64/lib/cmake/Qt6"
