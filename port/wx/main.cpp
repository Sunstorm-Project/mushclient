// port/wx/main.cpp — wxWidgets/X11 shell for the SPARC Solaris 7 port
// of nickgammon/mushclient.
//
// First-principles GUI: wxApp + wxFrame split into an output pane
// (custom-drawn wxScrolledWindow) on top and an input wxTextCtrl on
// bottom, with a menubar and statusbar. No wxRichTextCtrl — the
// wxX11/wxUniversal backend has known issues with it on Solaris 7.
//
// This is the bring-up shell — it does NOT yet wire up the MUSHclient
// document/socket layer. Goal for v0.1: clickable, visible, types
// echo into the output pane locally. Once that runs on the QEMU
// framebuffer we can grow it into a real client.

#include <wx/wx.h>
#include <wx/splitter.h>
#include <wx/textctrl.h>
#include <wx/menu.h>
#include <wx/statusbr.h>
#include <wx/font.h>
#include <wx/dcclient.h>
#include <wx/scrolwin.h>
#include <wx/socket.h>
#include <wx/textdlg.h>
#include <string>

#ifdef __WXUNIVERSAL__
// Force-link wxUniversal themes. Without these, the static archive
// `libwx_x11univu-3.0.a` carries the theme classes but no theme .o
// is referenced from app code, so the linker drops them and
// wxTheme::CreateDefault() reports "no built-in themes found" (see
// the comment on WX_USE_THEME in wx/univ/theme.h: "without it, an
// over optimizing linker may discard the object module containing
// the theme implementation entirely"). On the Motif build wx uses
// native libXm widgets directly — no theme registration needed.
#include <wx/univ/theme.h>
WX_USE_THEME(win32);
WX_USE_THEME(gtk);
WX_USE_THEME(mono);
#endif

#include <cstddef>
#include <cstdlib>
#include <vector>

// ─────────────────────────────────────────────────────────────────────
// AnsiSpan — one stretch of text sharing a single foreground colour
// and bold attribute. Lines in the OutputPane are sequences of spans.
// We don't render background colour yet; the entire pane stays
// black-on-coloured.
// ─────────────────────────────────────────────────────────────────────

struct AnsiSpan {
    wxString text;
    wxColour fg;
    bool     bold;
};

// Standard ANSI palette, indexed by code 30-37 (foreground).
// Use full-saturation colours rather than the more subtle 0xAA
// half-strengths because the Solaris 7 framebuffer is 8-bit
// PseudoColor on TCX — wx's nearest-colormap-entry allocation
// for any non-primary RGB triple often falls back to black or
// near-black, making text invisible. Pure 0xFF / 0x00 channels
// are guaranteed to map to the static-colour entries the X11
// server installs at Xsun startup.
static wxColour AnsiPaletteColour(int idx, bool bold) {
    static const unsigned char palette[8][3] = {
        {0x00,0x00,0x00}, // 0 black
        {0xFF,0x00,0x00}, // 1 red
        {0x00,0xFF,0x00}, // 2 green
        {0xFF,0xFF,0x00}, // 3 yellow
        {0x00,0x00,0xFF}, // 4 blue
        {0xFF,0x00,0xFF}, // 5 magenta
        {0x00,0xFF,0xFF}, // 6 cyan
        {0xFF,0xFF,0xFF}, // 7 white
    };
    if (idx < 0 || idx > 7) idx = 7;
    // BLACK on a black background would be invisible. Bump to grey
    // — but on 8-bit PseudoColor, grey is risky too; use pale yellow
    // which has reliably-allocated palette entries on Xsun.
    if (idx == 0) return bold ? wxColour(0xC0, 0xC0, 0xC0)
                              : wxColour(0x80, 0x80, 0x80);
    const unsigned char * p = palette[idx];
    return wxColour(p[0], p[1], p[2]);
}

// ─────────────────────────────────────────────────────────────────────
// AnsiTelnetParser — feeds raw socket bytes through a state machine,
// splits on '\n', strips telnet IAC sequences (we don't reply to
// negotiations yet — most MUDs accept silence as "WONT"), and
// converts ANSI SGR escapes into per-span colour/bold metadata.
//
// The parser keeps state across Feed() calls so a chunk that splits
// mid-escape or mid-line works correctly.
// ─────────────────────────────────────────────────────────────────────

class AnsiTelnetParser {
public:
    enum Phase {
        P_NORMAL,
        P_ESC,
        P_CSI,
        P_IAC,
        P_IAC_OPT,
        P_IAC_SB,
        P_IAC_SB_IAC,
    };

    // Feed n bytes; for each completed '\n'-terminated line, call
    // emit(std::vector<AnsiSpan> &&). Carriage returns are stripped.
    template <typename Emit>
    void Feed(const char * data, std::size_t n, Emit emit) {
        for (std::size_t i = 0; i < n; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            switch (m_phase) {
                case P_NORMAL:
                    if (c == 0x1B)      { Flush(); m_phase = P_ESC; }
                    else if (c == 0xFF) { Flush(); m_phase = P_IAC; }
                    else if (c == '\r') { /* swallow */ }
                    else if (c == '\n') {
                        Flush();
                        emit(std::move(m_curLine));
                        m_curLine.clear();
                    } else {
                        m_cur.append(reinterpret_cast<const char *>(&c), 1);
                    }
                    break;
                case P_ESC:
                    if (c == '[') { m_phase = P_CSI; m_csiParams.clear(); }
                    else          { m_phase = P_NORMAL; }
                    break;
                case P_CSI:
                    if ((c >= '0' && c <= '9') || c == ';') {
                        m_csiParams.append(reinterpret_cast<const char *>(&c), 1);
                    } else {
                        if (c == 'm') ApplySGR();
                        m_phase = P_NORMAL;
                    }
                    break;
                case P_IAC:
                    // 251 WILL  252 WONT  253 DO  254 DONT  → option byte follows
                    if (c >= 251 && c <= 254) m_phase = P_IAC_OPT;
                    else if (c == 250)        m_phase = P_IAC_SB;     // SB → subneg
                    else                      m_phase = P_NORMAL;     // SE/NOP/etc
                    break;
                case P_IAC_OPT:
                    m_phase = P_NORMAL;       // eat the option byte
                    break;
                case P_IAC_SB:
                    if (c == 0xFF) m_phase = P_IAC_SB_IAC;            // saw IAC inside SB
                    // else stay in subneg (data byte) — ignored
                    break;
                case P_IAC_SB_IAC:
                    // either SE (0xF0, end of subneg) or escaped 0xFF in subneg data;
                    // in both cases return to NORMAL to keep the parser tractable.
                    m_phase = P_NORMAL;
                    break;
            }
        }
    }

    // Snapshot the in-progress line (m_curLine + any partial m_cur)
    // for rendering as a "pending" line below the committed output.
    // Used so MUD prompts and banner-without-trailing-newline are
    // visible immediately, not held until the next \n.
    std::vector<AnsiSpan> Pending() const {
        std::vector<AnsiSpan> out = m_curLine;
        if (!m_cur.empty()) {
            AnsiSpan span;
            span.text = wxString::FromUTF8(m_cur.c_str(), m_cur.size());
            span.fg   = AnsiPaletteColour(m_fgIndex, m_bold);
            span.bold = m_bold;
            out.push_back(std::move(span));
        }
        return out;
    }

private:
    void Flush() {
        if (m_cur.empty()) return;
        AnsiSpan span;
        span.text = wxString::FromUTF8(m_cur.c_str(), m_cur.size());
        span.fg   = AnsiPaletteColour(m_fgIndex, m_bold);
        span.bold = m_bold;
        m_curLine.push_back(std::move(span));
        m_cur.clear();
    }

    void ApplySGR() {
        std::vector<int> codes;
        std::string token;
        for (char c : m_csiParams) {
            if (c == ';') {
                if (!token.empty()) codes.push_back(std::atoi(token.c_str()));
                token.clear();
            } else {
                token += c;
            }
        }
        if (!token.empty()) codes.push_back(std::atoi(token.c_str()));
        if (codes.empty())  codes.push_back(0);   // ESC[m == ESC[0m

        for (int code : codes) {
            if      (code == 0)                  { m_bold = false; m_fgIndex = 7; }
            else if (code == 1)                  { m_bold = true; }
            else if (code == 22)                 { m_bold = false; }
            else if (code >= 30 && code <= 37)   { m_fgIndex = code - 30; }
            else if (code == 39)                 { m_fgIndex = 7; }
            // background (40-47, 49) ignored for v0.3 — we draw on
            // a fixed-black pane for now
        }
    }

    Phase                  m_phase{P_NORMAL};
    std::string            m_csiParams;
    std::string            m_cur;
    std::vector<AnsiSpan>  m_curLine;
    bool                   m_bold{false};
    int                    m_fgIndex{7};   // default = white
};

// ─────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────
// OutputPane — wxTextCtrl-backed read-only view of MUD output.
//
// Empirical finding 2026-05-07: wxX11 / wxUniversal's wxPaintDC::
// DrawText silently produces zero pixels for our custom-drawn pane
// regardless of fg/bg/font/SetBackgroundMode/SetForegroundColour/
// font-face/explicit-Courier/wxNORMAL_FONT/warm-up-DrawRectangle.
// DrawRectangle on the same DC works fine. The wx widgets that
// render text correctly (menubar / statusbar / dialogs / wxTextCtrl
// itself) all go through the wxRenderer-based widget text path,
// not the DC text path.
//
// So we back the OutputPane with a real wxTextCtrl. We lose per-
// span colour rendering — wxTextCtrl can't multi-attribute text
// without wxRichTextCtrl which the original handoff already
// flagged as "renders poorly on wxX11". For a first usable MUD
// client, plain monochrome MUD output is far better than the
// invisible-everything we'd get from the DC path.
//
// AnsiSpan / SetPendingLine API stays so the parser → output
// flow doesn't have to change. Per-span colour is flattened to
// concatenated text.
// ─────────────────────────────────────────────────────────────────────

class OutputPane : public wxTextCtrl {
public:
    OutputPane(wxWindow * parent)
        : wxTextCtrl(parent, wxID_ANY, wxEmptyString,
                     wxDefaultPosition, wxDefaultSize,
                     wxTE_READONLY | wxTE_MULTILINE |
                     wxTE_DONTWRAP | wxBORDER_SUNKEN)
    {
        // Use system defaults (white bg, black text) for visibility.
        // SetBackgroundColour on wxX11 wxTextCtrl doesn't recolour
        // the text-area background — it's painted by the widget's
        // own theme. Setting it black combined with default-black
        // text would render invisibly. Keep default and let the
        // user see something.
    }

    void AppendLine(const wxString & s) {
        AppendText(s);
        AppendText(wxT("\n"));
        SetInsertionPointEnd();   // auto-scroll
    }

    void AppendStyledLine(std::vector<AnsiSpan> && spans) {
        wxString line;
        for (const AnsiSpan & span : spans) line += span.text;
        AppendLine(line);
    }

    // wxTextCtrl can't render an "in-progress" line that gets
    // replaced as bytes arrive. For now ignore the pending state;
    // partial server output (no trailing \n) waits for the next \n
    // to commit.
    void SetPendingLine(std::vector<AnsiSpan> /*spans*/) {}
};


// ─────────────────────────────────────────────────────────────────────
// Frame — top-level window. Splitter divides output (top) from input
// (bottom). Menubar + status bar.
// ─────────────────────────────────────────────────────────────────────

enum {
    ID_File_Quit = wxID_EXIT,
    ID_Help_About = wxID_ABOUT,
    ID_Input_Send = wxID_HIGHEST + 1,
    ID_File_Connect,
    ID_File_Disconnect,
    ID_Socket,
};

class MainFrame : public wxFrame {
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, wxT("MUSHclient — SPARC Solaris 7 port"),
                  wxDefaultPosition, wxSize(880, 600))
    {
        BuildMenuBar();
        BuildStatusBar();
        BuildSplit();
        SetMinSize(wxSize(480, 360));
    }

private:
    void BuildMenuBar() {
        wxMenu * file = new wxMenu;
        file->Append(ID_File_Connect,    wxT("&Connect...\tCtrl+N"), wxT("Connect to a MUD server"));
        file->Append(ID_File_Disconnect, wxT("&Disconnect"),         wxT("Drop the current connection"));
        file->AppendSeparator();
        file->Append(ID_File_Quit,       wxT("E&xit\tCtrl+Q"),       wxT("Close MUSHclient"));
        wxMenu * help = new wxMenu;
        help->Append(ID_Help_About, wxT("&About...\tF1"), wxT("About MUSHclient SPARC port"));
        wxMenuBar * mb = new wxMenuBar;
        mb->Append(file, wxT("&File"));
        mb->Append(help, wxT("&Help"));
        SetMenuBar(mb);
    }

    void BuildStatusBar() {
        wxStatusBar * sb = CreateStatusBar(2);
        int widths[2] = { -3, -1 };
        sb->SetStatusWidths(2, widths);
        sb->SetStatusText(wxT("not connected"), 0);
        sb->SetStatusText(wxT("v0.5-port"), 1);
    }

    void BuildSplit() {
        wxSplitterWindow * split = new wxSplitterWindow(this, wxID_ANY,
            wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);

        m_output = new OutputPane(split);
        m_input  = new wxTextCtrl(split, ID_Input_Send,
                                  wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER);
        split->SplitHorizontally(m_output, m_input, -120);
        split->SetMinimumPaneSize(60);

        m_output->AppendLine(wxT("MUSHclient SPARC Solaris 7 port — v0.5 shell"));
        m_output->AppendLine(wxT("File -> Connect... to dial a MUD; lines you type"));
        m_output->AppendLine(wxT("are sent to the server. ANSI colour / triggers /"));
        m_output->AppendLine(wxT("aliases / scripting are still TODO — bring-up only."));
        m_output->AppendLine(wxEmptyString);

        m_input->SetFocus();
    }

    void OnQuit(wxCommandEvent &)  { Close(true); }
    void OnAbout(wxCommandEvent &) {
        wxMessageBox(wxT("MUSHclient SPARC Solaris 7 port\n"
                         "wxWidgets/X11 shell — v0.5\n\n"
                         "Connect / Disconnect via the File menu.\n"
                         "Lines from the server land in the output pane.\n"
                         "Lines you type in the input box go to the server.\n\n"
                         "Sunstorm-Project/mushclient feat/sparc-solaris7-port"),
                     wxT("About"), wxOK | wxICON_INFORMATION, this);
    }

    void OnConnect(wxCommandEvent &) {
        if (m_socket && m_socket->IsConnected()) {
            wxMessageBox(wxT("Already connected — disconnect first."),
                         wxT("Connect"), wxOK, this);
            return;
        }
        // Default to Aardwolf as a recognisable MUSH-style sandbox.
        wxString hp = wxGetTextFromUser(
            wxT("Enter host:port (e.g. aardmud.org:23)"),
            wxT("Connect to MUD"),
            m_lastHostPort.IsEmpty() ? wxT("aardmud.org:23") : m_lastHostPort,
            this);
        if (hp.IsEmpty()) return;
        m_lastHostPort = hp;

        wxString host, portstr;
        const int colon = hp.Find(':', true /*from end*/);
        if (colon == wxNOT_FOUND) {
            host = hp;
            portstr = wxT("23");
        } else {
            host    = hp.Mid(0, colon);
            portstr = hp.Mid(colon + 1);
        }
        long port = 0;
        if (!portstr.ToLong(&port) || port <= 0 || port > 65535) {
            wxMessageBox(wxT("Bad port number."), wxT("Connect"),
                         wxOK | wxICON_ERROR, this);
            return;
        }

        wxIPV4address addr;
        addr.Hostname(host);
        addr.Service(static_cast<unsigned short>(port));

        if (m_socket) { m_socket->Destroy(); m_socket = nullptr; }
        m_socket = new wxSocketClient(wxSOCKET_NOWAIT);
        m_socket->SetEventHandler(*this, ID_Socket);
        m_socket->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG | wxSOCKET_CONNECTION_FLAG);
        m_socket->Notify(true);
        m_parser = AnsiTelnetParser{};   // reset parser state per connection

        m_output->AppendLine(wxString::Format(wxT("Connecting to %s:%ld..."), host, port));
        m_socket->Connect(addr, false);  // async — events come back via OnSocketEvent
    }

    void OnDisconnect(wxCommandEvent &) {
        if (m_socket) {
            m_socket->Close();
            m_socket->Destroy();
            m_socket = nullptr;
            m_output->AppendLine(wxT("--- disconnected ---"));
        }
        UpdateStatus();
    }

    void OnSocketEvent(wxSocketEvent & ev) {
        wxSocketBase * sock = ev.GetSocket();
        switch (ev.GetSocketEvent()) {
            case wxSOCKET_CONNECTION:
                m_output->AppendLine(wxT("--- connected ---"));
                UpdateStatus();
                break;
            case wxSOCKET_INPUT: {
                // Drain the socket recv buffer; wxSOCKET_INPUT only
                // re-fires when MORE bytes arrive, so we have to
                // pull everything currently available in one event.
                for (;;) {
                    char buf[4096];
                    sock->Read(buf, sizeof(buf));
                    const std::size_t n = sock->LastCount();
                    if (n == 0) break;
                    m_parser.Feed(buf, n,
                        [this](std::vector<AnsiSpan> && spans) {
                            m_output->AppendStyledLine(std::move(spans));
                        });
                    if (n < sizeof(buf)) break;
                }
                m_output->SetPendingLine(m_parser.Pending());
                break;
            }
            case wxSOCKET_LOST:
                m_output->AppendLine(wxT("--- connection lost ---"));
                if (m_socket) { m_socket->Destroy(); m_socket = nullptr; }
                UpdateStatus();
                break;
            default:
                break;
        }
    }

    void UpdateStatus() {
        wxStatusBar * sb = GetStatusBar();
        if (!sb) return;
        if (m_socket && m_socket->IsConnected()) {
            sb->SetStatusText(wxString::Format(wxT("connected: %s"), m_lastHostPort), 0);
        } else {
            sb->SetStatusText(wxT("not connected"), 0);
        }
    }

    void OnInputEnter(wxCommandEvent & ev) {
        const wxString line = ev.GetString();
        if (m_socket && m_socket->IsConnected()) {
            wxString out = line + wxT("\r\n");
            const wxScopedCharBuffer utf8 = out.utf8_str();
            m_socket->Write(utf8.data(), utf8.length());
            // local echo so the user sees what they typed even before
            // the server echoes it back. MUDs typically suppress echo
            // of password lines via TELNET WILL ECHO; we'll handle
            // that later when we add a real ANSI/telnet parser.
            m_output->AppendLine(wxString::Format(wxT("> %s"), line));
        } else {
            m_output->AppendLine(wxString::Format(wxT("(offline) > %s"), line));
        }
        m_input->Clear();
    }

    OutputPane *      m_output{nullptr};
    wxTextCtrl *      m_input{nullptr};
    wxSocketClient *  m_socket{nullptr};
    AnsiTelnetParser  m_parser;        // server-side ANSI / telnet parser
    wxString          m_lastHostPort;  // remember last destination

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_File_Quit,         MainFrame::OnQuit)
    EVT_MENU(ID_File_Connect,      MainFrame::OnConnect)
    EVT_MENU(ID_File_Disconnect,   MainFrame::OnDisconnect)
    EVT_MENU(ID_Help_About,        MainFrame::OnAbout)
    EVT_TEXT_ENTER(ID_Input_Send,  MainFrame::OnInputEnter)
    EVT_SOCKET(ID_Socket,          MainFrame::OnSocketEvent)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────
// Solaris 7 SPARC global INIT_ARRAY walker.
//
// Solaris 7's ld.so.1 honours DT_INIT (a single function pointer) and
// runs `_init()` for every loaded ELF, but it does NOT understand
// DT_INIT_ARRAY — the modern way GCC records C++ static constructors
// and `__attribute__((constructor))` functions. Since GCC 5+, every
// ctor lands in DT_INIT_ARRAY by default; none of them run on Solaris 7.
//
// For wxX11 + glib + pango, that's catastrophic: GObject type
// registration for pango lives in `__attribute__((constructor))`
// functions inside libgobject / libpangoxft / libpango / libcairo /
// libgio / libglib. With those skipped, `g_type_init()` aborts the
// first time anything touches GObject.
//
// Walking just our own binary's INIT_ARRAY isn't enough — the ctors
// that initialise GObject's GType system live in the shared libs.
// We have to walk every loaded DSO's INIT_ARRAY.
//
// Plan: at DT_INIT time (which Solaris ld.so.1 calls for our main
// exe AFTER all the shared libs are loaded but BEFORE main runs),
// use dl_iterate_phdr (provided by libsolcompat on Solaris 7) to walk
// every loaded ELF, find each one's PT_DYNAMIC, scan it for
// DT_INIT_ARRAY + DT_INIT_ARRAYSZ, and call each function pointer.
// Skip our own main exe (we're being called from its DT_INIT — its
// INIT_ARRAY runs last via the linker-defined __init_array_start/end
// symbols).

// Solaris 7's <link.h> transitively includes <libelf.h>, which the
// patched sysroot copy errors-out on whenever `_FILE_OFFSET_BITS != 32`
// — a state most C++ source picks up implicitly via wx's flags.
// We don't need either header for the walker; declare just enough of
// the ELF dynamic-table machinery and dl_iterate_phdr's signature.

struct sst_phdr {
    unsigned int  p_type;
    unsigned long p_offset;
    unsigned long p_vaddr;
    unsigned long p_paddr;
    unsigned long p_filesz;
    unsigned long p_memsz;
    unsigned int  p_flags;
    unsigned long p_align;
};

struct sst_dyn {
    long          d_tag;
    union { unsigned long d_val; unsigned long d_ptr; } d_un;
};

struct sst_dl_phdr_info {
    unsigned long          dlpi_addr;
    const char *           dlpi_name;
    const struct sst_phdr * dlpi_phdr;
    unsigned short         dlpi_phnum;
    // libsolcompat truncates to these fields; the Linux glibc struct
    // has more after this, but we only ever read the head.
};

#define PT_DYNAMIC          2
#define DT_NULL             0
#define DT_INIT_ARRAY       25
#define DT_INIT_ARRAYSZ     27

extern "C" {
    extern void (*__init_array_start[])(int, char **, char **) __attribute__((weak));
    extern void (*__init_array_end[])(int, char **, char **)   __attribute__((weak));

    int dl_iterate_phdr(int (*)(struct sst_dl_phdr_info *, std::size_t, void *), void *);

    static int sst_sol7_visit_dso(struct sst_dl_phdr_info * info, std::size_t /*sz*/, void * /*data*/) {
        // Skip the main executable — its INIT_ARRAY is run separately
        // via the linker-emitted __init_array_{start,end} brackets.
        if (info->dlpi_addr == 0 && info->dlpi_name && info->dlpi_name[0] == '\0') {
            return 0;
        }
        // Find PT_DYNAMIC in this DSO.
        const struct sst_phdr * dyn_phdr = nullptr;
        for (unsigned i = 0; i < info->dlpi_phnum; ++i) {
            if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
                dyn_phdr = &info->dlpi_phdr[i];
                break;
            }
        }
        if (!dyn_phdr) return 0;

        const struct sst_dyn * dyn = (const struct sst_dyn *)(info->dlpi_addr + dyn_phdr->p_vaddr);
        void (**init_array)(int, char **, char **) = nullptr;
        std::size_t init_array_sz = 0;
        for (; dyn->d_tag != DT_NULL; ++dyn) {
            if (dyn->d_tag == DT_INIT_ARRAY)
                init_array = (void (**)(int, char **, char **))(info->dlpi_addr + dyn->d_un.d_ptr);
            else if (dyn->d_tag == DT_INIT_ARRAYSZ)
                init_array_sz = dyn->d_un.d_val / sizeof(void *);
        }
        if (!init_array || !init_array_sz) return 0;

        // Solaris 7 printf doesn't grok %zu — prints literal "zu".
        // Cast to unsigned long and use %lu to match.
        std::fprintf(stderr, "[sst-init] %s: walking %lu init_array entries\n",
                     info->dlpi_name && *info->dlpi_name ? info->dlpi_name : "(self)",
                     static_cast<unsigned long>(init_array_sz));
        for (std::size_t i = 0; i < init_array_sz; ++i) {
            if (init_array[i]) init_array[i](0, nullptr, nullptr);
        }
        return 0;
    }

    __attribute__((visibility("default")))
    void sst_sol7_run_init_array(void) {
        std::setvbuf(stderr, nullptr, _IONBF, 0);   // unbuffered so we see progress before any abort
        std::fprintf(stderr, "[sst-init] DT_INIT entered\n");

        // Phase 1: walk every loaded shared library's INIT_ARRAY.
        // dl_iterate_phdr is provided by libsolcompat on Solaris 7.
        dl_iterate_phdr(sst_sol7_visit_dso, nullptr);

        // Phase 2: walk our own binary's INIT_ARRAY (the C++ ctors
        // for our wxApp etc. live here, not in any DSO).
        if (__init_array_start && __init_array_end) {
            std::size_t n = __init_array_end - __init_array_start;
            std::fprintf(stderr, "[sst-init] (main exe): walking %lu init_array entries\n",
                         static_cast<unsigned long>(n));
            for (void (**fn)(int, char **, char **) = __init_array_start;
                 fn < __init_array_end; ++fn)
            {
                if (*fn) (*fn)(0, nullptr, nullptr);
            }
        }
        std::fprintf(stderr, "[sst-init] DT_INIT done\n");
    }
}

// ─────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────

class App : public wxApp {
public:
    bool OnInit() override {
        std::fprintf(stderr, "[wx] App::OnInit entered\n");
        if (!wxApp::OnInit()) {
            std::fprintf(stderr, "[wx] base OnInit returned false\n");
            return false;
        }
        wxSocketBase::Initialize();
        std::fprintf(stderr, "[wx] base OnInit OK; creating MainFrame\n");
        MainFrame * f = new MainFrame();
        std::fprintf(stderr, "[wx] MainFrame ctor returned; calling Show\n");
        f->Show(true);
        std::fprintf(stderr, "[wx] Show returned; entering event loop\n");
        return true;
    }
};

// Use IMPLEMENT_APP_NO_MAIN so we can write our own main() that
// drives the global INIT_ARRAY walker before wxEntry instantiates
// the wxApp instance.
wxIMPLEMENT_APP_NO_MAIN(App);

int main(int argc, char ** argv) {
    sst_sol7_run_init_array();
    std::fprintf(stderr, "[main] sst init done; DISPLAY=%s; calling wxEntry\n",
                 std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "(unset)");
    std::fflush(stderr);
    int rc = wxEntry(argc, argv);
    std::fprintf(stderr, "[main] wxEntry returned %d\n", rc);
    return rc;
}
