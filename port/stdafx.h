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
#include <climits>
#include <cfloat>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

// MUSHclient sources use unqualified `deque<string>`, `string`, etc.
// They relied on `using namespace std;` at the top of the original
// stdafx.h. We pull the most-used names into the global namespace
// here rather than `using namespace std;` so we don't shadow names
// that the shim defines (CString, CTime, ...).
// MUSHclient code (Gammon) was written against MFC's stdafx.h umbrella
// which transitively did `using namespace std;` — every .cpp uses
// unqualified `string`, `vector`, `deque`, `ostringstream`, `boolalpha`,
// `count`, `find`, etc. Replicating each `using` individually is
// whack-a-mole; just import the namespace globally here so all the
// MFC sources compile 1:1. This shadows nothing in the shim because
// the shim's MFC types live in the global namespace as `class CString`,
// `class CTime`, etc — std doesn't have those names.
using namespace std;

#include "mfc_shim.h"

// MUSHclient's stdafx.h.windows-original line 168 includes
// "zlib\zlib.h"; on the port we use the system-installed zlib from
// the sysroot. doc.h has `z_stream m_zCompress` + `Bytef *` members.
#include <zlib.h>

// regexp.h gives us t_regexp (used by CFindInfo below) and
// MAX_WILDCARDS. It only depends on cstdint / standard headers, so
// it's safe to include this early.
#include "regexp.h"

// format.h defines `struct CFormat : CString` — used inline in
// exceptions.cpp's CSystemException::GetErrorMessage and a couple
// other files. Keep it pulled in always.
#include "format.h"

// MUSHclient sprinkles these MFC-flavoured bit-ops + collection-delete
// macros throughout the codebase. The original definitions live in the
// pre-port stdafx.h.windows-original; the bit ops are trivial; the
// list/map drains rely on shim collections (CList::IsEmpty/RemoveHead,
// CMapBase::m_map / RemoveAll).
#ifndef IS_SET
#define IS_SET(var, bit)        (((var) & (bit)) != 0)
#define SET_BIT(var, bit)       ((var) |= (bit))
#define REMOVE_BIT(var, bit)    ((var) &= ~(bit))
#define TOGGLE_BIT(var, bit)    ((var) ^= (bit))
#endif

#ifndef DELETE_LIST
#define DELETE_LIST(listname)                       \
    do {                                            \
        while (!(listname).IsEmpty())               \
            delete (listname).RemoveHead();         \
    } while (false)
#endif

// MFC's DELETE_MAP copies into an intermediate list before deleting,
// because POSITION-iteration breaks under modification. std::map
// doesn't have that constraint — iterate, delete, then RemoveAll().
#ifndef DELETE_MAP
#define DELETE_MAP(mapname, pointertype)            \
    do {                                            \
        for (auto & __kv : (mapname).m_map)         \
            delete static_cast<pointertype *>(__kv.second); \
        (mapname).RemoveAll();                      \
    } while (false)
#endif

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

// ─────────────────────────────────────────────────────────────────────
// Constants migrated from stdafx.h.windows-original.
// Mostly preprocessor macros + a couple of trivial utility functions.
// MUSHclient sources reference these without an explicit include
// (they relied on stdafx.h being PCH-included as the very first
// header). Port them verbatim here, dropping anything that needs
// MFC types (DDV/DDX dialog data exchange, CFindInfo/CProgressDlg).
// ─────────────────────────────────────────────────────────────────────

#define SAMECOLOUR              65535
#define NO_COLOUR               0xFFFFFFFF      // for COLORREF
#define NOSOUNDLIT              "(No sound)"

// helper define for appending an "s" / "ies" / "es" to plural amounts
#define PLURAL(arg)   (arg), (arg) == 1 ? ""  : "s"
#define PLURALIE(arg) (arg), (arg) == 1 ? "y" : "ies"
#define PLURALES(arg) (arg), (arg) == 1 ? ""  : "es"

#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

#define DEFAULT_CHAT_PORT       4050
#define DEFAULT_CHAT_NAME       "Name-not-set"

#define MAX_CUSTOM              16   // maximum custom colours at present
#define OTHER_CUSTOM            16   // colour for triggers which represents 'other'

// Bit manipulations on bit-NUMBERS (vs. our IS_SET above which take masks).
#define CHECK_BIT_NUMBER(flag, bit)  (((flag) & (1 << (bit))) != 0)
#define SET_BIT_NUMBER(flag, bit)    ((flag) |= (1 << (bit)))
#define CLEAR_BIT_NUMBER(flag, bit)  ((flag) &= ~(1 << (bit)))
#define TOGGLE_BIT_NUMBER(flag, bit) ((flag) ^= (1 << (bit)))

// SQLite forward decls — the SPARC port doesn't yet ship a usable
// libsqlite, but doc.h declares `sqlite3 * db;` so the type name has
// to exist. Real sqlite3 will plug in when the prefs DB lands.
struct sqlite3;
struct sqlite3_stmt;

// DirectSound — Windows-only sound API. doc.h has
// `LPDIRECTSOUNDBUFFER m_pDirectSoundSecondaryBuffer[MAX_SOUND_BUFFERS]`.
// We can't link DirectSound on Solaris 7; the long-term plan is
// SDL2_mixer or libao. For now, define LPDIRECTSOUNDBUFFER as an
// opaque pointer so the array compiles. Code paths that actually
// touch m_pDirectSoundSecondaryBuffer get conditioned out at port
// time.
struct IDirectSoundBuffer;
typedef IDirectSoundBuffer * LPDIRECTSOUNDBUFFER;
struct IDirectSound;
typedef IDirectSound * LPDIRECTSOUND;
#define MAX_SOUND_BUFFERS 10

// Additional Win32 typedefs that doc.h pokes at directly (beyond the
// HFONT/HMENU/HICON/HBITMAP/LARGE_INTEGER/DISPID/BSTR set already in
// mfc_shim.h):
typedef void * HACCEL;          // accelerator-table handle (MFC LoadAccelerators)

// COM Automation — mfc_shim.h already provides DISPID / BSTR / WORD /
// UINT / GUID / REFIID. doc.h's plugin invocation signatures further
// reference VARIANT / DISPPARAMS / EXCEPINFO / LCID / COleVariant by
// pointer or reference; stub the rest so the signatures compile.
// The plugin/IDispatch path is conditioned out at port time in
// favour of Lua-only plugins (see PORT_STATUS).
// VARIANT is fully defined in mfc_shim.h (with vt + a value union).
// DISPPARAMS / EXCEPINFO need to be COMPLETE here because timers.cpp
// and similar code declare local `DISPPARAMS params{}` variables.
struct DISPPARAMS {
    VARIANT * rgvarg;
    DISPID  * rgdispidNamedArgs;
    UINT      cArgs;
    UINT      cNamedArgs;
};
typedef IDispatch * LPDISPATCH;     // mfc_shim defines `using IDispatch = void`
struct IUnknown;
typedef IUnknown * LPUNKNOWN;
typedef unsigned long LCID;
class CWaitCursor { public: CWaitCursor() {} ~CWaitCursor() {} void Restore() {} };

// MUSHclient writes `struct compare_plugin_name : binary_function<...>`
// (no `std::` qualifier) — relies on a `using namespace std;` upstream
// in stdafx.h's MFC-flavoured PCH soup. C++17 deprecated but still
// ships std::binary_function under <functional>; pull the unqualified
// name into the global namespace for those references.
#include <functional>
using std::binary_function;

// PLUGIN_UNIQUE_ID_LENGTH — used by plugins.h. Original definition.
#define PLUGIN_UNIQUE_ID_LENGTH 24

// Keypad indexes — original lives at line 588 of stdafx.h.windows-original.
// Used for m_keypad[] sizing in doc.h and as case labels in sendvw.cpp.
enum {
    eKeypad_0,
    eKeypad_1,
    eKeypad_2,
    eKeypad_3,
    eKeypad_4,
    eKeypad_5,
    eKeypad_6,
    eKeypad_7,
    eKeypad_8,
    eKeypad_9,
    eKeypad_Dot,
    eKeypad_Slash,
    eKeypad_Star,
    eKeypad_Dash,
    eKeypad_Plus,
    eCtrl_Keypad_0,
    eCtrl_Keypad_1,
    eCtrl_Keypad_2,
    eCtrl_Keypad_3,
    eCtrl_Keypad_4,
    eCtrl_Keypad_5,
    eCtrl_Keypad_6,
    eCtrl_Keypad_7,
    eCtrl_Keypad_8,
    eCtrl_Keypad_9,
    eCtrl_Keypad_Dot,
    eCtrl_Keypad_Slash,
    eCtrl_Keypad_Star,
    eCtrl_Keypad_Dash,
    eCtrl_Keypad_Plus,

    eKeypad_Max_Items   // this must be last   !!!
};

// MFC POSITION — opaque iterator handle. The shim's collections
// already implement POSITION-walks via void* internally; expose the
// type-name so doc.h's POSITION-typed pointer fields compile.
typedef void * POSITION;

// CStringList — MFC-flavoured CObList<CString>. doc.h has
// `CStringList m_strHistoryList` etc. Just std::list under the hood.
class CStringList : public CObject {
public:
    using list_t = std::list<CString>;
    list_t m_list;
    int  GetCount() const                  { return static_cast<int>(m_list.size()); }
    BOOL IsEmpty()  const                  { return m_list.empty() ? TRUE : FALSE; }
    void AddHead(const CString & v)        { m_list.push_front(v); }
    void AddTail(const CString & v)        { m_list.push_back(v); }
    CString RemoveHead()                   { CString v = m_list.front(); m_list.pop_front(); return v; }
    CString RemoveTail()                   { CString v = m_list.back();  m_list.pop_back();  return v; }
    void RemoveAll()                       { m_list.clear(); }
    CString & GetHead()                    { return m_list.front(); }
    CString & GetTail()                    { return m_list.back(); }

    // POSITION-walk subset — same encoding as CList. See port/mfc_shim.h
    // for the memory-management caveat.
    using iter_t = list_t::iterator;
    POSITION GetHeadPosition() {
        return m_list.empty() ? nullptr : new iter_t(m_list.begin());
    }
    POSITION GetTailPosition() {
        if (m_list.empty()) return nullptr;
        auto it = m_list.end(); --it;
        return new iter_t(it);
    }
    CString & GetAt(POSITION pos) { return **static_cast<iter_t *>(pos); }
    CString & GetNext(POSITION & rPos) {
        auto * pIter = static_cast<iter_t *>(rPos);
        CString & ref = **pIter; ++(*pIter);
        if (*pIter == m_list.end()) { delete pIter; rPos = nullptr; }
        return ref;
    }
};

// CEvent — MFC sync primitive. doc_construct.cpp inits
// `m_eventScriptFileChanged(FALSE, TRUE)` (manual reset, initially
// signalled). The port doesn't yet wire up real signalling; ship a
// stub that compiles but is inert.
class CEvent {
public:
    CEvent(BOOL = FALSE, BOOL = FALSE, LPCTSTR = nullptr, void * = nullptr) {}
    BOOL SetEvent()    { return TRUE; }
    BOOL ResetEvent()  { return TRUE; }
    BOOL PulseEvent()  { return TRUE; }
};

// chatsock.h forward-decl chain — full include is GUI-rooted.
// CChatSocketList is `CTypedPtrList<CObList, CChatSocket*>` in the
// real header; the shim's CTypedPtrList works just as well over
// std::list<CChatSocket*>.
class CChatSocket;
typedef CList<CChatSocket *> CChatSocketList;

// CProgressDlg forward decl for CFindInfo's pointer member.
class CProgressDlg;

// CFindInfo — find/replace state for one of the doc's search dialogs.
// Verbatim from stdafx.h.windows-original line 256, with the destructor's
// `delete m_regexp` kept (regexp.h provides t_regexp which is on the
// port path). Full definition is needed because doc.h has multiple
// CFindInfo by-value members (m_DisplayFindInfo, m_RecallFindInfo, ...).
class CFindInfo : public CObject {
public:
    CFindInfo() {
        m_bCanGoBackwards = true;
        m_bForwards = true;
        m_bMatchCase = false;
        m_bRegexp = false;
        m_bUTF8 = false;
        m_pFindPosition = NULL;
        m_pProgressDlg = NULL;
        m_iStartColumn = 0;
        m_iEndColumn = 0;
        m_nTotalLines = 0;
        m_nCurrentLine = 0;
        m_iControlColumns = 0;
        m_regexp = NULL;
        m_bRepeatOnSameLine = false;
    }
    ~CFindInfo() { delete m_regexp; }

    CString  m_strTitle;
    bool     m_bCanGoBackwards;
    bool     m_bForwards;
    bool     m_bMatchCase;
    bool     m_bAgain;
    bool     m_bRegexp;
    bool     m_bUTF8;
    int      m_iStartColumn;
    int      m_iEndColumn;
    long     m_nTotalLines;
    long     m_nCurrentLine;
    POSITION m_pFindPosition;
    CProgressDlg * m_pProgressDlg;
    int      m_iControlColumns;
    t_regexp * m_regexp;
    CStringList m_strFindStringList;
    bool     m_bRepeatOnSameLine;
    std::list<std::pair<int, int> > m_MatchesOnLine;
};

// Function-pointer typedefs that Finding.cpp's signature requires.
// Upstream defines these alongside CFindInfo; they walk a generic
// "buffer of lines" via the two callbacks. The keep-list path only
// needs the type-name to parse, so the bodies aren't called at link
// time yet.
typedef void (*InitiateSearch)(const CObject * pObject, CFindInfo & FindInfo);
typedef bool (*GetNextLine)   (const CObject * pObject, CFindInfo & FindInfo, CString & strLine);

// ─────────────────────────────────────────────────────────────────────
// MUSHclient ANSI-code constants. Original lives in
// stdafx.h.windows-original line 538+; copied verbatim here so that
// ansi.cpp and friends see the named cases without relying on the
// pre-port stdafx.h.
// ─────────────────────────────────────────────────────────────────────

#define ANSI_RESET             0
#define ANSI_BOLD              1
#define ANSI_BLINK             3
#define ANSI_UNDERLINE         4
#define ANSI_SLOW_BLINK        5
#define ANSI_FAST_BLINK        6
#define ANSI_INVERSE           7
#define ANSI_STRIKEOUT         9
#define ANSI_CANCEL_BOLD      22
#define ANSI_CANCEL_BLINK     23
#define ANSI_CANCEL_UNDERLINE 24
#define ANSI_CANCEL_SLOW_BLINK  25
#define ANSI_CANCEL_INVERSE   27
#define ANSI_CANCEL_STRIKEOUT 29
#define ANSI_TEXT_BLACK       30
#define ANSI_TEXT_RED         31
#define ANSI_TEXT_GREEN       32
#define ANSI_TEXT_YELLOW      33
#define ANSI_TEXT_BLUE        34
#define ANSI_TEXT_MAGENTA     35
#define ANSI_TEXT_CYAN        36
#define ANSI_TEXT_WHITE       37
#define ANSI_TEXT_256_COLOUR  38
#define ANSI_SET_FOREGROUND_DEFAULT 39
#define ANSI_BACK_BLACK       40
#define ANSI_BACK_RED         41
#define ANSI_BACK_GREEN       42
#define ANSI_BACK_YELLOW      43
#define ANSI_BACK_BLUE        44
#define ANSI_BACK_MAGENTA     45
#define ANSI_BACK_CYAN        46
#define ANSI_BACK_WHITE       47
#define ANSI_BACK_256_COLOUR  48
#define ANSI_SET_BACKGROUND_DEFAULT 49

// ─────────────────────────────────────────────────────────────────────
// Port-side externs that MUSHclient sources expect to be in scope
// after stdafx.h. The actual definitions live in port/wx/main.cpp and
// the future doc-core port files.
// ─────────────────────────────────────────────────────────────────────

// Win32 GetVersion family — VER_PLATFORM_WIN32s is the legacy
// Win32s value; on the port nothing actually runs Win32s, so we
// keep the constant in scope but the comparison `App.platform ==
// VER_PLATFORM_WIN32s` will always be false.
#define VER_PLATFORM_WIN32s        0
#define VER_PLATFORM_WIN32_WINDOWS 1
#define VER_PLATFORM_WIN32_NT      2

// Forward declarations for types referenced below.
class CMUSHclientApp;

// CMUSHclientApp port-side stub. The Windows build has a much larger
// CWinApp-derived class; for the KEEP+SHIM compile we only need the
// fields/methods the keep-list .cpp files actually touch. Add more
// here when failing files surface new ones.
class CMUSHclientApp /* : public CWinApp on Windows */ {
public:
    // Platform discriminator (0/1/2 — see VER_PLATFORM_* above).
    int platform = VER_PLATFORM_WIN32_NT;

    // Default-file paths saved across sessions.
    CString m_strDefaultNameGenerationFile;

    // Regex engine global flags. Mirrors upstream prefs; defaults
    // chosen to match the Win32 MUSHclient install. Add fields here
    // when KEEP+SHIM tally surfaces new App.* references.
    BOOL m_bRegexpMatchEmpty = FALSE;

    // High-precision counter freq — Win32 QueryPerformanceFrequency.
    // The port uses clock_gettime(CLOCK_MONOTONIC) which is already
    // nanosecond, so this is mostly inert; kept for the call sites
    // that divide an elapsed-counter by it.
    LARGE_INTEGER m_iCounterFrequency{};

    // Name→COLORREF lookup populated by Color.cpp at startup.
    // world_debug.cpp walks this via CMap POSITION-based API
    // (GetStartPosition / GetNextAssoc) and passes a CString out
    // parameter, so the key type has to be CString — std::string
    // would force a conversion the upstream code doesn't do.
    CMapBase<CString, COLORREF> m_ColoursMap;

    // Persistence stub — Windows uses the registry; the port writes
    // these to a SQLite prefs DB. Stub returns 0 so callers compile.
    int db_write_string(LPCTSTR section, LPCTSTR key, LPCTSTR value) {
        (void)section; (void)key; (void)value; return 0;
    }
};
extern CMUSHclientApp App;     // port-side singleton (mirrors theApp on Windows)

// MUSHclient's UMessageBox is a unicode-aware wrapper around
// AfxMessageBox; here we just funnel through AfxMessageBox.
inline int UMessageBox(LPCTSTR s, UINT type = 0, UINT helpid = 0) {
    return AfxMessageBox(s, type, helpid);
}
inline int UMessageBox(const CString & s, UINT type = 0, UINT helpid = 0) {
    return AfxMessageBox((LPCTSTR)s, type, helpid);
}

// InitZlib lives in MUSHclient.cpp. Forward-decl so worldsock /
// telnet_phases see it before their own use sites.
int InitZlib(z_stream & strm);

// xterm_256_colours table — the actual definition is in Color.cpp;
// ansi.cpp and mushview.cpp index into it. The Windows build exposes
// this via MUSHclient.h; we expose it here so the keep-list .cpp
// files don't need to pull in MUSHclient.h directly.
extern COLORREF xterm_256_colours[256];

// Translate — i18n helper; the actual definition lives in MUSHclient.cpp
// (or wherever the Win32 build keeps it). Forward-decl so anywhere
// that calls Translate("...") compiles.
CString Translate(LPCTSTR english);

// CStyle is defined in OtherTypes.h and used widely as `new CStyle`.
// The NEWSTYLE / DELETESTYLE macros are the upstream-codebase
// conventions for the allocation+free pair. Declared here so Line.cpp
// / ProcessPreviousLine.cpp / doc.cpp see them. The macros expand at
// the call site, where CStyle's full definition is in scope via the
// .cpp's other includes.
class CStyle;
#define NEWSTYLE        (new CStyle)
#define DELETESTYLE(p)  delete (p)

// CHotspot — defined in miniwindow.h. Forward-decl here so MUSHview.h
// (which references it in member-pointer types) parses without
// pulling miniwindow.h into the KEEP+SHIM compile.
class CHotspot;

// lua_State — forward-decl matching Lua's own typedef. The KEEP+SHIM
// compile path doesn't need the full Lua API, just the type-name so
// that headers like TextView.h can declare `lua_State * L;` member
// fields. .cpp files that actually call Lua will pull in <lua.hpp>.
extern "C" { struct lua_State; }

// TFormat — printf-style formatter that returns a CString. Real
// implementation lives in Utilities.cpp. Forward-decl with C-style
// variadic so call sites in Finding.cpp, telnet_phases.cpp, etc.
// compile without dragging in Utilities.h.
CString TFormat(const char * fmt, ...);

// String split helpers (Utilities.cpp). MUSHclient has both List and
// Vector flavors; the actual signatures take std::string + std::vector
// of std::string (not CString) and a trim-spaces flag.
class CStringList;   // defined later in stdafx.h
void StringToList   (LPCTSTR strString, LPCTSTR strDelimiter, CStringList & strList);
void StringToVector (const std::string s,
                     std::vector<std::string> & v,
                     const std::string delim,
                     const bool trim_spaces);

// tolower(string) — Utilities.cpp:1118 defines a per-string overload
// that returns a lowercased copy. Forward-decl matches that signature
// so call sites in evaluate.cpp etc. compile alongside the standard
// <cctype> int tolower(int) — overload resolution picks correctly.
std::string tolower(const std::string & s);

// FindAndReplace — string find/replace helper used widely by
// evaluate.cpp / lua_utils.cpp / functionlist.cpp. The actual
// definition isn't in the upstream tree we cloned (Gammon may have
// kept it in a header we don't have); forward-decl with the call-site
// shape so .cpp files compile, and the port-side .cpp will provide
// the real implementation alongside StringToVector / Translate.
std::string FindAndReplace(const std::string & s,
                           const char * find, const char * repl);

// File-browsing CWD helpers (Utilities.cpp:3345 / 3352).
void ChangeToFileBrowsingDirectory();
void ChangeToStartupDirectory();

// More Utilities.cpp helpers used widely in the keep set. Default
// args mirror upstream call sites that pass 1 arg (and rely on the
// header to fill in the rest).
CString FixHTMLString(const CString strToFix);
CString ConvertToRegularExpression(const CString & strMatchString,
                                   const bool bWholeLine = false,
                                   const bool bMakeAsterisksWildcards = false);
const CString Convert_PCRE_Runtime_Error(const int iError);
bool    IsStringNumber(const std::string & s, const bool bSigned = true);

// NameGeneration entry point used from mainfrm.cpp / app menu.
void ReadNames(const LPCTSTR sName, const bool bNoDialog = false);

// TMessageBox — translated-text variant of MessageBox; same shape as
// UMessageBox / AfxMessageBox. Real implementation in Utilities.cpp.
int TMessageBox(LPCTSTR lpszText, UINT nType = 0, UINT nIDHelp = 0);

// CommandID-name lookup table (accelerators.cpp::CommandIDs[]).
// Value type is `{ int id; const char *name; }`.
struct tCommandIDMapping { int id; const char * name; };

// CChatSocket — minimal stub matching the chatsock.h field surface
// the keep-list .cpp files touch. Full class lives in chatsock.h
// (along with the Win32 CAsyncSocket dependency we don't pull in).
// Add fields here when KEEP+SHIM tally surfaces new ones.
class CChatSocket : public CAsyncSocket {
public:
    BOOL m_bDeleteMe     = FALSE;
    int  m_iChatStatus   = 0;
    BOOL m_bCanSnoop     = FALSE;
    BOOL m_bHeIsSnooping = FALSE;
    void Process_Snoop(LPCTSTR /*data*/, int /*len*/) {}
};

// CScriptEngine — VBScript / JScript / Lua dispatcher. The port
// only keeps Lua, so the full engine class is being rewritten; for
// the keep-set compile we just need the type-name and the methods
// timers.cpp / scripting code touches.
class CScriptEngine {
public:
    bool IsLua() const { return true; }
    // ExecuteLua's call sites pass DISPID, CString, enum, CString, list,
    // ... in many shapes. Variadic template accepts any combination so
    // the keep-set compile path doesn't need to track every overload.
    template <typename... A> int ExecuteLua(A &&...) { return 0; }
};

// Chat-state enum. Real definition in chatsock.h; the keep-list
// .cpp files only compare against eChatConnected so a single name
// is enough.
enum eChatStatus {
    eChatNotConnected = 0,
    eChatConnecting,
    eChatConnected,
    eChatDisconnecting,
};

// CMUSHclientApp.m_ColoursMap — name→COLORREF lookup (e.g. for ANSI
// extended-colour names). Real definition is a CMapStringToOb in
// MUSHclient.cpp; for compile we just need a member of compatible
// type. Add to the App stub.
// (Field added directly on CMUSHclientApp below.)

// More Win32 constants. _MAX_PATH = 260 (matches Win32 NTFS legacy
// path limit). FW_DONTCARE / FW_NORMAL / FW_BOLD = LOGFONT weight
// values. CP_THREAD_ACP = 3 = "thread's ANSI codepage" — the port
// runs UTF-8 and ignores the value.
#ifndef _MAX_PATH
#define _MAX_PATH       260
#endif
#ifndef MAX_PATH
#define MAX_PATH        _MAX_PATH
#endif
#define FW_DONTCARE     0
#define FW_NORMAL       400
#define FW_BOLD         700
#define CP_THREAD_ACP   3
#define CP_ACP          0
#define CP_UTF8         65001

// One more dialog IDD — CG_IDD_PROGRESS shows up in the Component-
// Gallery-flavored ProgDlg.h that MUSHclient uses.
#define CG_IDD_PROGRESS 30007

// Win32 LOGFONT charset values + Unicode/MultiByte conversion flags
// used by scriptingoptions / telnet_phases. Values match the Win32
// SDK; the port doesn't actually do conversion against any specific
// codepage, but the call-sites use the named constants.
#define DEFAULT_CHARSET       1
#define ANSI_CHARSET          0
#define SYMBOL_CHARSET        2
#define OEM_CHARSET           255
#define MB_PRECOMPOSED        0x00000001
#define MB_COMPOSITE          0x00000002
#define MB_USEGLYPHCHARS      0x00000004
#define MB_ERR_INVALID_CHARS  0x00000008

// Frame — the global CMainFrame singleton, mirror of App. Used by
// many .cpp files to push status-bar messages and route menu state.
// Stub here exposes only the methods the keep-list .cpp files
// actually touch; widen as new tally failures surface.
struct CFrameStub {
    void SetStatusNormal() {}
    void SetStatus(LPCTSTR /*s*/) {}
    void SetStatusMessageNow(LPCTSTR /*s*/) {}
};
extern CFrameStub Frame;

// _itoa — Win32 itoa(int, char*, base). Standard C has the same on
// most platforms; provide an inline shim that wraps snprintf so we
// don't depend on Solaris 7's libc behaviour.
inline char * _itoa(int value, char * str, int radix) {
    if (radix == 10)      std::snprintf(str, 32, "%d",  value);
    else if (radix == 16) std::snprintf(str, 32, "%x",  value);
    else if (radix == 8)  std::snprintf(str, 32, "%o",  value);
    else                  std::snprintf(str, 32, "%d",  value);
    return str;
}

// VariantInit / VariantClear — Win32 OLE Automation. The port drops
// VBScript / JScript so these are inert; bodies provided so Mapping/
// scriptingoptions calls compile. Real Lua-only path doesn't go
// through VARIANT.
inline void VariantInit (VARIANT * v) { if (v) { v->vt = 0; } }
inline long VariantClear(VARIANT * v) { if (v) { v->vt = 0; } return 0; }

// MultiByteToWideChar — Win32 codepage → UTF-16 conversion. Stubbed
// to copy bytes 1:1 (UTF-8 / ASCII assumption); telnet_phases passes
// it through but the result feeds an inert charset path on the port.
inline int MultiByteToWideChar(UINT /*cp*/, DWORD /*flags*/,
                               const char * mbStr, int mbLen,
                               WCHAR * wStr, int wLen) {
    if (!mbStr) return 0;
    if (mbLen < 0) mbLen = static_cast<int>(std::strlen(mbStr));
    if (!wStr || wLen == 0) return mbLen;
    int n = std::min(mbLen, wLen);
    for (int i = 0; i < n; ++i) wStr[i] = static_cast<unsigned char>(mbStr[i]);
    return n;
}
inline int WideCharToMultiByte(UINT /*cp*/, DWORD /*flags*/,
                               const WCHAR * wStr, int wLen,
                               char * mbStr, int mbLen,
                               const char * /*defaultChar*/ = nullptr,
                               BOOL * /*usedDefaultChar*/ = nullptr) {
    if (!wStr) return 0;
    if (wLen < 0) { wLen = 0; while (wStr[wLen]) ++wLen; }
    if (!mbStr || mbLen == 0) return wLen;
    int n = std::min(wLen, mbLen);
    for (int i = 0; i < n; ++i) mbStr[i] = static_cast<char>(wStr[i] & 0xFF);
    return n;
}

// ColourToName — Utilities.cpp:694 returns CString for a COLORREF.
CString ColourToName(const COLORREF colour);

// FixupEscapeSequences — evaluate.cpp uses it to render \n / \t / \\
// inside scripted strings. Real impl in Utilities.cpp.
CString FixupEscapeSequences(LPCTSTR src);

// ptrCFont — upstream typedef used by FixFont() and similar dialog
// helpers. Owning pointer to a CFont; kept as a raw pointer to match
// the call-site semantics (delete pFont before assignment).
typedef CFont * ptrCFont;

// COleSafeArray — Win32 OLE SAFEARRAY wrapper used in scriptingoptions.cpp.
// The port drops scripted-property-bag passing, so a stub class with
// the methods scriptingoptions touches (Add, GetOneDimSize, etc.) is
// enough — bodies do nothing. Placed before the VARIANT constants
// since some bodies index into them.
class COleSafeArray {
public:
    COleSafeArray() = default;
    template <typename... A> void Add(A &&...) {}
    template <typename... A> void Create(A &&...) {}
    template <typename... A> void CreateOneDim(A &&...) {}
    template <typename... A> void GetElement(A &&...) {}
    template <typename... A> void PutElement(A &&...) {}
    long GetOneDimSize() const { return 0; }
    DWORD GetDim() const       { return 1; }
    void  Destroy() {}
    template <typename... A> void Detach(A &&...) {}
    template <typename... A> void Attach(A &&...) {}
    operator VARIANT() const   { return VARIANT{}; }
};

// Win32 OLE VARIANT type tags. The port drops VBScript so these are
// inert; the calling code just compares against the enum values.
#define VT_EMPTY    0
#define VT_NULL     1
#define VT_I2       2
#define VT_I4       3
#define VT_R4       4
#define VT_R8       5
#define VT_BSTR     8
#define VT_BOOL     11
#define VT_VARIANT  12
#define VT_I1       16
#define VT_UI1      17
#define VT_DATE     7

// pcre internals exposed by telnet_phases.cpp's UTF-8 validation
// shortcut. We don't ship pcre's private symbols on the port; supply
// a forward declaration that resolves at link time when the keep set
// gets linked into the real binary.
extern "C" int _pcre_valid_utf(const unsigned char *, int, int *);

// IDD_* dialog resource IDs. The Windows build has these in resource.h
// (auto-generated by the dialog editor); the port keeps them as
// numeric constants since the actual dialogs are deleted + replaced
// with wxDialog. Values just need to be unique-and-non-zero so the
// `enum { IDD = IDD_FOO };` declarations inside dialog classes parse.
// Add to this list when KEEP+SHIM tally surfaces new IDD references.
#define IDD_FIND              30001
#define IDD_MAPPER            30002
#define IDD_MAP_MOVE          30003
#define IDD_MAP_COMMENT       30004
#define IDD_REGEXP_PROBLEM    30005
#define IDD_PROGRESS          30006

#endif
