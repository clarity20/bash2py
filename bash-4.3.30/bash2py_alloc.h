#if !defined (BASH2PY_ALLOC_H)
#  define BASH2PY_ALLOC_H

#  ifdef BASH2PY

#    if 1	/* disable xmalloc since bug in burp.c usage */

#      include "stdc.h"
#      include "bashansi.h"

#      define xmalloc malloc
#      define xrealloc realloc
#      define xfree free

#  endif /* disable */

# endif /* BASH2PY */

#endif	/* BASH2PY_ALLOC_H */

