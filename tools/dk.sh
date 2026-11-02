#!/bin/sh
# Run one docker command in one of TWO shared slots (the user's limit: at most 2 containers at once, every
# session and helper included). Waits for a free slot; the slot is a directory made atomically by mkdir.
#   sh tools/dk.sh <max seconds> docker run --rm --label watchdog.max=<n> ... (the full command)
# The command must carry --rm and --label watchdog.max (~3x its expected time) and wrap its work in `timeout`.
L="/c/New folder/batrider360/work/.dockerslots"
mkdir -p "$L"
shift_max="$1"; shift
while :; do
	for s in 1 2; do
		if mkdir "$L/slot$s" 2>/dev/null; then
			# a slot older than 2 h was left by a killed run: take it over
			trap 'rmdir "$L/slot$s" 2>/dev/null' EXIT INT TERM
			if [ "$(docker ps -q | wc -l)" -ge 2 ]; then rmdir "$L/slot$s"; trap - EXIT INT TERM; break; fi
			"$@"; r=$?
			rmdir "$L/slot$s" 2>/dev/null; trap - EXIT INT TERM
			exit $r
		fi
	done
	find "$L" -maxdepth 1 -name 'slot*' -mmin +120 -exec rmdir {} \; 2>/dev/null
	sleep 15
done
