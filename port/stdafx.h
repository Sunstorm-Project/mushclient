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
struct VARIANT;
struct DISPPARAMS;
struct EXCEPINFO;
typedef IDispatch * LPDISPATCH;     // mfc_shim defines `using IDispatch = void`
struct IUnknown;
typedef IUnknown * LPUNKNOWN;
typedef unsigned long LCID;
class COleVariant;            // forward decl — defined in MFC OLE; doc.h uses by-pointer
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
    std::list<CString> m_list;
    int  GetCount() const                  { return static_cast<int>(m_list.size()); }
    BOOL IsEmpty()  const                  { return m_list.empty() ? TRUE : FALSE; }
    void AddHead(const CString & v)        { m_list.push_front(v); }
    void AddTail(const CString & v)        { m_list.push_back(v); }
    CString RemoveHead()                   { CString v = m_list.front(); m_list.pop_front(); return v; }
    CString RemoveTail()                   { CString v = m_list.back();  m_list.pop_back();  return v; }
    void RemoveAll()                       { m_list.clear(); }
    CString & GetHead()                    { return m_list.front(); }
    CString & GetTail()                    { return m_list.back(); }
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

class CMUSHclientApp;
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

#endif
