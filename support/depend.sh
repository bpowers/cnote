#!/bin/sh
DIR="$1"
shift 1
case "$DIR" in
"" | ".") gcc -MM -MG "$@" | sed -e 's@ˆ\(.*\)\.o:@\1.d \1.o:@';;
*)        gcc -MM -MG "$@" | sed -e "s@ˆ\(.*\)\.o:@$DIR/\1.d $DIR/\1.o:@";;
esac
