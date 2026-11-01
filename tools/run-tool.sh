#!/bin/sh
# Run a tool of the project from the system, preferring a versioned one.
#   tools/run-tool.sh clang-format --dry-run --Werror file.cc
# The variable CLANG_FORMAT (or CLANG_TIDY) wins; otherwise the plain name is
# used, and when it does not exist the newest name-N.

set -e

name=$1
shift

var=$(printf '%s' "$name" | tr 'a-z-' 'A-Z_')
eval "tool=\${$var:-}"

if [ -z "$tool" ]; then
    if command -v "$name" >/dev/null 2>&1; then
        tool=$name
    else
        tool=$("$(dirname "$0")/newest.sh" /usr/bin/"$name"-[0-9]* \
            /usr/local/bin/"$name"-[0-9]*)
    fi
fi

if [ -z "$tool" ]; then
    echo "$name not found, set $var" >&2
    exit 1
fi

exec "$tool" "$@"
