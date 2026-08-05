/*
 * The library has both libvncserver/sockets.c and common/sockets.c; the
 * build system flattens object names by basename, so the two would
 * collide (and vpath can even resolve the wrong source). Both are
 * therefore compiled through uniquely-named wrappers using
 * directory-prefixed includes - the ports source directory root is on
 * the include path.
 */
#include "common/sockets.c"
