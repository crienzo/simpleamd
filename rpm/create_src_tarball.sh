#!/bin/sh -e

SIMPLEAMD_DIR=${0%/*}/..
VERSION=$(sed -r -n "s/^Version: (.*)/\1/p" "$SIMPLEAMD_DIR"/rpm/simpleamd.spec)
: ${RPMBUILD_DIR:=$HOME/rpmbuild}

mkdir -p "$RPMBUILD_DIR"/{SOURCES,BUILD,BUILDROOT,RPMS,SRPMS,SPECS}
tar czf "$RPMBUILD_DIR/SOURCES/simpleamd-$VERSION.tar.gz" -C "$SIMPLEAMD_DIR" .
cp "$SIMPLEAMD_DIR"/rpm/simpleamd.spec "$RPMBUILD_DIR"/SPECS
