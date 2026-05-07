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
// OutputPane — custom-painted scrolled view of styled MUD output.
//
// wxMotif's wxTextCtrl backs onto an XmText widget that only supports
// a single foreground/background pair per widget — per-character ANSI
// colour can't be done that way. Gammon's Win32 MUSHclient uses a CView
// (CMUSHView) that paints the styled output line-by-line via CDC; we
// do the same here on a wxScrolledWindow + wxPaintDC, which:
//   • gives us a true black background (XmText ignores SetBg on Sol 7)
//   • renders each AnsiSpan in its own foreground colour
//   • supports the CDE-style fixed-pitch font as a single uniform face
//   • is the natural drop-in target for the eventual mushview.cpp port.
//
// wxBORDER_SUNKEN trips X_ConfigureWindow BadValue on wxMotif Sol 7;
// use wxNO_BORDER. SetBackgroundStyle(wxBG_STYLE_PAINT) at construction
// also raises BadValue — skip it; the OnPaint fill handles bg.
// ─────────────────────────────────────────────────────────────────────

class OutputPane : public wxScrolledWindow {
public:
    OutputPane(wxWindow * parent)
        : wxScrolledWindow(parent, wxID_ANY,
                           wxPoint(0, 0), wxSize(100, 100),
                           wxBORDER_SUNKEN)   // per the flicker-free
                                              // recipe — wxNO_BORDER
                                              // works visually but
                                              // SUNKEN gives Motif
                                              // a stable widget tree.
    {
        // wxMotif on Solaris 7 raises X_ConfigureWindow BadValue if we
        // call SetBackgroundStyle(wxBG_STYLE_PAINT) at construction —
        // skip it. Black bg comes from wx's default erase using the
        // colour we set here.
        SetBackgroundColour(*wxBLACK);
        // MUSHclient default WHITE-normal from Utilities.cpp:1655 —
        // light grey on black.
        SetForegroundColour(wxColour(192, 192, 192));

        // CDE dtterm-flavour terminal font. wxFONTFAMILY_TELETYPE asks
        // X11 for a fixed-pitch face; on Solaris 7 CDE that typically
        // resolves to one of:
        //   -bitstream-courier-medium-r-normal--14-100-100-100-m-90-iso8859-1
        //   -misc-fixed-medium-r-normal--14-130-75-75-c-70-iso8859-1
        m_font = wxFont(12, wxFONTFAMILY_TELETYPE,
                        wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL,
                        false, wxEmptyString);
        m_fontBold = m_font; m_fontBold.SetWeight(wxFONTWEIGHT_BOLD);

        // Cell metrics — defer measurement until the widget is shown
        // (wxClientDC before realize on wxMotif raises BadValue from
        // X_ConfigureWindow). Use safe defaults until first paint.
        m_cellH = 14;
        m_cellW = 8;

        Bind(wxEVT_PAINT, &OutputPane::OnPaint, this);
        Bind(wxEVT_SIZE,  &OutputPane::OnSize,  this);
        Bind(wxEVT_SHOW,  &OutputPane::OnFirstShow, this);
        // Flicker-free black bg recipe (per the 2026-05-07 evening
        // handoff). wxMotif's automatic erase paints wxSYS_COLOUR_3DFACE
        // (Motif grey) regardless of SetBackgroundColour. Intercept
        // EVT_ERASE_BACKGROUND and paint BLACK on the DC the event hands
        // us — the same DC drives the Expose region the subsequent
        // EVT_PAINT will draw text on, so the user sees one composite,
        // no grey-flash before text. Crucially: a no-op handler does NOT
        // work (leaves the inherited grey); we MUST do the black fill
        // here, not in OnPaint.
        Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent & ev) {
            wxDC * dc = ev.GetDC();
            if (!dc) return;
            dc->SetBackground(wxBrush(*wxBLACK));
            dc->Clear();
        });
    }

    // API matching the old wxTextCtrl-based pane so callers don't change.
    void AppendLine(const wxString & s) {
        AnsiSpan span;
        span.text = s;
        span.fg   = wxColour(192, 192, 192);   // ANSI light grey
        span.bold = false;
        m_lines.push_back({span});
        OnContentChanged();
    }

    void AppendStyledLine(std::vector<AnsiSpan> && spans) {
        m_lines.push_back(std::move(spans));
        OnContentChanged();
    }

    // A line that's still being received (no terminating \n yet).
    // Drawn at the bottom but not committed to m_lines.
    //
    // CRITICAL: only Refresh if the pending content actually changed.
    // The poll-timer calls this every 50ms regardless of whether data
    // arrived; an unconditional Refresh here forced a 20 Hz full
    // repaint = the visible "constant flicker" the user reported. With
    // the early-out, idle ticks are zero-cost and only real content
    // updates trigger a paint.
    void SetPendingLine(std::vector<AnsiSpan> spans) {
        if (PendingEqual(m_pending, spans)) return;
        m_pending = std::move(spans);
        Refresh(false);
    }

private:
    static bool PendingEqual(const std::vector<AnsiSpan> & a,
                             const std::vector<AnsiSpan> & b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (a[i].text != b[i].text) return false;
            if (a[i].bold != b[i].bold) return false;
            if (a[i].fg   != b[i].fg)   return false;
        }
        return true;
    }
public:

private:
    void OnFirstShow(wxShowEvent & ev) {
        ev.Skip();
        if (m_metricsDone) return;
        m_metricsDone = true;
        wxClientDC dc(this);
        dc.SetFont(m_font);
        const int h = dc.GetCharHeight();
        const int w = dc.GetCharWidth();
        if (h >= 8) m_cellH = h;
        if (w >= 4) m_cellW = w;
        SetScrollRate(m_cellW, m_cellH);
        SetVirtualSize(2000, std::max<int>(1, m_lines.size() + 1) * m_cellH);
    }

    void OnContentChanged() {
        // Trim the scrollback to a reasonable maximum so this doesn't
        // grow without bound. 5000 lines is plenty for a MUD session.
        constexpr std::size_t kMaxLines = 5000;
        if (m_lines.size() > kMaxLines) {
            m_lines.erase(m_lines.begin(),
                          m_lines.begin() + (m_lines.size() - kMaxLines));
        }
        const int total = static_cast<int>(m_lines.size());
        SetVirtualSize(2000, (total + 1) * m_cellH);
        // Auto-scroll to bottom.
        int xUnit, yUnit;
        GetScrollPixelsPerUnit(&xUnit, &yUnit);
        if (yUnit > 0) {
            const int visibleRows = GetClientSize().GetHeight() / m_cellH;
            const int targetUnit  = std::max(0, total - visibleRows + 1);
            Scroll(0, targetUnit);
        }
        Refresh(false);
    }

    void OnSize(wxSizeEvent & ev) { ev.Skip(); Refresh(false); }

    void OnPaint(wxPaintEvent &) {
        // No bg fill in OnPaint — our EVT_ERASE_BACKGROUND handler
        // (in the ctor) has already painted black on the same DC's
        // expose region. We just stamp text on top. Adding a fill
        // here would be a SECOND op per paint, which is the visible
        // flicker we used to see.
        wxPaintDC dc(this);
        DoPrepareDC(dc);

        const wxRect view = GetUpdateRegion().GetBox();
        int virtX, virtY;
        CalcUnscrolledPosition(view.x, view.y, &virtX, &virtY);
        const int firstLine = std::max(0, virtY / m_cellH);
        const int lastLine  = std::min(static_cast<int>(m_lines.size()),
                                       (virtY + view.height) / m_cellH + 1);

        for (int i = firstLine; i < lastLine; ++i) {
            DrawLine(dc, i * m_cellH, m_lines[static_cast<std::size_t>(i)]);
        }
        if (!m_pending.empty()) {
            DrawLine(dc, static_cast<int>(m_lines.size()) * m_cellH, m_pending);
        }
    }

    void DrawLine(wxDC & dc, int y, const std::vector<AnsiSpan> & spans) {
        int x = 0;
        dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
        for (const AnsiSpan & span : spans) {
            dc.SetFont(span.bold ? m_fontBold : m_font);
            dc.SetTextForeground(span.fg);
            dc.DrawText(span.text, x, y);
            int w, h;
            dc.GetTextExtent(span.text, &w, &h);
            x += w;
        }
    }

    std::vector<std::vector<AnsiSpan> > m_lines;
    std::vector<AnsiSpan>               m_pending;
    wxFont   m_font;
    wxFont   m_fontBold;
    int      m_cellW{8};
    int      m_cellH{14};
    bool     m_metricsDone{false};
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
    ID_PollTimer,
};

// SegmentedStatusBar — wxMotif's default wxStatusBar paints as a flat
// strip with no field separators. Real CDE / Motif apps use a row of
// XmFrame-with-SHADOW_IN segments. We approximate that with a wxPanel
// hosting a horizontal sizer of wxStaticText controls each with a
// wxBORDER_SUNKEN; wxMotif maps that to an XmFrame with shadowType=
// SHADOW_IN, giving the inset look. Layout matches Gammon's MainFrm:
// wide status field plus three narrow fields (lines / output / time)
// at stretch 4:1:1:1.
class SegmentedStatusBar : public wxPanel {
public:
    enum Field { F_Status = 0, F_Lines, F_Output, F_Time, F_Count };
    SegmentedStatusBar(wxWindow * parent)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    {
        wxBoxSizer * row = new wxBoxSizer(wxHORIZONTAL);
        const int stretch[F_Count] = { 4, 1, 1, 1 };
        for (int i = 0; i < F_Count; ++i) {
            m_field[i] = new wxStaticText(this, wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxBORDER_SUNKEN | wxST_NO_AUTORESIZE);
            row->Add(m_field[i], stretch[i], wxEXPAND | wxALL, 1);
        }
        SetSizer(row);
        SetMinSize(wxSize(-1, 22));
    }
    void Set(Field f, const wxString & s) {
        if (f >= 0 && f < F_Count) m_field[f]->SetLabel(s);
    }
private:
    wxStaticText * m_field[F_Count]{};
};

class MainFrame : public wxFrame {
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, wxT("MUSHclient"),
                  wxDefaultPosition, wxSize(880, 600))
    {
        BuildMenuBar();
        BuildClientArea();
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

    void BuildClientArea() {
        // Top-level vertical layout: splitter on top, segmented status
        // bar pinned to the bottom. We build our own status bar (rather
        // than wxFrame::CreateStatusBar) because wxMotif's default is a
        // flat un-segmented strip; SegmentedStatusBar gives us the
        // proper CDE sunken-bevel multi-field look.
        wxBoxSizer * frame_sizer = new wxBoxSizer(wxVERTICAL);

        wxSplitterWindow * split = new wxSplitterWindow(this, wxID_ANY,
            wxDefaultPosition, wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
        m_output = new OutputPane(split);
        m_input  = new wxTextCtrl(split, ID_Input_Send, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER);
        split->SplitHorizontally(m_output, m_input, -120);
        split->SetMinimumPaneSize(60);

        m_status = new SegmentedStatusBar(this);
        m_status->Set(SegmentedStatusBar::F_Status, wxT("Ready"));

        frame_sizer->Add(split,    1, wxEXPAND);
        frame_sizer->Add(m_status, 0, wxEXPAND);
        SetSizer(frame_sizer);

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

public:
    // Public so MUSHCLIENT_AUTOCONNECT env-var hook can call us
    // without going through the wxGetTextFromUser modal.
    void DoConnect(const wxString & hp) {
        if (m_socket && m_socket->IsConnected()) {
            wxMessageBox(wxT("Already connected — disconnect first."),
                         wxT("Connect"), wxOK, this);
            return;
        }
        if (hp.IsEmpty()) return;
        m_lastHostPort = hp;

        wxString host, portstr;
        const int colon = hp.Find(':', true);
        if (colon == wxNOT_FOUND) { host = hp; portstr = wxT("23"); }
        else { host = hp.Mid(0, colon); portstr = hp.Mid(colon + 1); }
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
        m_parser = AnsiTelnetParser{};

        m_output->AppendLine(wxString::Format(wxT("Connecting to %s:%ld..."), host, port));
        // wxMotif's async wxSOCKET_CONNECTION event is unreliable on
        // Solaris 7; explicit WaitOnConnect drives the FD to readiness
        // here. wxSOCKET_INPUT also doesn't fire reliably so we poll
        // the recv buffer on a 50ms wxTimer (see m_pollTimer + DrainSocket).
        m_socket->Connect(addr, false);
        const bool ok = m_socket->WaitOnConnect(10);
        if (!ok || !m_socket->IsConnected()) {
            m_output->AppendLine(wxT("--- connect failed (timeout or refused) ---"));
            return;
        }
        m_output->AppendLine(wxT("--- connected ---"));
        UpdateStatus();
        m_pollTimer.Start(50);
    }
private:
    void OnConnect(wxCommandEvent &) {
        wxString hp = wxGetTextFromUser(
            wxT("Enter host:port (e.g. aardmud.org:23)"),
            wxT("Connect to MUD"),
            m_lastHostPort.IsEmpty() ? wxT("aardmud.org:23") : m_lastHostPort,
            this);
        if (hp.IsEmpty()) return;
        DoConnect(hp);
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
        if (!m_status) return;
        if (m_socket && m_socket->IsConnected()) {
            m_status->Set(SegmentedStatusBar::F_Status,
                          wxString::Format(wxT("Connected to %s"), m_lastHostPort));
        } else {
            m_status->Set(SegmentedStatusBar::F_Status, wxT("Ready"));
        }
    }

    void OnInputEnter(wxCommandEvent & ev) {
        // wxMotif's wxTextCtrl populates ev.GetString() inconsistently
        // — it can come back empty even when the box has text, so we
        // were sending blank "\r\n" lines to the server. Read the
        // widget's value directly; fall back to the event string only
        // if m_input is somehow null.
        const wxString line = m_input ? m_input->GetValue() : ev.GetString();
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
        m_input->SetFocus();
    }

    OutputPane *         m_output{nullptr};
    wxTextCtrl *         m_input{nullptr};
    wxSocketClient *     m_socket{nullptr};
    SegmentedStatusBar * m_status{nullptr};
    AnsiTelnetParser     m_parser;        // server-side ANSI / telnet parser
    wxString             m_lastHostPort;  // remember last destination
    wxTimer              m_pollTimer{this, ID_PollTimer};

public:
    // Pulled out of OnSocketEvent so the poll-timer path can call it.
    // wxMotif on Solaris 7 doesn't fire wxSOCKET_INPUT events reliably;
    // we drive reads off m_pollTimer at 50ms instead.
    void DrainSocket() {
        if (!m_socket || !m_socket->IsConnected()) return;
        for (;;) {
            char buf[4096];
            m_socket->Read(buf, sizeof(buf));
            const std::size_t n = m_socket->LastCount();
            if (n == 0) break;
            m_parser.Feed(buf, n,
                [this](std::vector<AnsiSpan> && spans) {
                    m_output->AppendStyledLine(std::move(spans));
                });
            if (n < sizeof(buf)) break;
        }
        m_output->SetPendingLine(m_parser.Pending());
        if (m_socket->IsDisconnected()) {
            m_output->AppendLine(wxT("--- connection lost ---"));
            m_socket->Destroy();
            m_socket = nullptr;
            m_pollTimer.Stop();
            UpdateStatus();
        }
    }
    void OnPollTimer(wxTimerEvent &) { DrainSocket(); }

private:
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_File_Quit,         MainFrame::OnQuit)
    EVT_MENU(ID_File_Connect,      MainFrame::OnConnect)
    EVT_MENU(ID_File_Disconnect,   MainFrame::OnDisconnect)
    EVT_MENU(ID_Help_About,        MainFrame::OnAbout)
    EVT_TEXT_ENTER(ID_Input_Send,  MainFrame::OnInputEnter)
    EVT_SOCKET(ID_Socket,          MainFrame::OnSocketEvent)
    EVT_TIMER(ID_PollTimer,        MainFrame::OnPollTimer)
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

        // MUSHCLIENT_AUTOCONNECT=host:port — auto-connect at startup,
        // skipping the wxGetTextFromUser dialog. Useful for scripted
        // testing and as a poor-man's auto-open-last-world.
        if (const char * hp = std::getenv("MUSHCLIENT_AUTOCONNECT")) {
            if (*hp) {
                std::fprintf(stderr, "[wx] autoconnect: %s\n", hp);
                f->DoConnect(wxString::FromUTF8(hp));
            }
        }
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
