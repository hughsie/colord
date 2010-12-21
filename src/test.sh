./colormgr create-device dave
./colormgr get-devices
./colormgr create-profile norman
./colormgr get-profiles
./colormgr device-add-profile /org/freedesktop/ColorManager/dave /org/freedesktop/ColorManager/norman
./colormgr find-device dave
./colormgr find-profile norman
./colormgr profile-set-filename /org/freedesktop/ColorManager/norman /home/hughsie/Code/colord/data/test.icc
./colormgr profile-set-qualifier /org/freedesktop/ColorManager/norman "RGB.Plain.300dpi"
./colormgr get-devices
./colormgr device-get-profile-for-qualifier /org/freedesktop/ColorManager/dave "RGB.*.300dpi"
./colormgr delete-device dave
./colormgr get-profiles
