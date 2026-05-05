// port/process.h — replacement for Win32 <process.h>
//
// MUSHclient sources include <process.h> just for `_getpid()` and
// occasional `_spawn*` calls; map to POSIX. mfc_shim.h already
// `#define _getpid getpid` and includes <unistd.h>, so this header
// is a no-op shim that exists only so that `#include <process.h>`
// resolves on the include path.

#ifndef MUSHCLIENT_PORT_PROCESS_H
#define MUSHCLIENT_PORT_PROCESS_H

#include "mfc_shim.h"
#include <unistd.h>

#endif
