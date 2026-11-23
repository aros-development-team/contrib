"""Compile the real hyperlink allocator into retained-line C test fixtures."""
from pathlib import Path

def hyperlink_source():
    root=Path(__file__).resolve().parents[1]
    header=(root/'hyperlink.h').read_text().replace('#include <exec/types.h>','')
    source='\n'.join(line for line in (root/'hyperlink.c').read_text().splitlines() if not line.startswith('#include'))
    return '''\n#include <string.h>\n#include <stdlib.h>\ntypedef unsigned char UBYTE; typedef int BOOL;
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef MEMF_ANY
#define MEMF_ANY 0
#endif
#ifndef AllocMem
#define AllocMem(n,f) malloc(n)
#endif
#ifndef FreeMem
#define FreeMem(p,n) free(p)
#endif
'''+header+source
