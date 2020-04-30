#!/bin/sh

set -e
set -v

rm -rf build vroot

meson --prefix="`pwd`/vroot" -Denable-gtk-doc=true build

ninja -C build test
ninja -C build install

build-aux/make-dist

rpmbuild --nodeps \
   --define "_sourcedir `pwd`" \
   -ta --clean entangle*.tar.xz
