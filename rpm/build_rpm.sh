#!/bin/sh -e

: ${RPMBUILD_DIR:=$HOME/rpmbuild}
exec rpmbuild --define "_topdir $RPMBUILD_DIR" \
	-ba "$RPMBUILD_DIR"/SPECS/simpleamd.spec
