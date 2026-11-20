#!/bin/sh
# Builds and runs the headless UI simulator (Linux, no Win32 needed).
# Screenshots + PASS/FAIL click-test results land in sim/shots/.
set -e
cd "$(dirname "$0")/.."
mkdir -p sim/shots
g++ -std=c++17 -O1 -Wall -Wno-unused-variable \
    -I src -I third_party/imgui -I third_party/imgui/backends \
    sim/sim_main.cpp src/ui.cpp \
    third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp \
    third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp \
    -o sim/sim
./sim sim/shots
