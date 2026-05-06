// port/direct.h — replacement for Win32 <direct.h>
//
// MUSHclient sources use _chdir / _mkdir / _getcwd / _rmdir from
// <direct.h>. Map to POSIX equivalents in <unistd.h> + <sys/stat.h>.

#ifndef MUSHCLIENT_PORT_DIRECT_H
#define MUSHCLIENT_PORT_DIRECT_H

#include <unistd.h>
#include <sys/stat.h>

#define _chdir   chdir
#define _getcwd  getcwd
#define _rmdir   rmdir

// _mkdir on Win32 takes a single path argument; POSIX mkdir takes
// path + mode. Provide a thin wrapper at file scope.
static inline int _mkdir(const char * path) {
    return mkdir(path, 0755);
}

#endif
