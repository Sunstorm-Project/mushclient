// port/stdafx.h — replacement precompiled header for the SPARC port.
//
// MUSHclient sources begin with `#include "stdafx.h"`. The original
// header pulls in <afx.h> (the MFC umbrella) plus DirectSound,
// mmsystem, and resource.h. None of those are usable on Solaris 7;
// this version pulls in the MFC-compatibility shim instead, plus the
// minimum standard-library headers the codebase typically expects to
// be already-included after stdafx.

#ifndef MUSHCLIENT_PORT_STDAFX_H
#define MUSHCLIENT_PORT_STDAFX_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <ctime>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <set>
#include <algorithm>

#include "mfc_shim.h"

// MUSHclient defines #define new DEBUG_NEW after stdafx.h in many .cpp
// files. Keep DEBUG_NEW = new so that compiles cleanly.
#ifndef DEBUG_NEW
#define DEBUG_NEW new
#endif

// MUSHclient sprinkles BASED_CODE for old MS Windows segment hints —
// no-op on modern compilers.
#ifndef BASED_CODE
#define BASED_CODE
#endif

#endif
