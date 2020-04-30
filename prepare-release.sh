#!/bin/sh

set -e
set -v

rm -rf build vroot

meson --prefix="`pwd`/vroot" -Denable-gtk-doc=true build

ninja -C build dist

rpmbuild --nodeps \
   --define "_sourcedir `pwd`" \
   -ta --clean build/meson-dist/entangle*.tar.xz
