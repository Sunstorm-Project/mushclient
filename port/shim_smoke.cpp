// port/shim_smoke.cpp — exercise mfc_shim.h end-to-end so we know the
// header compiles and behaves correctly before pointing real MUSHclient
// sources at it.
//
// Build (Mac, native):
//   g++ -std=c++17 -Wall -Wextra port/shim_smoke.cpp -o /tmp/shim_smoke
//   /tmp/shim_smoke
//
// Cross-compile for SPARC Solaris 7:
//   docker compose -f cross-build-userland/docker-compose.yml \
//     -f local-dev/compose.override.yml run --rm \
//     --entrypoint /bin/bash userland-build -c \
//     'sparc-sun-solaris2.7-g++ --sysroot=/opt/sysroot-build -std=c++17 \
//       /opt/mushclient/port/shim_smoke.cpp -o /opt/output/shim_smoke'
//   (mount mushclient into the container first, or run from inside.)

#include "mfc_shim.h"
#include <cassert>
#include <cstdio>
#include <unistd.h>

static void test_cstring() {
    CString s("hello");
    assert(s.GetLength() == 5);
    assert(!s.IsEmpty());
    assert(s.Left(3) == "hel");
    assert(s.Right(3) == "llo");
    assert(s.Mid(1, 3) == "ell");

    s += " world";
    assert(s == "hello world");

    CString upper = s;
    upper.MakeUpper();
    assert(upper == "HELLO WORLD");

    CString f;
    f.Format("%d/%s", 42, "answers");
    assert(f == "42/answers");

    CString r("aaaXaaa");
    int n = r.Replace("X", "YYY");
    assert(n == 1 && r == "aaaYYYaaa");

    CString tr("   spaced   ");
    tr.TrimLeft(); tr.TrimRight();
    assert(tr == "spaced");

    CString eq1("apple"), eq2("banana");
    assert(eq1 != eq2);
    assert(eq1.CompareNoCase("APPLE") == 0);

    char * buf = f.GetBuffer(20);
    std::strcpy(buf, "rebuilt");
    f.ReleaseBuffer();
    assert(f == "rebuilt");

    std::printf("CString OK (len=%d, sample='%s')\n", (int)s.GetLength(), (const char *)s);
}

static void test_geometry() {
    CRect r(10, 20, 110, 220);
    assert(r.Width() == 100 && r.Height() == 200);
    assert(r.PtInRect(CPoint(50, 50)));
    assert(!r.PtInRect(CPoint(0, 0)));
    CSize sz(8, 16);
    assert(sz.cx == 8 && sz.cy == 16);
    std::printf("CRect/CPoint/CSize OK\n");
}

static void test_time() {
    CTime now = CTime::GetCurrentTime();
    assert(now.GetYear() >= 1970);
    CTime t(2026, 1, 1, 0, 0, 0);
    CTimeSpan d(7, 0, 0, 0);
    CTime later = t + d;
    assert(later - t == CTimeSpan(7 * 86400));
    std::printf("CTime OK (year=%d)\n", now.GetYear());
}

static void test_macros() {
    ASSERT(1 + 1 == 2);
    VERIFY(1 + 1 == 2);
    TRACE("trace fires: %s\n", "ok");
    AfxMessageBox("smoke test message");
    COLORREF c = RGB(0xAA, 0xBB, 0xCC);
    assert(GetRValue(c) == 0xAA && GetGValue(c) == 0xBB && GetBValue(c) == 0xCC);
    std::printf("Diagnostic macros OK (RGB=0x%06x)\n", (unsigned)c);
}

static void test_containers() {
    CMapStringToString m;
    m.SetAt("alpha", "1");
    m.SetAt("beta",  "2");
    std::string v;
    assert(m.Lookup("alpha", v) && v == "1");
    assert(m.GetCount() == 2);

    CList<int> l;
    l.AddTail(10);
    l.AddTail(20);
    l.AddHead(5);
    assert(l.GetCount() == 3);
    assert(l.RemoveHead() == 5);
    std::printf("CMap/CList OK\n");
}

static void test_file() {
    CFile f;
    char tmpname[] = "/tmp/shim_smoke_XXXXXX";
    int fd = mkstemp(tmpname);
    if (fd >= 0) close(fd);
    BOOL ok = f.Open(tmpname, CFile::modeCreate | CFile::modeWrite);
    assert(ok);
    f.Write("payload", 7);
    f.Close();
    ok = f.Open(tmpname, CFile::modeRead);
    assert(ok);
    char buf[16]{};
    UINT n = f.Read(buf, 7);
    assert(n == 7 && std::strncmp(buf, "payload", 7) == 0);
    f.Close();
    std::remove(tmpname);
    std::printf("CFile OK\n");
}

int main() {
    test_cstring();
    test_geometry();
    test_time();
    test_macros();
    test_containers();
    test_file();
    std::printf("\nAll mfc_shim.h smoke checks passed.\n");
    return 0;
}
