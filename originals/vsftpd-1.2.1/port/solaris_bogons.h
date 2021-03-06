#ifndef VSF_SOLARIS_BOGONS_H
#define VSF_SOLARIS_BOGONS_H

/* This bogon ensures we get access to CMSG_DATA, CMSG_FIRSTHDR */
#define _XPG4_2

/* This bogon prevents _XPG4_2 breaking the include of signal.h! */
#define __EXTENSIONS__

/* Need dirfd() */
#include "dirfd_extras.h"

#endif /* VSF_SOLARIS_BOGONS_H */

