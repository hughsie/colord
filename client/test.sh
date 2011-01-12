./colormgr create-device dave normal
./colormgr get-devices
./colormgr create-profile norman normal
./colormgr create-profile victorian temp
./colormgr get-profiles
./colormgr device-add-profile /org/freedesktop/ColorManager/dave /org/freedesktop/ColorManager/norman
./colormgr find-device dave
./colormgr find-profile norman
./colormgr profile-set-property /org/freedesktop/ColorManager/norman Filename /home/hughsie/Code/colord/data/test.icc
./colormgr profile-set-property /org/freedesktop/ColorManager/norman Qualifier "RGB.Plain.300dpi"
./colormgr get-devices
./colormgr device-get-profile-for-qualifier /org/freedesktop/ColorManager/dave "RGB.*.300dpi"
./colormgr delete-device dave
./colormgr get-profiles
