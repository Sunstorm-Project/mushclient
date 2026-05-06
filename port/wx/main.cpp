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

#include <cstddef>
#include <vector>

// ─────────────────────────────────────────────────────────────────────
// OutputPane — custom-drawn rolling-text window. wxRichTextCtrl is
// avoided because wxX11/wxUniversal renders it poorly. Each line is a
// std::string in a circular buffer; OnPaint walks the visible range
// and renders via wxDC::DrawText.
// ─────────────────────────────────────────────────────────────────────

class OutputPane : public wxScrolledWindow {
public:
    OutputPane(wxWindow * parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxHSCROLL | wxBORDER_SUNKEN)
    {
        SetBackgroundColour(wxColour(0, 0, 0));
        m_font = wxFont(wxFontInfo(11).Family(wxFONTFAMILY_TELETYPE));
        SetScrollbars(8, 14, 0, 0);
    }

    void AppendLine(const wxString & s) {
        m_lines.push_back(s);
        if (m_lines.size() > 5000) m_lines.erase(m_lines.begin());
        SetVirtualSize(wxDefaultCoord, static_cast<int>(m_lines.size()) * LineHeight());
        Refresh();
    }

private:
    int LineHeight() const { return 14; }

    void OnPaint(wxPaintEvent &) {
        wxPaintDC dc(this);
        DoPrepareDC(dc);
        dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
        dc.Clear();
        dc.SetTextForeground(wxColour(0xC0, 0xC0, 0xC0));
        dc.SetFont(m_font);
        const int lh = LineHeight();
        for (std::size_t i = 0; i < m_lines.size(); ++i) {
            dc.DrawText(m_lines[i], 4, static_cast<int>(i) * lh + 1);
        }
    }

    std::vector<wxString> m_lines;
    wxFont m_font;

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(OutputPane, wxScrolledWindow)
    EVT_PAINT(OutputPane::OnPaint)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────
// Frame — top-level window. Splitter divides output (top) from input
// (bottom). Menubar + status bar.
// ─────────────────────────────────────────────────────────────────────

enum {
    ID_File_Quit = wxID_EXIT,
    ID_Help_About = wxID_ABOUT,
    ID_Input_Send = wxID_HIGHEST + 1,
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
        file->Append(ID_File_Quit, wxT("E&xit\tCtrl+Q"), wxT("Close MUSHclient"));
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
        sb->SetStatusText(wxT("v0.1-port"), 1);
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

        m_output->AppendLine(wxT("MUSHclient SPARC Solaris 7 port — v0.1 shell"));
        m_output->AppendLine(wxT("Type below and press Enter to echo."));
        m_output->AppendLine(wxEmptyString);

        m_input->SetFocus();
    }

    void OnQuit(wxCommandEvent &)  { Close(true); }
    void OnAbout(wxCommandEvent &) {
        wxMessageBox(wxT("MUSHclient SPARC Solaris 7 port\n"
                         "wxWidgets/X11 shell — v0.1\n\n"
                         "Sunstorm-Project/mushclient feat/sparc-solaris7-port"),
                     wxT("About"), wxOK | wxICON_INFORMATION, this);
    }
    void OnInputEnter(wxCommandEvent & ev) {
        const wxString line = ev.GetString();
        m_output->AppendLine(wxString::Format(wxT("> %s"), line));
        m_input->Clear();
    }

    OutputPane * m_output{nullptr};
    wxTextCtrl * m_input{nullptr};

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MainFrame, wxFrame)
    EVT_MENU(ID_File_Quit,     MainFrame::OnQuit)
    EVT_MENU(ID_Help_About,    MainFrame::OnAbout)
    EVT_TEXT_ENTER(ID_Input_Send, MainFrame::OnInputEnter)
wxEND_EVENT_TABLE()

// ─────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────

class App : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;
        (new MainFrame())->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(App);
