#!/bin/sh
# simple version number script loosely based on git-version-gen

v=`git describe --tags --abbrev=4 --match="v*" HEAD 2>/dev/null || echo "UNKNOWN"`
v=`echo "$v" | tr -d "vg" | tr "\-" "."`
printf %s "$v"
