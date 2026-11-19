#!/bin/sh
# Builds and tests tiny-shell inside a BSD virtual machine of the CI
# (.github/workflows/bsd.yml); the argument is freebsd, openbsd or netbsd.
set -u
os=${1:?usage: ci-bsd.sh freebsd|openbsd|netbsd}

as_root=sudo
command -v doas > /dev/null 2>&1 && as_root=doas

case $os in
freebsd)
    $as_root pkg install -y gmake pkgconf googletest xorg-vfbserver xauth \
        libX11 libXft libXrender libXext libXrandr libXinerama fontconfig dejavu
    export PKG_CONFIG_PATH=/usr/local/libdata/pkgconfig:/usr/local/lib/pkgconfig
    make=gmake
    ;;
openbsd)
    # X11 (with Xft and Xvfb) is a part of the base system
    $as_root pkg_add gmake pkgconf googletest
    export PATH=/usr/X11R6/bin:"$PATH"
    export PKG_CONFIG_PATH=/usr/X11R6/lib/pkgconfig:/usr/local/lib/pkgconfig
    make=gmake
    ;;
netbsd)
    # X11 (with Xft and Xvfb) is a part of the base system, the rest comes
    # from pkgsrc
    export PATH=/usr/pkg/bin:/usr/pkg/sbin:/usr/X11R7/bin:"$PATH"
    export PKG_CONFIG_PATH=/usr/X11R7/lib/pkgconfig:/usr/pkg/lib/pkgconfig
    $as_root pkgin -y install gmake pkgconf googletest
    make=gmake
    ;;
*)
    echo "unknown system: $os" >&2
    exit 2
    ;;
esac

# in the background with redirected output, otherwise the SSH session of the
# VM never ends
Xvfb :99 -screen 0 1280x1024x24 > /dev/null 2>&1 &
xvfb=$!
sleep 2
rc=0
DISPLAY=:99 STRICT=${STRICT:-1} "$make" XVFB_RUN= all check examples || rc=$?
kill $xvfb
exit $rc
