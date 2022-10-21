#!/bin/sh
# simple version number script loosely based on git-version-gen

v=`git describe --abbrev=4 --match="v*" HEAD`
v=`echo "$v" | tr -d "vg" | tr "\-" "."`
printf %s "$v"
