// stdafx.h — port-aware precompiled header for the SPARC Solaris 7 port
// of nickgammon/mushclient.
//
// The original Windows-only contents are preserved at
// port/stdafx.h.windows-original for reference. On the SPARC port we
// replace the MFC umbrella (#include <afx.h>) and DirectSound headers
// with port/mfc_shim.h, which provides std::string-backed CString,
// CTime/CRect/CPoint/CSize POD wrappers, and stub diagnostic macros.
//
// The GUI-touching .cpp files (*View.cpp, *Doc.cpp, MDI*.cpp, dialogs/,
// MUSHclient.cpp boot path) are NOT compiled on this port — they get
// rewritten on the wxWidgets shell. Everything else (scripting/, mxp/,
// names/, Replace.cpp, Color.cpp, doc.cpp's non-GUI portions, plugins)
// reaches into MFC only via the shim.

#pragma once

#include "port/mfc_shim.h"

// Standard library headers that downstream MUSHclient .cpp files
// historically expected to be transitively-included via <afx.h>.
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
#include <utility>

// MUSHclient sprinkles `#define new DEBUG_NEW` after stdafx.h. Keep
// DEBUG_NEW = new so that compiles cleanly.
#ifndef DEBUG_NEW
#define DEBUG_NEW new
#endif

// MUSHclient sprinkles BASED_CODE for old MS Windows segment hints —
// no-op on modern compilers.
#ifndef BASED_CODE
#define BASED_CODE
#endif

// PCRE expects HAVE_CONFIG_H (the original stdafx.h set it). Keep
// the same toggle so the vendored pcre/ subdir compiles unmodified.
#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif
