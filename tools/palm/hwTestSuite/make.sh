#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR

APPNAME="TstSuite"

if [ "$1" = "clean" ]; then
   rm -rf *.o *.a $APPNAME $APPNAME-sections.s $APPNAME-sections.ld $APPNAME.prc *.bin
   exit
fi

declare -a FILES=("testSuite" "viewer" "tools" "tests" "cpu" "emuFunctions" "ugui" "undocumentedApis" "debug")
CFLAGS="-palmos4 -O3"

if [ "$1" = "debug" ]; then
   CFLAGS="$CFLAGS -DDEBUG -g"
fi

m68k-palmos-multigen $APPNAME.def
m68k-palmos-gcc $CFLAGS -c $APPNAME-sections.s -o $APPNAME-sections.o
for I in "${FILES[@]}"; do
   m68k-palmos-gcc $CFLAGS -c $I.c -o $I.o
done
m68k-palmos-gcc -o $APPNAME *.o $APPNAME-sections.ld

# if possible generate icon trees
if type "MakePalmIcon" &> /dev/null; then
   MakePalmIcon ./appIcon.svg ./
fi

pilrc $APPNAME.rcp
build-prc $APPNAME.def $APPNAME *.bin
