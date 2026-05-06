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

        std::fprintf(stderr, "[sst-init] %s: walking %zu init_array entries\n",
                     info->dlpi_name && *info->dlpi_name ? info->dlpi_name : "(self)",
                     init_array_sz);
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
            std::fprintf(stderr, "[sst-init] (main exe): walking %zu init_array entries\n", n);
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
