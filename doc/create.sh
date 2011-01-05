./colormgr create-device t61xrandr normal
./colormgr create-profile t61xrandr-default normal
./colormgr create-profile profile-victorian temp
./colormgr device-add-profile /org/freedesktop/ColorManager/t61xrandr /org/freedesktop/ColorManager/t61xrandr_default
./colormgr profile-set-filename /org/freedesktop/ColorManager/t61xrandr_default /home/hughsie/.color/icc/ibm-t61.icc
./colormgr profile-set-qualifier /org/freedesktop/ColorManager/t61xrandr_default "RGB.Plain.300dpi"
