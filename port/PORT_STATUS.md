# SPARC Solaris 7 port of nickgammon/mushclient — status

Started 2026-05-05. Goal: a wxWidgets/X11 build of MUSHclient that runs on
SPARC Solaris 7 (skywarp / local-dev QEMU SS-4) using the SST cross-build
pipeline.

## Where to start tomorrow

1. Read this doc top to bottom.
2. Look at [mfc_shim.h](mfc_shim.h) — that's the substrate the port is built on.
3. Look at [shim_smoke.cpp](shim_smoke.cpp) — that's the test that proves the
   shim compiles + behaves correctly on Mac (native) and SPARC (cross).
4. Run the smoke (Mac):
   ```
   cd ~/Documents/Projects/source/mushclient
   g++ -std=c++17 -Wall -Wextra port/shim_smoke.cpp -o /tmp/shim_smoke && /tmp/shim_smoke
   ```
5. Cross-compile the smoke for SPARC:
   ```
   cd ~/Documents/Projects/source/sparc-build-host
   docker compose -f cross-build-userland/docker-compose.yml \
     -f local-dev/compose.override.yml run --rm \
     --entrypoint /bin/bash \
     -v /Users/julianwolfe/Documents/Projects/source/mushclient:/opt/mushclient:ro \
     userland-build -c '
       source /opt/cross-build-userland/common.sh
       PACKAGES_DIR=/opt/cross-build-userland/packages
       prepare_sysroot >/dev/null 2>&1
       ensure_staged libsolcompat libgcc libstdcxx >/dev/null 2>&1
       ${CXX} ${COMMON_CPPFLAGS} ${COMMON_CXXFLAGS:-${COMMON_CFLAGS}} \
         -std=c++17 -Wall --sysroot=${SYSROOT} \
         /opt/mushclient/port/shim_smoke.cpp \
         ${COMMON_LDFLAGS} ${COMMON_LIBS} -lsolcompat -Wl,--disable-new-dtags \
         -o /opt/output/shim_smoke
       file /opt/output/shim_smoke'
   ```
   Both produce a working binary today.

## What's done

* **Cloned** nickgammon/mushclient at this depth-50 snapshot
  (~99 MB, 529 .cpp/.h files, 55 top-level .cpp files, 100k+ LOC).
* **Inventoried MFC usage** (top-level .cpp only):
  * `CString` — 1277 references; methods used: `IsEmpty` (245),
    `GetLength` (133), `Left` (87), `Replace` (71), `Mid` (61), `Format` (57),
    `CompareNoCase` (52), `Find` (36), `GetBuffer` (30), `ReleaseBuffer` (28),
    `TrimLeft` (21), `MakeLower` (21), `TrimRight` (20), `Right` (18),
    `MakeUpper` (5).
  * `CTime` (113) / `CTimeSpan` (13)
  * `CRect` (95) / `CPoint` (120) / `CSize` (20)
  * `CFile` (34) / `CStdioFile` (8) / `CArchive` (61)
  * `CMapStringToPtr` (8) / `CTypedPtrMap` (8) / `CTypedPtrList` (10)
  * `CObject` (34) / `CException` (32)
  * `COleDateTime` (16) / `COleVariant` (29) — drop (no WSH)
  * Macros: `ASSERT_VALID` (130), `ASSERT` (83), `VERIFY` (27), `TRACE/0/1/…` (97), `AfxMessageBox` (19)
* **Wrote [port/mfc_shim.h](mfc_shim.h)** (≈600 lines): non-GUI MFC compat
  layer with `CString`, `CTime`, `CTimeSpan`, `CRect`, `CPoint`, `CSize`,
  `CObject`, `CException`, `CFile`, `CStdioFile`, `CMapStringToPtr`/etc.,
  `CList`, plus the diagnostic-macro suite. `CArchive` is deliberately
  `delete`d (`= delete` constructor) so any caller forces a TODO during
  port — MFC's binary serialization gets rewritten as JSON or flat I/O.
* **Wrote [port/shim_smoke.cpp](shim_smoke.cpp)**: exercises the shim end
  to end. Passes natively on macOS arm64 and cross-compiles to a 4.7 KB
  SPARC ELF that links clean against `libsolcompat` + `libstdc++`.
* **Replaced [stdafx.h](../stdafx.h) in-place** with a port-aware version.
  Original Windows contents preserved at
  [port/stdafx.h.windows-original](stdafx.h.windows-original). The new
  stdafx pulls in `port/mfc_shim.h` plus a small standard-library set;
  no `<afx.h>`, no DirectSound, no Win32 headers.
* **Compile-validated [Replace.cpp](../Replace.cpp)** against the shim.
  Cross-compiles clean to a SPARC `.o` (4716 bytes).

## File-by-file disposition (top-level .cpp files)

DELETE — pure GUI / Windows-specific. Replace with wxWidgets equivalents:

* `ActivityDoc.cpp`, `ActivityView.cpp` (CDocument / CView)
* `TextDocument.cpp`, `TextView.cpp` (CTextDocument / CTextView)
* `MDIClientWnd.cpp`, `MDITabs.cpp` (MFC MDI)
* `MUSHclient.cpp` (CWinApp boot)
* `MakeWindowTransparent.cpp` (Win32 layered windows)
* `MySplitterWnd.cpp`, `MyStatusBar.cpp` (MFC controls)
* `TimerWnd.cpp` (CWnd timer host)
* `accelerators.cpp` (MFC accelerator-table loader)
* `activitychildfrm.cpp`, `childfrm.cpp`, `textchildfrm.cpp` (CMDIChildWnd)
* `art.cpp` (toolbar bitmaps)
* `mainfrm.cpp` (CMainFrame)
* `mushview.cpp`, `sendvw.cpp` (CMUSHView, CSendView — main output + input panes)
* `miniwindow.cpp` (popup window)
* `paneline.cpp` (splitter separator)
* `HyprLink.cpp`, `StatLink.cpp` (clickable-text controls)
* `DDV_validation.cpp` (MFC dialog data validators)
* `genprint.cpp` (CView print preview)
* `winplace.cpp` (WINDOWPLACEMENT save/restore)
* `globalregistryoptions.cpp` (Windows registry — replace with wxConfig / file)

KEEP + SHIM — non-GUI logic. Compiles against `port/mfc_shim.h`:

* `Color.cpp` (HSL ↔ RGB)
* `Dmetaph.cpp` (Double Metaphone fuzzy match)
* `Finding.cpp` (string utilities)
* `Line.cpp` (text-line data)
* `Mapping.cpp` (auto-mapper logic — verify no GUI hooks)
* `NameGeneration.cpp` (random name generator) — needs `MUSHclient.h`
  edit to fix `scripting\scripting.h` backslash path
* `ProcessPreviousLine.cpp`
* `Replace.cpp` ✓ already cross-compiles
* `Utilities.cpp` (misc helpers — likely needs heavy reading)
* `ansi.cpp` (ANSI/VT100 colour parsing)
* `mcdatetime.cpp` (MUSHclient time helpers)
* `regexp.cpp` (PCRE wrapper)
* `evaluate.cpp` (expression evaluator)
* `exceptions.cpp` (error reporting)
* `format.cpp` (output formatting)
* `scriptingoptions.cpp` (scripting config)
* `telnet_phases.cpp` (telnet IAC state machine)
* `timers.cpp` (timer scheduling logic — strip CWnd hooks)
* `world_debug.cpp` (`/debug` commands)
* `stdafx.cpp` (PCH trigger — empty .cpp)

KEEP + REWRITE NETWORK LAYER — currently MFC `CSocket`/`CAsyncSocket`,
needs swap to wxSocket or POSIX BSD sockets:

* `UDPsocket.cpp` (UDP for MUSH server discovery)
* `chatlistensock.cpp`, `chatsock.cpp` (MUD chat protocol)
* `worldsock.cpp` (main MUD TCP socket)

PARTIAL REWRITE — heavy mix of GUI and logic; needs splitting:

* `doc.cpp`, `doc_construct.cpp` — `CMUSHclientDoc` is the central
  state container *and* a `CDocument`. The non-GUI fields (triggers,
  aliases, timers, plugins, output buffer) move into a plain class;
  the `CDocument` parts move to a wxWidgets-side adapter.
* `plugins.cpp` — uses `IDispatch` for plugin VBScript/JScript. Drop
  WSH; keep Lua-only plugins; rewrite with wxLua or direct lua.
* `serialize.cpp` — uses `CArchive`. Convert to JSON (already have
  a JSON dep via the vendored `lsqlite/` adjacency? — verify).

NOT YET CLASSIFIED:

* `dialogs/*.cpp` — large directory of CDialog-derived classes. All
  delete + replace with wxDialog.
* `mxp/*.cpp` — MUD eXtension Protocol parser. Probably KEEP + SHIM.
* `names/*.cpp` — name-list utilities. Probably KEEP + SHIM.
* `scripting/*.cpp` — Lua/Python/TCL/PHP plugin glue. Drop WSH paths,
  keep Lua, see if Python/TCL/PHP build out-of-the-box on Solaris 7.
* `spell/*.cpp` — spellcheck. Likely DELETE (use hunspell directly
  if needed; the scripting/ directory may already integrate it).

## Compile pass / fail tally (top-level KEEP+SHIM .cpp files, as of 2026-05-05 16:30)

Shim covers: CString (full), CTime/CTimeSpan, CRect/CPoint/CSize,
CObject/CException/CFileException/CMemoryException/CSystemException
(forward decl), CFile/CStdioFile, CMapBase/CList, CWinApp/CCmdTarget/CWnd
/CView/CDocument/CDialog/CFrameWnd/CMDIChildWnd/CMDIFrameWnd/CCmdUI
(empty bases for inheritance), MSG/POINT/RECT/SIZE/LARGE_INTEGER PODs,
CSocket/CAsyncSocket (empty), CCriticalSection (empty Lock/Unlock),
CRuntimeClass (empty), CFormat (forward decl).

Plus: process.h shim → unistd.h, `_getpid` → `getpid`.

PASSING (3): Color.cpp, Dmetaph.cpp, Replace.cpp.

FAILING — categorized by shim gap:

* **CMultiDocTemplate undefined** in MUSHclient.h: ansi.cpp, Finding.cpp,
  NameGeneration.cpp, ProcessPreviousLine.cpp, Line.cpp, evaluate.cpp,
  regexp.cpp, telnet_phases.cpp, timers.cpp, world_debug.cpp, serialize.cpp.
  Fix: add `class CMultiDocTemplate {};` to the shim.

* **CFormat incomplete in format.cpp**: format.cpp's `CFormat::CFormat(...)`
  defines methods on a class that's only forward-declared in the shim.
  Fix: `#include "format.h"` first, OR move CFormat definition into the
  shim (it's just a CString-derived varargs ctor anyway, see prior commit
  attempt).

* **CSystemException incomplete in exceptions.cpp**: same pattern. Fix: same.

* **<direct.h> missing** in Utilities.cpp:3339. Fix: shim that maps to
  unistd.h/sys/stat.h. Most uses are `_chdir`, `_mkdir`, `_getcwd` →
  `chdir`, `mkdir`, `getcwd`.

* **COleDateTimeSpan missing** in mcdatetime.cpp:96. Fix: alias to
  `CmcDateTimeSpan` (since COleDateTime semantics are the same kind
  of double-encoded date).

* **mcdatetime.cpp also needed `#include "mcdatetime.h"`** explicitly —
  fixed in-place.

The pattern is clear: each compile pass surfaces ~3-5 missing types.
Add them to the shim, retry, repeat. Estimated 2-4 more iteration
rounds to clear the top-level KEEP+SHIM list of 18 files. Then on to
scripting/, mxp/, names/, plugins.cpp, doc.cpp, doc_construct.cpp.

## Known port issues already surfaced

1. **Backslash include paths in MUSHclient.h**:
   `#include "scripting\scripting.h"` won't resolve on Linux. Sed-rewrite
   `\\` → `/` in all top-level .h/.cpp files as a one-shot port step.

2. **MUSHclient.h's `#error include 'stdafx.h' before this`**: files that
   transitively include MUSHclient.h without first pulling stdafx.h fail.
   Add `-include port/stdafx.h` to the port Makefile's CFLAGS.

3. **CArchive serialization**: 61 hits. Replace with JSON (cJSON or
   nlohmann::json from a header-only drop) before the port can read or
   write its world files. Tracked as `CArchive = delete` in the shim so
   call sites flag at compile time.

4. **CRegistryKey-style `globalregistryoptions.cpp`**: Windows registry
   isn't on Solaris. Either delete (most options are command-line/file-driven
   anyway) or replace with `wxConfigBase` / a flat `~/.config/mushclient.ini`.

5. **DirectSound**: `MUSHclient.h` defines `DIRECTSOUND_VERSION 5` and
   includes `<dsound.h>`. The audio playback paths use IDirectSound
   buffers. Replace with SDL2_mixer (already in the SST pipeline as
   `SSTsdm`). Estimate: ~200-400 LOC across `MUSHclient.cpp` (the
   `m_pDS`/`m_lpDirectSound`/`MAX_SOUND_BUFFERS` machinery).

6. **WSH (VBScript/JScript)**: `hostsite.h` ships `IActiveScriptSite` /
   `IActiveScriptSiteWindow`. Drop the entire COM scripting layer; keep
   the Lua glue under `scripting/`. About 1500 LOC across
   `hostsite.h`, `plugins.cpp`'s COM code paths, `MUSHclient.cpp`'s
   ATL/COM init.

7. **CCriticalSection / CWinThread**: not yet inventoried; if
   present in non-GUI files, shim with `std::mutex` / `std::thread`.

## Suggested next moves

1. **Run the file-classification properly**: walk every top-level .cpp,
   open it, mark KEEP / SHIM / DELETE / REWRITE; write the result back to
   this doc. ~3-4 hours.
2. **Patch backslash paths**: one shell pass over all MUSHclient.{h,cpp}
   to convert `scripting\` → `scripting/` etc. Trivial.
3. **Build infrastructure**: write `port/Makefile` (or a CMakeLists.txt)
   that compiles the KEEP+SHIM list into a static library `libmushcore.a`,
   ready for the wxWidgets shell to link against.
4. **Compile the KEEP+SHIM list** one file at a time, fixing shim gaps as
   they surface. This is where the bulk of the 1-2 weeks for the MFC shim
   layer lives.
5. **Strip `CArchive` call sites** by replacing serialize.cpp with a
   nlohmann::json-based world-file reader/writer. Probably a few days.
6. **Begin the wxWidgets shell** in `port/wx/`: wxApp + wxFrame + a
   custom-drawn output pane. The pickup prompt at the top of this thread
   has the strawman.
7. **Drop `dialogs/`** as a unit — that directory's whole purpose was MFC
   CDialog subclasses. Replace post-hoc as the wxWidgets shell needs each
   dialog (preferences, find, world properties, …).

## Build-pipeline integration plan

The port doesn't yet have a `cross-build-userland/packages/mushclient/`
directory in sst-build-pipeline. Sketch:

```
packages/mushclient/
  build.sh        # download nickgammon/mushclient, apply port patches, compile
  pkginfo         # SSTmush — wxWidgets-X11 MUD client
  traits          # expected_outputs=binary,data; threads_self_managed=yes
  depend          # SSTwxwd, SSTlua51, SSTossl, SSTsdm (SDL2_mixer),
                  # SSTsqlit, plus transitives
```

`build.sh` will pull from the Sunstorm-Project fork (when we make it)
rather than upstream nickgammon, to keep the port tree under our control.

## Org / repo strategy (open question)

The mushclient checkout under
`~/Documents/Projects/source/mushclient` is a clone of nickgammon's repo.
Once the port has bones (this commit), fork to
`Sunstorm-Project/mushclient` and push from there. The port branch
naming convention from sst-build-pipeline (`feat/qt6-mudlet`, etc.) would
suggest `feat/sparc-port` or just `main` on the fork.

## Estimate

From the original pickup prompt:

* MFC shim layer: 1-2 weeks (in progress; the easy parts are done, but
  CArchive/registry/socket conversions are still ahead).
* Gut the GUI: 1 week (delete the 22 GUI .cpp files + dialogs/, chase
  linker errors).
* wxWidgets shell: 2-3 weeks (output pane is the hard part; everything
  else is straightforward wx widgetry).
* Audio rewrite (DirectSound → SDL2_mixer): 3-5 days.
* Drop WSH + verify Lua scripting still works: 1 week.

Net: 5-6 months at full focus, matches the pickup prompt's estimate.
