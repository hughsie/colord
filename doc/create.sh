../client/colormgr create-device t61xrandr normal
../client/colormgr create-profile t61xrandr-default normal
../client/colormgr create-profile profile-victorian temp
../client/colormgr device-add-profile /org/freedesktop/ColorManager/t61xrandr /org/freedesktop/ColorManager/t61xrandr_default
../client/colormgr device-set-property /org/freedesktop/ColorManager/t61xrandr Model "Cray 3000"
../client/colormgr device-set-property /org/freedesktop/ColorManager/t61xrandr Kind "printer"
../client/colormgr profile-set-filename /org/freedesktop/ColorManager/t61xrandr_default /home/hughsie/.color/icc/ibm-t61.icc
../client/colormgr profile-set-qualifier /org/freedesktop/ColorManager/t61xrandr_default "RGB.Plain.300dpi"
