// port/mfc_shim.h — non-GUI MFC compatibility shim for the SPARC Solaris 7 port
//
// Replaces <afx.h> for MUSHclient sources that use MFC primitives (CString,
// CTime, CRect, ASSERT, AfxMessageBox, …) but don't reach into the MFC GUI
// stack. The GUI-touching files (CView/CDocument/CDialog/MDIClient/etc.) are
// not handled here — those are deleted and replaced with wxWidgets equivalents.
//
// Scope: enough of the MFC API surface that scripting/, mxp/, names/, and the
// non-GUI portions of doc.cpp / Utilities.cpp / Color.cpp / etc. compile against
// libstdc++ on Solaris 7 + the cross-toolchain. Patterns established here:
//
//   - CString  →  std::string-backed wrapper exposing the MFC method names
//                 actually used by MUSHclient (IsEmpty, GetLength, Left, Right,
//                 Mid, Format, Replace, Find, MakeUpper, MakeLower, Trim*,
//                 GetBuffer, ReleaseBuffer, CompareNoCase). Inventoried 1277 uses.
//   - typedefs → BOOL/DWORD/UINT/BYTE/WORD/LONG/ULONG/COLORREF as fixed-width
//                ints; HWND/HINSTANCE/HFONT etc. as opaque void*.
//   - macros   → ASSERT/VERIFY/ASSERT_VALID become assert() (debug) or no-op
//                (release). TRACE/TRACE0/TRACE1 → fprintf(stderr,…).
//                AfxMessageBox → wxMessageBox passthrough later, here a stub.
//   - misc     → CObject empty base class; CException base wrapping
//                std::exception; CRect/CPoint/CSize/CTime/CTimeSpan POD
//                wrappers exposing the MFC-style accessors the codebase uses.
//
// Out of scope (deleted, not shimmed):
//   - CView/CDocument/CFrameWnd/CDialog/CMDIChildWnd/CCmdUI            → wxWidgets
//   - CDC/CBitmap/CPen/CBrush/CFont/CMetaFileDC                        → wxDC etc.
//   - CWinApp/CMainFrame/CChildFrame/CMUSHclientApp/CSplitterWnd       → wxApp/wxFrame
//   - CSocket/CAsyncSocket                                             → wxSocket
//   - CRegKey                                                          → wxConfig / drop
//   - CCmdTarget/RUNTIME_CLASS/DECLARE_DYNCREATE/IMPLEMENT_DYNCREATE   → drop
//   - DDX_/DDV_ MFC dialog data exchange                               → wxValidator
//   - COleDateTime / COleVariant / IDispatch / IActiveScriptSite       → drop (no WSH)
//   - DirectSound, mmsystem, dsound.h                                  → SDL2_mixer
//
// This header is C++17. Use libstdc++ from cross-GCC 15.2 on Solaris 7
// (provided by SSTlstdc, runtime libstdc++.so.6.0.34).

#ifndef MFC_SHIM_H
#define MFC_SHIM_H

// Sentinel that other port-aware headers (MUSHclient.h, doc.h, etc.)
// check via #ifdef __SPARC_SOLARIS7_PORT__ to elide their Win32-only
// content. Defined here so any TU that includes the shim
// transitively gets it.
#ifndef __SPARC_SOLARIS7_PORT__
#define __SPARC_SOLARIS7_PORT__ 1
#endif

// Win32-only headers some MUSHclient sources pull in directly. Map
// them to POSIX equivalents via this shim — no need to litter the
// downstream sources with #ifdefs.
#define process_h_shim_loaded 1
#define _getpid getpid
#include <unistd.h>     // getpid + friends, replaces <process.h>

// Network typedefs — Win32 uses SOCKADDR_IN / SOCKADDR (uppercase
// typedef of struct sockaddr_in / sockaddr). POSIX provides the
// struct directly, so alias.
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using SOCKADDR_IN = struct sockaddr_in;
using SOCKADDR    = struct sockaddr;
using IN_ADDR     = struct in_addr;
using SOCKET      = int;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <time.h>      // clock_gettime / struct timespec for QueryPerformance* shim
#include <cerrno>
#include <chrono>
#include <exception>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <list>
#include <algorithm>

// MFC's afx.h leaks ::min and ::max as macros so unqualified `min(a,b)`
// in MUSHclient sources compiles. We can't use macros without breaking
// std::min/std::max calls in headers we include below, so expose the
// std versions via using-declarations at global scope. Mirror MFC's
// behaviour without the preprocessor footgun.
using std::min;
using std::max;

// MUSHclient's headers use unqualified `string`/`vector`/`map` etc.
// (the Windows build presumably had `using namespace std;` via the
// MFC umbrella). Pull the most-used std types into ::scope so the
// existing source compiles without sprinkling std:: prefixes.
using std::string;
using std::vector;
using std::map;
using std::list;
using std::set;
using std::pair;

// ─────────────────────────────────────────────────────────────────────
// Win32 type aliases (the subset actually referenced in MUSHclient
// sources we keep).
// ─────────────────────────────────────────────────────────────────────

using BOOL     = int;          // MFC convention: TRUE = nonzero, FALSE = 0
using BYTE     = std::uint8_t;
using WORD     = std::uint16_t;
using DWORD    = std::uint32_t;
using LONG     = std::int32_t;
using ULONG    = std::uint32_t;
using UINT     = unsigned int;
using LONGLONG = std::int64_t;
using ULONGLONG= std::uint64_t;

using LPSTR    = char *;
using LPCSTR   = const char *;
using LPVOID   = void *;
using LPCVOID  = const void *;
using LPBYTE   = BYTE *;
using PUINT    = UINT *;
using PDWORD   = DWORD *;
using PBYTE    = BYTE *;
using PWORD    = WORD *;

// Win32 message-handler scalars. WPARAM / LPARAM are the message
// payload words; LRESULT is the return type of every message handler.
// On 32-bit SPARC: same widths as on 32-bit Windows.
using WPARAM   = std::uintptr_t;
using LPARAM   = std::intptr_t;
using LRESULT  = std::intptr_t;
using ATOM     = WORD;

using HANDLE    = void *;
using HWND      = void *;
using HINSTANCE = void *;
using HFONT     = void *;
using HICON     = void *;
using HCURSOR   = void *;
using HMENU     = void *;
using HBRUSH    = void *;
using HBITMAP   = void *;
using HMODULE   = void *;
using HRESULT   = LONG;

// Win32 / COM compat scalars. The actual COM scripting (WSH /
// IActiveScript path) is dropped in the SPARC port; these keep the
// declarations referencing DISPID / __int64 / VARIANT compiling but
// the values are inert.
using DISPID    = LONG;
#define DISPID_UNKNOWN  ((DISPID)-1)
using __int64   = long long;
using ITypeInfo = void;             // never dereferenced post-WSH-drop
using IDispatch = void;
using REFIID    = const struct GUID *;
struct GUID { unsigned long d1; unsigned short d2, d3; unsigned char d4[8]; };
struct EXCEPINFO { unsigned short wCode, wReserved; void *bstrSource, *bstrDescription, *bstrHelpFile; unsigned long dwHelpContext; void *pvReserved; void *pfnDeferredFillIn; long scode; };
using BSTR      = wchar_t *;
struct VARIANT { unsigned short vt; unsigned short wReserved1, wReserved2, wReserved3; union { long lVal; double dblVal; void *byref; }; };
struct VARIANTARG : VARIANT {};

using COLORREF = DWORD;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL nullptr
#endif

#define RGB(r, g, b) \
    ((COLORREF)((BYTE)(r) | ((WORD)((BYTE)(g)) << 8) | ((DWORD)((BYTE)(b)) << 16)))
#define GetRValue(c) ((BYTE)((c) & 0xFF))
#define GetGValue(c) ((BYTE)(((c) >> 8) & 0xFF))
#define GetBValue(c) ((BYTE)(((c) >> 16) & 0xFF))

// MFC HRESULT helpers
#define S_OK            ((HRESULT)0L)
#define S_FALSE         ((HRESULT)1L)
#define E_FAIL          ((HRESULT)0x80004005L)
#define SUCCEEDED(hr)   ((HRESULT)(hr) >= 0)
#define FAILED(hr)      ((HRESULT)(hr) <  0)

// Win32 NLS sublanguage IDs (winnt.h). exceptions.cpp uses
// SUBLANG_SYS_DEFAULT for FormatMessage; the value matters only on
// Windows. On the port we accept any constant — strerror() ignores it.
#ifndef SUBLANG_DEFAULT
#define SUBLANG_DEFAULT     0x01
#define SUBLANG_SYS_DEFAULT 0x02
#define LANG_NEUTRAL        0x00
#endif

// Win32 string-copy helpers used in exceptions.cpp + a couple other
// places. Map to standard strncpy with explicit NUL termination.
inline char * lstrcpyn(char * dest, const char * src, int n) {
    if (n <= 0) return dest;
    std::strncpy(dest, src, static_cast<std::size_t>(n) - 1);
    dest[n - 1] = '\0';
    return dest;
}

// ANSI palette indices. The original stdafx.h.windows-original
// declares these as a single anonymous enum (line 191):
//   enum { BLACK = 0, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE };
// We mirror that here so MUSHclient's `iForeColour = WHITE` /
// `m_normalcolour[BLACK]` references compile against the shim.
enum {
    BLACK = 0,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE
};

// Calling-convention macros: meaningless on SPARC, define empty so the
// thousands of WINAPI / CALLBACK / STDMETHODCALLTYPE annotations stay valid.
#define WINAPI
#define CALLBACK
#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define APIENTRY
#define _cdecl
#define __cdecl
#define __stdcall
#define _stdcall
#define _T(x) x
#define TEXT(x) x
using TCHAR = char;
using LPTSTR = char *;
using LPCTSTR = const char *;

// ─────────────────────────────────────────────────────────────────────
// CString — std::string with MFC-flavoured method surface.
//
// Inventoried usage in MUSHclient (across ~110 .cpp files):
//   IsEmpty       245
//   GetLength     133
//   Left           87
//   Replace        71
//   Mid            61
//   Format         57
//   CompareNoCase  52
//   Find           36
//   GetBuffer      30
//   ReleaseBuffer  28
//   TrimLeft       21
//   MakeLower      21
//   TrimRight      20
//   Right          18
//   MakeUpper       5
//
// Plus operators: =, +, +=, ==, !=, <, > and implicit char* conversion.
//
// Implementation goals:
//   - Behave exactly like MFC's CStringA on the parts we use.
//   - Backed by std::string so all the heavy lifting (allocation,
//     refcount semantics, COW) is delegated to the standard library.
//   - GetBuffer / ReleaseBuffer mutate the underlying buffer; we use
//     std::string::data() (non-const since C++17) and resize() to
//     reproduce MFC's "give me a writable pointer to N chars" idiom.
// ─────────────────────────────────────────────────────────────────────

class CString {
public:
    CString() = default;
    CString(const char * s) : m_str(s ? s : "") {}
    CString(const char * s, int len) : m_str(s ? s : "", static_cast<std::size_t>(len)) {}
    CString(const CString & other) = default;
    CString(CString && other) noexcept = default;
    CString(char ch, int repeat = 1) : m_str(static_cast<std::size_t>(repeat), ch) {}
    explicit CString(const std::string & s) : m_str(s) {}

    CString & operator=(const CString &) = default;
    CString & operator=(CString &&) noexcept = default;
    CString & operator=(const char * s) { m_str = s ? s : ""; return *this; }
    CString & operator=(char ch)        { m_str.assign(1, ch); return *this; }

    // Implicit conversion to const char* — matches MFC behaviour.
    operator const char *() const { return m_str.c_str(); }

    // Length / emptiness
    int  GetLength() const noexcept     { return static_cast<int>(m_str.size()); }
    BOOL IsEmpty() const noexcept       { return m_str.empty() ? TRUE : FALSE; }
    void Empty() noexcept               { m_str.clear(); }

    // Indexed access
    char GetAt(int i) const             { return m_str.at(static_cast<std::size_t>(i)); }
    void SetAt(int i, char ch)          { m_str.at(static_cast<std::size_t>(i)) = ch; }
    char operator[](int i) const        { return m_str[static_cast<std::size_t>(i)]; }

    // Substring extraction
    CString Left(int n) const {
        if (n < 0) n = 0;
        if (static_cast<std::size_t>(n) > m_str.size()) n = static_cast<int>(m_str.size());
        return CString(m_str.substr(0, static_cast<std::size_t>(n)));
    }
    CString Right(int n) const {
        if (n < 0) n = 0;
        if (static_cast<std::size_t>(n) > m_str.size()) n = static_cast<int>(m_str.size());
        return CString(m_str.substr(m_str.size() - static_cast<std::size_t>(n)));
    }
    CString Mid(int first) const {
        if (first < 0) first = 0;
        if (static_cast<std::size_t>(first) >= m_str.size()) return CString();
        return CString(m_str.substr(static_cast<std::size_t>(first)));
    }
    CString Mid(int first, int count) const {
        if (first < 0) first = 0;
        if (count < 0) count = 0;
        if (static_cast<std::size_t>(first) >= m_str.size()) return CString();
        return CString(m_str.substr(
            static_cast<std::size_t>(first),
            static_cast<std::size_t>(count)));
    }

    // Case
    void MakeUpper() {
        for (char & c : m_str) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    void MakeLower() {
        for (char & c : m_str) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Trim — MFC defaults to whitespace
    void TrimLeft() {
        std::size_t i = 0;
        while (i < m_str.size() && std::isspace(static_cast<unsigned char>(m_str[i]))) ++i;
        m_str.erase(0, i);
    }
    void TrimRight() {
        while (!m_str.empty() && std::isspace(static_cast<unsigned char>(m_str.back()))) m_str.pop_back();
    }
    void TrimLeft(char ch) {
        std::size_t i = 0;
        while (i < m_str.size() && m_str[i] == ch) ++i;
        m_str.erase(0, i);
    }
    void TrimRight(char ch) {
        while (!m_str.empty() && m_str.back() == ch) m_str.pop_back();
    }

    // Find — MFC returns -1 on miss, position otherwise
    int Find(char ch) const {
        auto p = m_str.find(ch);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int Find(char ch, int start) const {
        auto p = m_str.find(ch, static_cast<std::size_t>(start));
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int Find(const char * sub) const {
        auto p = m_str.find(sub);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int Find(const char * sub, int start) const {
        auto p = m_str.find(sub, static_cast<std::size_t>(start));
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int ReverseFind(char ch) const {
        auto p = m_str.rfind(ch);
        return p == std::string::npos ? -1 : static_cast<int>(p);
    }

    // Replace — MFC returns count of replacements made
    int Replace(char from, char to) {
        int n = 0;
        for (char & c : m_str) if (c == from) { c = to; ++n; }
        return n;
    }
    int Replace(const char * from, const char * to) {
        if (!from || !*from) return 0;
        int n = 0;
        std::size_t flen = std::strlen(from);
        std::size_t tlen = to ? std::strlen(to) : 0;
        std::size_t pos = 0;
        while ((pos = m_str.find(from, pos)) != std::string::npos) {
            m_str.replace(pos, flen, to ? to : "");
            pos += tlen;
            ++n;
        }
        return n;
    }

    // Comparison
    int  Compare(const char * s) const          { return std::strcmp(m_str.c_str(), s ? s : ""); }
    int  CompareNoCase(const char * s) const    { return ::strcasecmp(m_str.c_str(), s ? s : ""); }

    // Format / FormatV — printf-style
    void Format(const char * fmt, ...) {
        std::va_list ap;
        va_start(ap, fmt);
        FormatV(fmt, ap);
        va_end(ap);
    }
    void FormatV(const char * fmt, std::va_list ap) {
        std::va_list ap2;
        va_copy(ap2, ap);
        int needed = std::vsnprintf(nullptr, 0, fmt, ap2);
        va_end(ap2);
        if (needed < 0) { m_str.clear(); return; }
        m_str.resize(static_cast<std::size_t>(needed));
        std::vsnprintf(&m_str[0], static_cast<std::size_t>(needed) + 1, fmt, ap);
    }

    // GetBuffer / ReleaseBuffer — MFC's "give me a writable C string of at
    // least N chars" idiom. Caller must call ReleaseBuffer() after writing,
    // optionally passing the new length (-1 = strlen the result).
    char * GetBuffer(int min_len = 0) {
        if (static_cast<std::size_t>(min_len) > m_str.size())
            m_str.resize(static_cast<std::size_t>(min_len));
        return &m_str[0];
    }
    void ReleaseBuffer(int new_len = -1) {
        if (new_len < 0)
            new_len = static_cast<int>(std::strlen(m_str.c_str()));
        m_str.resize(static_cast<std::size_t>(new_len));
    }

    // LoadString — MFC pulls from STRING_TABLE in the .rc file. We don't
    // have those; return FALSE so callers fall through to the constant
    // string they already have inline.
    BOOL LoadString(UINT /*id*/) { return FALSE; }

    // Insert / Delete — direct char-position editing
    int Insert(int pos, const char * s) {
        if (!s) return GetLength();
        if (pos < 0) pos = 0;
        if (static_cast<std::size_t>(pos) > m_str.size()) pos = static_cast<int>(m_str.size());
        m_str.insert(static_cast<std::size_t>(pos), s);
        return GetLength();
    }
    int Insert(int pos, char ch) {
        if (pos < 0) pos = 0;
        if (static_cast<std::size_t>(pos) > m_str.size()) pos = static_cast<int>(m_str.size());
        m_str.insert(m_str.begin() + pos, ch);
        return GetLength();
    }
    int Delete(int pos, int count = 1) {
        if (pos < 0 || static_cast<std::size_t>(pos) >= m_str.size()) return GetLength();
        if (count < 0) count = 0;
        m_str.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(count));
        return GetLength();
    }

    // Concatenation
    CString & operator+=(const CString & rhs) { m_str += rhs.m_str; return *this; }
    CString & operator+=(const char * rhs)    { m_str += (rhs ? rhs : ""); return *this; }
    CString & operator+=(char ch)             { m_str += ch; return *this; }

    friend CString operator+(const CString & a, const CString & b) {
        CString r(a); r += b; return r;
    }
    friend CString operator+(const CString & a, const char * b) {
        CString r(a); r += b; return r;
    }
    friend CString operator+(const char * a, const CString & b) {
        CString r(a); r += b; return r;
    }

    // Equality / ordering
    friend bool operator==(const CString & a, const CString & b) { return a.m_str == b.m_str; }
    friend bool operator==(const CString & a, const char * b)    { return a.m_str == (b ? b : ""); }
    friend bool operator==(const char * a, const CString & b)    { return (a ? a : "") == b.m_str; }
    friend bool operator!=(const CString & a, const CString & b) { return !(a == b); }
    friend bool operator!=(const CString & a, const char * b)    { return !(a == b); }
    friend bool operator!=(const char * a, const CString & b)    { return !(a == b); }
    friend bool operator< (const CString & a, const CString & b) { return a.m_str <  b.m_str; }
    friend bool operator<=(const CString & a, const CString & b) { return a.m_str <= b.m_str; }
    friend bool operator> (const CString & a, const CString & b) { return a.m_str >  b.m_str; }
    friend bool operator>=(const CString & a, const CString & b) { return a.m_str >= b.m_str; }

    // Escape hatch for std::-based code
    const std::string & str() const noexcept { return m_str; }
    std::string &       str()       noexcept { return m_str; }

private:
    std::string m_str;
};

// ─────────────────────────────────────────────────────────────────────
// CRect / CPoint / CSize — POD geometry. 95+120+20 uses; we keep the
// API minimal (the GUI files that drive the heavy operator overloads
// are being replaced with wxWidgets sources).
// ─────────────────────────────────────────────────────────────────────

struct CSize {
    LONG cx{0};
    LONG cy{0};
    CSize() = default;
    CSize(LONG x, LONG y) : cx(x), cy(y) {}
};

struct CPoint {
    LONG x{0};
    LONG y{0};
    CPoint() = default;
    CPoint(LONG x_, LONG y_) : x(x_), y(y_) {}
    void Offset(LONG dx, LONG dy) { x += dx; y += dy; }
};

struct CRect {
    LONG left{0}, top{0}, right{0}, bottom{0};
    CRect() = default;
    CRect(LONG l, LONG t, LONG r, LONG b) : left(l), top(t), right(r), bottom(b) {}
    LONG Width()  const { return right - left; }
    LONG Height() const { return bottom - top; }
    CPoint TopLeft()     const { return CPoint(left, top); }
    CPoint BottomRight() const { return CPoint(right, bottom); }
    bool   IsRectEmpty() const { return Width() <= 0 || Height() <= 0; }
    void   SetRectEmpty() { left = top = right = bottom = 0; }
    bool   PtInRect(const CPoint & p) const {
        return p.x >= left && p.x < right && p.y >= top && p.y < bottom;
    }
};

// ─────────────────────────────────────────────────────────────────────
// CTime / CTimeSpan — wraps time_t / chrono::seconds. 113+13 uses.
// ─────────────────────────────────────────────────────────────────────

class CTimeSpan {
public:
    CTimeSpan() = default;
    explicit CTimeSpan(std::time_t s) : m_seconds(s) {}
    CTimeSpan(LONG days, int hours, int mins, int secs)
        : m_seconds(days * 86400LL + hours * 3600LL + mins * 60LL + secs) {}

    LONGLONG GetTotalSeconds() const noexcept { return m_seconds; }
    LONG     GetDays()         const noexcept { return static_cast<LONG>(m_seconds / 86400); }
    LONG     GetTotalHours()   const noexcept { return static_cast<LONG>(m_seconds / 3600); }
    LONG     GetTotalMinutes() const noexcept { return static_cast<LONG>(m_seconds / 60); }
    LONG     GetHours()        const noexcept { return static_cast<LONG>((m_seconds / 3600) % 24); }
    LONG     GetMinutes()      const noexcept { return static_cast<LONG>((m_seconds / 60) % 60); }
    LONG     GetSeconds()      const noexcept { return static_cast<LONG>(m_seconds % 60); }

    bool operator==(const CTimeSpan & rhs) const { return m_seconds == rhs.m_seconds; }
    bool operator!=(const CTimeSpan & rhs) const { return m_seconds != rhs.m_seconds; }
    bool operator< (const CTimeSpan & rhs) const { return m_seconds <  rhs.m_seconds; }
    bool operator> (const CTimeSpan & rhs) const { return m_seconds >  rhs.m_seconds; }

private:
    LONGLONG m_seconds{0};
};

class CTime {
public:
    CTime() = default;
    // MFC CTime allows `t = 0;` to mean "epoch" — int is a common
    // value (`tWhenMatched = 0` in OtherTypes.h's CAlias ctor). Take
    // by value, not std::time_t-only, so int literals don't error.
    CTime(int t) : m_time(static_cast<std::time_t>(t)) {}
    CTime(std::time_t t) : m_time(t) {}
    CTime & operator=(int t) { m_time = static_cast<std::time_t>(t); return *this; }
    CTime & operator=(std::time_t t) { m_time = t; return *this; }
    CTime(int year, int month, int day, int hour, int min, int sec, int /*dst*/ = -1) {
        std::tm tm{};
        tm.tm_year = year - 1900;
        tm.tm_mon  = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min  = min;
        tm.tm_sec  = sec;
        tm.tm_isdst = -1;
        m_time = std::mktime(&tm);
    }

    static CTime GetCurrentTime() { return CTime(std::time(nullptr)); }

    std::time_t GetTime() const noexcept { return m_time; }

    int GetYear()  const { return get_tm().tm_year + 1900; }
    int GetMonth() const { return get_tm().tm_mon + 1; }
    int GetDay()   const { return get_tm().tm_mday; }
    int GetHour()  const { return get_tm().tm_hour; }
    int GetMinute()const { return get_tm().tm_min; }
    int GetSecond()const { return get_tm().tm_sec; }
    int GetDayOfWeek() const { return get_tm().tm_wday + 1; }

    CString Format(const char * fmt) const {
        char buf[256];
        std::tm tm = get_tm();
        std::strftime(buf, sizeof buf, fmt, &tm);
        return CString(buf);
    }

    CTimeSpan operator-(const CTime & rhs) const {
        return CTimeSpan(static_cast<std::time_t>(m_time - rhs.m_time));
    }
    CTime operator+(const CTimeSpan & ts) const {
        return CTime(static_cast<std::time_t>(m_time + ts.GetTotalSeconds()));
    }
    CTime operator-(const CTimeSpan & ts) const {
        return CTime(static_cast<std::time_t>(m_time - ts.GetTotalSeconds()));
    }

    bool operator==(const CTime & rhs) const { return m_time == rhs.m_time; }
    bool operator!=(const CTime & rhs) const { return m_time != rhs.m_time; }
    bool operator< (const CTime & rhs) const { return m_time <  rhs.m_time; }
    bool operator> (const CTime & rhs) const { return m_time >  rhs.m_time; }

private:
    std::tm get_tm() const {
        std::tm tm{};
        std::tm * p = std::localtime(&m_time);
        if (p) tm = *p;
        return tm;
    }
    std::time_t m_time{0};
};

// ─────────────────────────────────────────────────────────────────────
// CObject — base class. MFC uses it for RTTI, serialisation, and
// runtime-class introspection. We're dropping serialisation (CArchive
// handling stays as a stub); for everything else, an empty base is
// enough to keep the inheritance chains compiling.
// ─────────────────────────────────────────────────────────────────────

class CObject {
public:
    CObject() = default;
    virtual ~CObject() = default;

    // RTTI placeholders — drop runtime-class introspection.
    virtual void AssertValid() const {}
    virtual void Dump(class CDumpContext & /*dc*/) const {}

    // No copy by default in MFC; we leave that to derived classes.
    CObject(const CObject &) = delete;
    CObject & operator=(const CObject &) = delete;
};

class CDumpContext {};   // stub — TRACE goes through fprintf instead

// ─────────────────────────────────────────────────────────────────────
// CException — base for MFC exceptions. We bridge to std::exception so
// catch (std::exception &) at the SDL/wx event-loop boundary picks them up.
// ─────────────────────────────────────────────────────────────────────

class CException : public std::exception, public CObject {
public:
    CException() = default;
    virtual BOOL GetErrorMessage(char * buf, UINT max, UINT * /*help*/ = nullptr) {
        if (!buf || max == 0) return FALSE;
        std::strncpy(buf, "CException", max);
        buf[max - 1] = '\0';
        return TRUE;
    }
    void Delete() { delete this; }
    const char * what() const noexcept override { return "CException"; }
};

class CFileException : public CException {
public:
    int m_cause{0};
    int m_lOsError{0};
    char m_strFileName[260]{};
    enum { none, generic, fileNotFound, badPath, tooManyOpenFiles,
           accessDenied, invalidFile, removeCurrentDir, directoryFull,
           badSeek, hardIO, sharingViolation, lockViolation,
           diskFull, endOfFile };
};

class CMemoryException : public CException {};

// ─────────────────────────────────────────────────────────────────────
// Diagnostic macros. ASSERT_VALID is the most-used (130 hits) and is a
// no-op in release builds — we keep it that way unconditionally for
// the port (SPARC builds run un-instrumented).
// ─────────────────────────────────────────────────────────────────────

#ifdef NDEBUG
  #define ASSERT(expr)        ((void)0)
  #define VERIFY(expr)        ((void)(expr))
  #define ASSERT_VALID(p)     ((void)0)
  #define TRACE(...)          ((void)0)
  #define TRACE0(s)           ((void)0)
  #define TRACE1(s, a)        ((void)0)
  #define TRACE2(s, a, b)     ((void)0)
  #define TRACE3(s, a, b, c)  ((void)0)
#else
  #define ASSERT(expr)        assert(expr)
  #define VERIFY(expr)        assert(expr)
  #define ASSERT_VALID(p)     do { if (p) (p)->AssertValid(); } while (0)
  #define TRACE(...)          std::fprintf(stderr, __VA_ARGS__)
  #define TRACE0(s)           std::fprintf(stderr, "%s", (s))
  #define TRACE1(s, a)        std::fprintf(stderr, (s), (a))
  #define TRACE2(s, a, b)     std::fprintf(stderr, (s), (a), (b))
  #define TRACE3(s, a, b, c)  std::fprintf(stderr, (s), (a), (b), (c))
#endif

// MFC message-map markers — `afx_msg` annotates virtual methods that
// participate in the message map. No-op on the port.
#define afx_msg

// Unused-on-Solaris macros that show up in MFC sources we keep.
#define DECLARE_DYNAMIC(cls)
#define DECLARE_DYNCREATE(cls)
#define DECLARE_SERIAL(cls)
#define IMPLEMENT_DYNAMIC(cls, base)
#define IMPLEMENT_DYNCREATE(cls, base)
#define IMPLEMENT_SERIAL(cls, base, ver)
#define DECLARE_MESSAGE_MAP()
#define BEGIN_MESSAGE_MAP(cls, base)  void cls::__msg_map_unused__()
#define END_MESSAGE_MAP()
#define ON_COMMAND(id, fn)
#define ON_UPDATE_COMMAND_UI(id, fn)
#define ON_BN_CLICKED(id, fn)
#define ON_EN_CHANGE(id, fn)
#define ON_LBN_DBLCLK(id, fn)
#define ON_NOTIFY(code, id, fn)
#define ON_WM_CREATE()
#define ON_WM_DESTROY()
#define ON_WM_TIMER()
#define ON_WM_SIZE()
#define ON_WM_PAINT()

// ─────────────────────────────────────────────────────────────────────
// Afx* convenience entry points. The caller-visible behaviour here is
// minimal — a real wxWidgets shell will route AfxMessageBox →
// wxMessageBox at the application layer.
// ─────────────────────────────────────────────────────────────────────

inline int AfxMessageBox(const char * msg, UINT /*type*/ = 0, UINT /*helpid*/ = 0) {
    if (msg) std::fprintf(stderr, "[AfxMessageBox] %s\n", msg);
    return 1;   // IDOK
}
// AfxIsValidString — MFC's "is this a non-null, non-corrupt char*?"
// debug helper. Treat any non-null pointer as valid; the port doesn't
// chase null pointer reads in the same way.
inline BOOL AfxIsValidString(const char * s, int /*len*/ = -1) { return s ? TRUE : FALSE; }
inline BOOL AfxIsValidAddress(const void * p, UINT /*nBytes*/, BOOL /*bReadWrite*/ = TRUE) { return p ? TRUE : FALSE; }
inline int AfxMessageBox(UINT /*string_id*/, UINT /*type*/ = 0, UINT /*helpid*/ = 0) {
    std::fprintf(stderr, "[AfxMessageBox] (string-id resource lookup not implemented)\n");
    return 1;
}
inline class CWinApp * AfxGetApp()      { return nullptr; }
inline HWND  AfxGetMainWnd()            { return nullptr; }
inline HINSTANCE AfxGetInstanceHandle() { return nullptr; }

// CWinApp / CWnd / CCmdTarget — empty bases so MUSHclient.h's
// `class CMUSHclientApp : public CWinApp` compiles. The real
// application logic lands on the wxWidgets-shell adapter; here we
// just need the inheritance chain to type-check so the data-bearing
// .cpp files (doc.cpp, plugins.cpp, scripting/) compile.

class CCmdTarget : public CObject {};
class CWinThread : public CCmdTarget {};
class CWinApp    : public CWinThread {
public:
    CWinApp(const char * /*name*/ = nullptr) {}
    HINSTANCE m_hInstance{nullptr};
    LPCSTR    m_lpCmdLine{nullptr};
    int       m_nCmdShow{0};
    virtual BOOL InitInstance()  { return TRUE; }
    virtual int  ExitInstance()  { return 0; }
    virtual int  Run()           { return 0; }
    virtual BOOL OnIdle(LONG /*lCount*/) { return FALSE; }
};

// CWnd / CView / CDialog / CDocument — minimal placeholders so the
// MUSHclient.h declarations type-check. The corresponding .cpp files
// are on the DELETE list (replaced with wxWidgets) and never reach
// the cross-compiler in real builds; this only matters for headers.
class CWnd       : public CCmdTarget {};
class CFrameWnd  : public CWnd {};
class CMDIChildWnd : public CFrameWnd {};
class CMDIFrameWnd : public CFrameWnd {};
class CDialog    : public CWnd {};
class CDocument  : public CCmdTarget {};
class CView      : public CWnd {};
class CScrollView: public CView {};
class CFormView  : public CView {};
class CCmdUI     : public CObject {};
class CRuntimeClass {};
class CDocTemplate : public CCmdTarget {};
class CMultiDocTemplate : public CDocTemplate {
public:
    CMultiDocTemplate(UINT /*id*/, CRuntimeClass * /*doc*/, CRuntimeClass * /*frame*/, CRuntimeClass * /*view*/) {}
};
class CSingleDocTemplate : public CDocTemplate {};

// Drawing primitives — empty bases for type-checking. Real GUI code
// is rewritten to wxDC etc., not these.
class CDC      : public CObject {};
class CGdiObject : public CObject {};
class CBitmap  : public CGdiObject {};
class CBrush   : public CGdiObject {};
class CPen     : public CGdiObject {};
class CFont    : public CGdiObject {};
class CPalette : public CGdiObject {};
class CRgn     : public CGdiObject {};
class CPaintDC : public CDC {};
class CClientDC: public CDC {};
class CMetaFileDC: public CDC {};
class CImageList : public CObject {};

// Common Controls / Dialog basics — referenced by header chains.
class CStatusBar : public CWnd {};
class CToolBar   : public CWnd {};
class CSplitterWnd : public CWnd {};
class CButton    : public CWnd {};
class CEdit      : public CWnd {};
class CListBox   : public CWnd {};
class CComboBox  : public CWnd {};
class CStatic    : public CWnd {};
class CSocket : public CObject {};
class CAsyncSocket : public CObject {};
class CCriticalSection {
public:
    void Lock() {}
    void Unlock() {}
};

// CFormat / CSystemException have their full definitions in the
// in-tree format.h / exceptions.h. format.cpp / exceptions.cpp now
// pull those in explicitly; everywhere else (headers that mention
// the type by name) takes the forward decl.
class CFormat;
class CSystemException;

// CmcDateTime / CmcDateTimeSpan are defined in the in-tree mcdatetime.h.
// MUSHclient code occasionally pokes COleDateTime* in places that
// would otherwise drag in <afxdisp.h>; alias both to MUSHclient's
// own classes so the type names line up.
class CmcDateTime;
class CmcDateTimeSpan;
using COleDateTime     = CmcDateTime;
using COleDateTimeSpan = CmcDateTimeSpan;

// MSG — Win32 message struct. PreTranslateMessage references it in
// every CWnd-derived class declaration. The MUSHclient port deletes
// all CWnd subclasses, so the parameter type just needs to exist.
struct MSG {
    HWND  hwnd;
    UINT  message;
    DWORD wParam;
    LONG  lParam;
    DWORD time;
    LONG  pt_x;
    LONG  pt_y;
};

// POINT / RECT / SIZE — Win32 POD geometry siblings of CPoint/CRect/CSize.
struct POINT { LONG x; LONG y; };
struct RECT  { LONG left; LONG top; LONG right; LONG bottom; };
struct SIZE  { LONG cx; LONG cy; };

// LARGE_INTEGER — used for high-resolution timers in a couple of files.
union LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; };
    LONGLONG QuadPart;
};

// SYSTEMTIME — Win32 broken-down time struct. Same idea as struct
// std::tm, but with field order/widths that GetLocalTime / SetLocalTime
// pin down. Provide the struct + GetLocalTime stub for mcdatetime.cpp.
struct SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
};
inline void GetLocalTime(SYSTEMTIME * st) {
    if (!st) return;
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    std::tm tm{};
    std::tm * p = std::localtime(&ts.tv_sec);
    if (p) tm = *p;
    st->wYear         = static_cast<WORD>(tm.tm_year + 1900);
    st->wMonth        = static_cast<WORD>(tm.tm_mon + 1);
    st->wDayOfWeek    = static_cast<WORD>(tm.tm_wday);
    st->wDay          = static_cast<WORD>(tm.tm_mday);
    st->wHour         = static_cast<WORD>(tm.tm_hour);
    st->wMinute       = static_cast<WORD>(tm.tm_min);
    st->wSecond       = static_cast<WORD>(tm.tm_sec);
    st->wMilliseconds = static_cast<WORD>(ts.tv_nsec / 1000000);
}
inline void GetSystemTime(SYSTEMTIME * st) { GetLocalTime(st); }   // close enough for the port

// Win32 message-base + a small handful of constants the codebase
// references directly. WM_APP marks user-defined messages above
// 0x8000; MUSHclient builds custom message IDs from it.
#define WM_APP    0x8000
#define WM_USER   0x0400

// QueryPerformanceFrequency / QueryPerformanceCounter — Win32 high-
// resolution timer. Map to clock_gettime(CLOCK_MONOTONIC). Frequency
// is reported as nanoseconds-per-second since clock_gettime returns
// nanoseconds, so QPF returns 1e9 and QPC returns ns since epoch.
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER * f) {
    if (!f) return FALSE;
    f->QuadPart = 1000000000LL;
    return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER * c) {
    if (!c) return FALSE;
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    c->QuadPart = static_cast<LONGLONG>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    return TRUE;
}
inline DWORD GetTickCount() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<DWORD>(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

// ─────────────────────────────────────────────────────────────────────
// CFile / CStdioFile — minimal wrapper around fopen/fclose. CArchive
// (61 uses, MFC binary serialisation) is left as a forward decl: any
// .cpp that uses it needs explicit conversion to a different
// persistence format (probably JSON or a flat binary). Marking this
// for the next pass.
// ─────────────────────────────────────────────────────────────────────

class CFile : public CObject {
public:
    enum OpenFlags {
        modeRead         = 0x0000,
        modeWrite        = 0x0001,
        modeReadWrite    = 0x0002,
        shareCompat      = 0x0000,
        shareExclusive   = 0x0010,
        shareDenyWrite   = 0x0020,
        shareDenyRead    = 0x0030,
        shareDenyNone    = 0x0040,
        modeNoInherit    = 0x0080,
        modeCreate       = 0x1000,
        modeNoTruncate   = 0x2000,
        typeText         = 0x4000,
        typeBinary       = (int)0x8000
    };
    enum SeekPosition { begin = 0, current = 1, end = 2 };

    CFile() = default;
    virtual ~CFile() { if (m_fp) std::fclose(m_fp); }

    virtual BOOL Open(const char * path, UINT flags, CFileException * /*err*/ = nullptr) {
        const char * mode = "rb";
        if (flags & modeWrite)     mode = (flags & modeNoTruncate) ? "ab" : "wb";
        if (flags & modeReadWrite) mode = "r+b";
        if ((flags & modeCreate) && !(flags & modeNoTruncate)) mode = "w+b";
        m_fp = std::fopen(path, mode);
        if (m_fp) m_path = path;
        return m_fp ? TRUE : FALSE;
    }
    virtual UINT Read(void * buf, UINT n) {
        return m_fp ? static_cast<UINT>(std::fread(buf, 1, n, m_fp)) : 0;
    }
    virtual void Write(const void * buf, UINT n) {
        if (m_fp) std::fwrite(buf, 1, n, m_fp);
    }
    virtual ULONGLONG Seek(LONGLONG off, UINT origin) {
        if (!m_fp) return 0;
        std::fseek(m_fp, static_cast<long>(off),
                   origin == begin ? SEEK_SET : origin == current ? SEEK_CUR : SEEK_END);
        return static_cast<ULONGLONG>(std::ftell(m_fp));
    }
    virtual ULONGLONG GetLength() const {
        if (!m_fp) return 0;
        long here = std::ftell(m_fp);
        std::fseek(m_fp, 0, SEEK_END);
        long size = std::ftell(m_fp);
        std::fseek(m_fp, here, SEEK_SET);
        return static_cast<ULONGLONG>(size);
    }
    virtual void Close() {
        if (m_fp) { std::fclose(m_fp); m_fp = nullptr; }
    }
    CString GetFilePath() const { return m_path; }

protected:
    std::FILE * m_fp{nullptr};
    CString     m_path;
};

class CStdioFile : public CFile {
public:
    CStdioFile() = default;
    BOOL ReadString(CString & line) {
        if (!m_fp) return FALSE;
        char buf[4096];
        if (!std::fgets(buf, sizeof buf, m_fp)) return FALSE;
        std::size_t n = std::strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        line = buf;
        return TRUE;
    }
    void WriteString(const char * s) {
        if (m_fp && s) std::fputs(s, m_fp);
    }
};

// CArchive — placeholder. Using it forces a TODO during port.
class CArchive {
public:
    CArchive() = delete;        // refuse instantiation; rewrite call sites to JSON or binary I/O
};

// ─────────────────────────────────────────────────────────────────────
// CMap-style associative containers — thin wrappers around std::map.
// MUSHclient's usage is mainly CMapStringToPtr / CTypedPtrMap with
// CString keys. We expose the subset of API actually called.
//
// The full MFC CMap takes <KEY, ARG_KEY, VALUE, ARG_VALUE> — too
// templated to drag along verbatim. The three typedefs below cover
// the call sites in the codebase.
// ─────────────────────────────────────────────────────────────────────

template <typename Key, typename Value>
class CMapBase {
public:
    using map_t = std::map<Key, Value>;
    map_t m_map;

    int  GetCount() const                     { return static_cast<int>(m_map.size()); }
    BOOL IsEmpty() const                      { return m_map.empty() ? TRUE : FALSE; }
    void RemoveAll()                          { m_map.clear(); }
    BOOL RemoveKey(const Key & k)             { return m_map.erase(k) ? TRUE : FALSE; }
    void SetAt(const Key & k, const Value & v){ m_map[k] = v; }
    BOOL Lookup(const Key & k, Value & v) const {
        auto it = m_map.find(k);
        if (it == m_map.end()) return FALSE;
        v = it->second;
        return TRUE;
    }
    Value & operator[](const Key & k) { return m_map[k]; }

    // MFC CMap API — InitHashTable is a no-op on std::map (it's a
    // tree, not a hash); ports that pass a bucket-count are ignored.
    void InitHashTable(unsigned int /*hashSize*/, BOOL = TRUE) {}

    // POSITION-walk subset. POSITION on MFC is an opaque token. We
    // back it with a heap-allocated iterator so the get/next pair
    // can be implemented without exposing iterator state through the
    // type system. GetNextAssoc copies key+value out and advances.
    using POSITION_t = void *;
    POSITION_t GetStartPosition() const {
        if (m_map.empty()) return nullptr;
        return new typename map_t::const_iterator(m_map.begin());
    }
    void GetNextAssoc(POSITION_t & rPos, Key & rKey, Value & rValue) const {
        auto * itp = static_cast<typename map_t::const_iterator *>(rPos);
        rKey   = (*itp)->first;
        rValue = (*itp)->second;
        ++(*itp);
        if (*itp == m_map.end()) {
            delete itp;
            rPos = nullptr;
        }
    }
};

using CMapStringToString = CMapBase<std::string, std::string>;
using CMapStringToPtr    = CMapBase<std::string, void *>;

template <typename T>
using CTypedPtrMap_StringToOb = CMapBase<std::string, T *>;

// CTypedPtrMap<BASE, KEY, VALUE> — canonical MFC 3-arg signature.
// Used primarily by xml/xmlparse.h's CAttributeMap and a couple of
// places that map CString → some pointer. Stores via std::map<KEY,
// VALUE> internally; ignores the BASE arg.
template <class BASE_CLASS, class KEY, class VALUE>
class CTypedPtrMap : public BASE_CLASS {
public:
    using map_t = std::map<KEY, VALUE>;
    map_t m_map;

    int  GetCount() const                     { return static_cast<int>(m_map.size()); }
    BOOL IsEmpty() const                      { return m_map.empty() ? TRUE : FALSE; }
    void RemoveAll()                          { m_map.clear(); }
    BOOL RemoveKey(const KEY & k)             { return m_map.erase(k) ? TRUE : FALSE; }
    void SetAt(const KEY & k, const VALUE & v){ m_map[k] = v; }
    BOOL Lookup(const KEY & k, VALUE & v) const {
        auto it = m_map.find(k);
        if (it == m_map.end()) return FALSE;
        v = it->second;
        return TRUE;
    }
    VALUE & operator[](const KEY & k) { return m_map[k]; }
    void InitHashTable(unsigned int /*hashSize*/, BOOL = TRUE) {}
};

// CPtrArray + CTypedPtrArray — vector counterpart to CTypedPtrMap.
// MUSHclient typedef sites read:
//   typedef CTypedPtrArray <CPtrArray, CFoo*> CFooArray;
class CPtrArray : public CObject {};

template <class BASE_CLASS, class T>
class CTypedPtrArray : public BASE_CLASS {
public:
    std::vector<T> m_arr;

    int  GetSize() const                     { return static_cast<int>(m_arr.size()); }
    int  GetCount() const                    { return static_cast<int>(m_arr.size()); }
    BOOL IsEmpty() const                     { return m_arr.empty() ? TRUE : FALSE; }
    void RemoveAll()                         { m_arr.clear(); }
    int  Add(const T & v)                    { m_arr.push_back(v); return static_cast<int>(m_arr.size()) - 1; }
    void SetAt(int i, const T & v)           { m_arr[i] = v; }
    void SetAtGrow(int i, const T & v) {
        if (static_cast<std::size_t>(i) >= m_arr.size())
            m_arr.resize(static_cast<std::size_t>(i) + 1);
        m_arr[i] = v;
    }
    void RemoveAt(int i)                     { m_arr.erase(m_arr.begin() + i); }
    T &       GetAt(int i)                   { return m_arr[i]; }
    const T & GetAt(int i) const             { return m_arr[i]; }
    T &       operator[](int i)              { return m_arr[i]; }
    const T & operator[](int i) const        { return m_arr[i]; }
};

// Scripting-arrays typedefs. Used in plugins.h and doc.h:
//   tStringMapOfMaps m_Arrays;
// where each value is a pointer to a string→string map.  The
// underlying types come from MUSHclient's Win32 build via some
// header we don't have a copy of; the iterator-shape used by the
// scripting/methods/methods_arrays.cpp call sites makes the
// definitions unambiguous (they iterate (begin,end), call find(),
// emplace via insert).
typedef std::map<std::string, std::string> tStringToStringMap;
typedef std::map<std::string, tStringToStringMap *> tStringMapOfMaps;

// ─────────────────────────────────────────────────────────────────────
// CList — std::list wrapper.
// ─────────────────────────────────────────────────────────────────────

template <typename T>
class CList {
public:
    using list_t = std::list<T>;
    list_t m_list;

    int  GetCount() const     { return static_cast<int>(m_list.size()); }
    BOOL IsEmpty()  const     { return m_list.empty() ? TRUE : FALSE; }
    void AddHead(const T & v) { m_list.push_front(v); }
    void AddTail(const T & v) { m_list.push_back(v); }
    T    RemoveHead()         { T v = m_list.front(); m_list.pop_front(); return v; }
    T    RemoveTail()         { T v = m_list.back();  m_list.pop_back();  return v; }
    void RemoveAll()          { m_list.clear(); }
    T &  GetHead()            { return m_list.front(); }
    T &  GetTail()            { return m_list.back(); }
};

template <typename T>
using CTypedPtrList_PtrList = CList<T *>;

// CPtrList — generic MFC pointer list, used as the BASE_CLASS argument
// to CTypedPtrList<BASE_CLASS, T> typedefs. We just need the type to
// exist; the template specialization below ignores the BASE_CLASS arg.
class CPtrList : public CObject {};

// CTypedPtrList<BASE_CLASS, T> — the canonical MFC signature.
// MUSHclient typedefs typically read:
//   typedef CTypedPtrList<CPtrList, CFoo*> CFooList;
// We accept any base and back the storage with std::list<T>.
template <class BASE_CLASS, class T>
class CTypedPtrList : public BASE_CLASS {
public:
    std::list<T> m_list;
    int  GetCount() const     { return static_cast<int>(m_list.size()); }
    BOOL IsEmpty()  const     { return m_list.empty() ? TRUE : FALSE; }
    void AddHead(const T & v) { m_list.push_front(v); }
    void AddTail(const T & v) { m_list.push_back(v); }
    T    RemoveHead()         { T v = m_list.front(); m_list.pop_front(); return v; }
    T    RemoveTail()         { T v = m_list.back();  m_list.pop_back();  return v; }
    void RemoveAll()          { m_list.clear(); }
    T &  GetHead()            { return m_list.front(); }
    T &  GetTail()            { return m_list.back(); }
};

// CStringArray — std::vector<CString> wrapper. ~Half a dozen call
// sites in NameGeneration.cpp + the names/ directory.
class CStringArray {
public:
    std::vector<CString> m_arr;

    int  GetSize() const                     { return static_cast<int>(m_arr.size()); }
    int  GetCount() const                    { return static_cast<int>(m_arr.size()); }
    BOOL IsEmpty() const                     { return m_arr.empty() ? TRUE : FALSE; }
    void RemoveAll()                         { m_arr.clear(); }
    void Add(const CString & s)              { m_arr.push_back(s); }
    void SetAtGrow(int i, const CString & s) {
        if (static_cast<std::size_t>(i) >= m_arr.size())
            m_arr.resize(static_cast<std::size_t>(i) + 1);
        m_arr[i] = s;
    }
    void SetAt(int i, const CString & s)     { m_arr[i] = s; }
    void RemoveAt(int i)                     { m_arr.erase(m_arr.begin() + i); }
    CString &       operator[](int i)        { return m_arr[i]; }
    const CString & operator[](int i) const  { return m_arr[i]; }
    CString &       GetAt(int i)             { return m_arr[i]; }
    const CString & GetAt(int i) const       { return m_arr[i]; }
};

// DATE — Win32 OLE date/time = double-encoded (days since 1899-12-30).
// MUSHclient's mcdatetime.cpp passes DATE& around when interfacing
// with the OS automation layer; on the port it's just a double.
using DATE = double;

// FormatMessage — Win32 system error → human string. The MUSHclient
// usage in exceptions.cpp formats GetLastError() output; on the port
// we route to strerror(3). Constants are stubs; FormatMessageA is a
// thin shim that fills the buffer.
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define LANG_NEUTRAL                   0x00
#define SUBLANG_DEFAULT                0x01
#define MAKELANGID(p, s)               ((WORD)(((WORD)(s) << 10) | (WORD)(p)))

inline DWORD FormatMessageA(
    DWORD /*dwFlags*/, LPCVOID /*lpSource*/, DWORD dwMessageId,
    DWORD /*dwLanguageId*/, LPSTR lpBuffer, DWORD nSize,
    void * /*Arguments*/ = nullptr)
{
    if (!lpBuffer || nSize == 0) return 0;
    const char * msg = std::strerror(static_cast<int>(dwMessageId));
    std::strncpy(lpBuffer, msg ? msg : "", nSize);
    lpBuffer[nSize - 1] = '\0';
    return static_cast<DWORD>(std::strlen(lpBuffer));
}
#define FormatMessage FormatMessageA

// LocalFree — Win32 heap free; FormatMessage with ALLOCATE_BUFFER
// stashes the buffer pointer and the caller frees it via LocalFree.
inline void * LocalFree(void * p) { std::free(p); return nullptr; }

// GetLastError — Win32 thread-local error code. We map to errno.
inline DWORD GetLastError() { return static_cast<DWORD>(errno); }
inline void  SetLastError(DWORD code) { errno = static_cast<int>(code); }

#endif // MFC_SHIM_H
