#!/bin/sh
#
# Compiler wrapper used by the AROS boost port when driving b2.
#
# b2's gcc toolset passes -pthread to the compiler for multi threaded
# targets (some libraries, e.g. iostreams, force <threading>multi).  The
# AROS gcc has no -pthread option - pthread is an ordinary link library
# there - so drop it and hand everything else to the real cross compiler,
# which is taken from $AROS_B2_CXX (may include a launcher such as ccache).
#
n=$#
i=0
while [ $i -lt $n ]; do
    a="$1"
    shift
    [ "$a" != "-pthread" ] && set -- "$@" "$a"
    i=$((i + 1))
done
exec ${AROS_B2_CXX:-g++} "$@"
