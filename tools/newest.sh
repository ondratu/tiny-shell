#!/bin/sh
# Print the file with the highest version number in the suffix ("clang++-19"
# beats "clang++-9") or nothing. It is a portable replacement of "sort -V".
for file in "$@"; do
    [ -e "$file" ] && echo "${#file} $file"
done | sort | tail -n 1 | cut -d' ' -f2-
