#!/bin/sh

library_path="$1"
executable="$2"

if [ "$LD_LIBRARY_PATH" ]; then
    LD_LIBRARY_PATH="$library_path:$LD_LIBRARY_PATH"
fi

shift 2

exec "$executable" "$@"
