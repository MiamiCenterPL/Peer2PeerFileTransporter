#pragma once
#include "wx/wxprec.h"
#include "P2P.h"

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include <vector>
#include <string>

class WxWidgetsGUI : public wxApp {
public:
    virtual bool OnInit();
    virtual int OnExit();
    void UpdatePeerList();
    void SetConnectedGUI();
	void UpdateFileList(const std::vector<std::string>& files);
    void Log(std::string log);
private:
    
    wxFrame* frame;
    wxPanel* panel;
    wxListBox* userListBox;
    wxListBox* consoleListBox;
    wxCheckListBox* fileListBox;
    wxButton* copySocketButton;
    wxButton* shareFileButton;
    wxButton* searchFileButton;
    wxButton* downloadFileButton;
    P2P networkManager;

    void OnHost(wxCommandEvent& event);
    void OnJoin(wxCommandEvent& event);
    void OnCopySocket(wxCommandEvent& event);
    void OnShareFile(wxCommandEvent& event);
    void OnSearchFile(wxCommandEvent& event);
    void OnDownloadFile(wxCommandEvent& event);
    void OnSelectedFile(wxCommandEvent& event);
};

wxDECLARE_APP(WxWidgetsGUI);
