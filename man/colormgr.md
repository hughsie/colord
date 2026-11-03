% colormgr(1) {{PACKAGE_VERSION}} | colormgr man page

## NAME

**colormgr** — Color Manager Testing Tool

## SYNOPSIS

| **colormgr** [**\-\-verbose**] [**\-\-version**] [CMD]

## DESCRIPTION

This manual page documents briefly the **colormgr** command.

**colormgr** allows an administrator to view and change color profile to device mappings.

## OPTIONS

This program follows the usual GNU command line syntax, with long options starting with two dashes.
A summary of options is included below.

**\-\-help**: Show summary of options.

## COMMANDS

This program takes commands with a variable number of arguments.

**create-device** *device_id* *scope*: Create a device

**create-profile** *profile_id* *scope*: Create a profile

**delete-device** *device*: Deletes a device

**delete-profile** *profile*: Deletes a profile

**device-add-profile** *device* *profile*: Add a profile to a device that already exists

**device-get-default-profile** *device*: Gets the default profile for a device

**device-get-profile-for-qualifier** *device* *qualifier*: Returns all the profiles that match a qualifier

**device-inhibit** *device* *timeout*: Inhibits colour profiles for this device

**device-make-profile-default** *device* *profile*: Makes a profile default for a device

**device-set-enabled**: Enables or disables the device

**device-set-kind** *device* *kind*: Sets the device kind

**device-set-model** *device* *model*: Sets the device model

**device-set-serial** *device* *serial*: Sets the device serial

**device-set-vendor** *device* *vendor*: Sets the device vendor

**find-device** *device_id*: Find a device from the device ID

**find-device-by-property** *key* *value*: Find a device with a given property value

**find-profile** *profile_id*: Find a profile from the profile ID

**find-profile-by-filename** *filename*: Find a profile by filename

**get-devices**: Gets all the color managed devices

**get-devices-by-kind** *kind*: Gets all the color managed devices of a specific kind

**get-profiles**: Gets all the available color profiles

**get-sensor-reading** *display_type*: Gets a reading from a sensor

**get-sensors**: Gets all the available colour sensors

**get-standard-space** *standard_space*: Get a standard colourspace

**import-profile** *filename*: Copies an .icc file into the correct per-user directory and shows
information about the imported profile.

**profile-set-property** *profile* *name* *value*: Sets extra properties on the profile

**sensor-lock**: Locks the colour sensor

**sensor-set-options** *name* *value*: Sets one or more sensor options

## AUTHOR

This manual page was written by Richard Hughes <richard@hughsie.com>.
