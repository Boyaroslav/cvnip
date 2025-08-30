#!/bin/bash
export DISPLAY=:0
export WAYLAND_DISPLAY=wayland-0
export XAUTHORITY=/home/$USER/.Xauthority 
/home/$USER/.local/bin/cvnip/cvdaemon &