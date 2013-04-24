#
#  bash completion support for colormgr console commands.
#
#  Copyright (C) 2012-2013 Richard Hughes <richard@hughsie.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#  02110-1301  USA


__colormgr_commandlist="
    create-device
    create-profile
    delete-device
    delete-profile
    device-add-profile
    device-get-default-profile
    device-get-profile-for-qualifier
    device-inhibit
    device-make-profile-default
    device-set-enabled
    device-set-kind
    device-set-model
    device-set-serial
    device-set-vendor
    find-device
    find-device-by-property
    find-profile
    find-profile-by-filename
    get-devices
    get-devices-by-kind
    get-profiles
    get-sensor-reading
    get-sensors
    get-standard-space
    profile-set-property
    sensor-lock
    sensor-set-options
    "

__colormgrcomp ()
{
	local all c s=$'\n' IFS=' '$'\t'$'\n'
	local cur="${COMP_WORDS[COMP_CWORD]}"
	if [ $# -gt 2 ]; then
		cur="$3"
	fi
	for c in $1; do
		case "$c$4" in
		*.)    all="$all$c$4$s" ;;
		*)     all="$all$c$4 $s" ;;
		esac
	done
	IFS=$s
	COMPREPLY=($(compgen -P "$2" -W "$all" -- "$cur"))
	return
}

_colormgr ()
{
	local i c=1 command

	while [ $c -lt $COMP_CWORD ]; do
		i="${COMP_WORDS[c]}"
		case "$i" in
		--help|--verbose|-v|-h|-?) ;;
		*) command="$i"; break ;;
		esac
		c=$((++c))
	done

    if [ $c -eq $COMP_CWORD -a -z "$command" ]; then
		case "${COMP_WORDS[COMP_CWORD]}" in
		--*=*) COMPREPLY=() ;;
		--*)   __colormgrcomp "
			--verbose
			--help
			"
			;;
        -*) __colormgrcomp "
            -v
            -h
            -?
            "
            ;;
		*)     __colormgrcomp "$__colormgr_commandlist" ;;
		esac
		return
	fi

	case "$command" in
	*)           COMPREPLY=() ;;
	esac
}

complete -o default -o nospace -F _colormgr colormgr
