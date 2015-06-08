#!/bin/sh

VERSION=1.0.0
SIMPLEAMD_DIR=`pwd`

if [ -z "$RPMBUILD_DIR" ]; then
  RPMBUILD_DIR=$HOME/rpmbuild
fi

mkdir -p $RPMBUILD_DIR
pushd $RPMBUILD_DIR
(mkdir -p SOURCES BUILD BUILDROOT RPMS SRPMS SPECS)
popd

tar czf $RPMBUILD_DIR/SOURCES/simpleamd-$VERSION.tar.gz -C $SIMPLEAMD_DIR .
cp rpm/simpleamd.spec $RPMBUILD_DIR/SPECS

