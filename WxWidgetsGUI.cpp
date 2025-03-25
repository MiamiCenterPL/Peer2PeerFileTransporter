#include "wx/wxprec.h"
#include "WxWidgetsGUI.h"
#include "P2P.h"
#include <thread>

wxIMPLEMENT_APP(WxWidgetsGUI);

P2P networkManager;

bool WxWidgetsGUI::OnInit() {
    frame = new wxFrame(NULL, wxID_ANY, "P2P", wxDefaultPosition, wxSize(610, 400));
    panel = new wxPanel(frame, wxID_ANY);

    wxButton* hostButton = new wxButton(panel, wxID_ANY, "HOST", wxPoint(10, 10));
    wxButton* joinButton = new wxButton(panel, wxID_ANY, "JOIN", wxPoint(100, 10));
    userListBox = new wxListBox(panel, wxID_ANY, wxPoint(10, 50), wxSize(280, 150));

    consoleListBox = new wxListBox(panel, wxID_ANY, wxPoint(10, 250), wxSize(570, 100));

    hostButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnHost, this);
    joinButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnJoin, this);

    frame->Show(true);
    return true;
}
int WxWidgetsGUI::OnExit() {
	networkManager.disconnect();
    WSACleanup();
	return wxApp::OnExit();
}

void WxWidgetsGUI::UpdatePeerList() {
    userListBox->Clear();
    for (const auto& user : networkManager.connectedUsers) {
        if (user == networkManager.myInfo) {
            userListBox->AppendString("You: " + user);
        }
        else {
            userListBox->AppendString(user);
        }
    }
}

void WxWidgetsGUI::SetConnectedGUI() {
    panel->Destroy();
    panel = new wxPanel(frame, wxID_ANY);
    userListBox = new wxListBox(panel, wxID_ANY, wxPoint(10, 10), wxSize(280, 150));
    fileListBox = new wxCheckListBox(panel, wxID_ANY, wxPoint(300, 10), wxSize(280, 150));
    fileListBox->Bind(wxEVT_CHECKLISTBOX, &WxWidgetsGUI::OnSelectedFile, this);

    copySocketButton = new wxButton(panel, wxID_ANY, "Copy Socket Info", wxPoint(10, 170));
    copySocketButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnCopySocket, this);

    shareFileButton = new wxButton(panel, wxID_ANY, "Share File", wxPoint(150, 170));
    shareFileButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnShareFile, this);

    searchFileButton = new wxButton(panel, wxID_ANY, "Search File", wxPoint(10, 210));
    searchFileButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnSearchFile, this);

    downloadFileButton = new wxButton(panel, wxID_ANY, "Download File", wxPoint(150, 210));
    downloadFileButton->Enable(false);
    downloadFileButton->Bind(wxEVT_BUTTON, &WxWidgetsGUI::OnDownloadFile, this);

	consoleListBox = new wxListBox(panel, wxID_ANY, wxPoint(10, 250), wxSize(570, 100));
    frame->Layout();
}

void WxWidgetsGUI::Log(std::string log) {
	consoleListBox->AppendString(log);
}

void WxWidgetsGUI::OnHost(wxCommandEvent& event) {
    networkManager.host();
    std::thread(&P2P::StartListening, &networkManager).detach();
}

void WxWidgetsGUI::OnJoin(wxCommandEvent& event) {
    wxTextEntryDialog dialog(NULL, "Enter peer info (IP:Port):", "Join Session");
    if (dialog.ShowModal() == wxID_OK) {
        std::string peerInfo = dialog.GetValue().ToStdString();
        std::thread([this, peerInfo]() {
            if (networkManager.join(peerInfo)) {
                std::thread(&P2P::StartListening, &networkManager).detach();
            }
            }).detach();
    }
}

void WxWidgetsGUI::OnCopySocket(wxCommandEvent& event) {
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(networkManager.myInfo));
        wxTheClipboard->Close();
        wxMessageBox("Informacje o socketcie zosta³y skopiowane do schowka.", "Informacja", wxOK | wxICON_INFORMATION);
    }
}

void WxWidgetsGUI::OnShareFile(wxCommandEvent& event) {
    wxFileDialog openFileDialog(NULL, _("Wybierz plik do udostêpnienia"), "", "", "*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openFileDialog.ShowModal() == wxID_OK) {
        networkManager.shareFile(openFileDialog);
    }
}

void WxWidgetsGUI::OnSearchFile(wxCommandEvent& event) {
    wxTextEntryDialog dialog(NULL, "WprowadŸ nazwê pliku do wyszukania:", "Wyszukiwanie pliku");
    if (dialog.ShowModal() == wxID_OK) {
        std::string fileName = dialog.GetValue().ToStdString();
        networkManager.searchFile(fileName);
    }
    //Prepare for receiving files
    fileListBox->Clear();
}

void WxWidgetsGUI::OnDownloadFile(wxCommandEvent& event) {
    wxArrayInt checkedItems;
    fileListBox->GetCheckedItems(checkedItems);
    // Wybór lokalizacji pobierania
    wxDirDialog dirDialog(NULL, "Wybierz folder do zapisu plików", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dirDialog.ShowModal() == wxID_OK) {
        wxString downloadPath = dirDialog.GetPath();

        for (size_t i = 0; i < checkedItems.GetCount(); ++i) {
            wxString file = fileListBox->GetString(checkedItems[i]);
            // Uruchomienie downloadFile w nowym w¹tku
            std::thread([this, file, downloadPath]() {
                networkManager.downloadFile(file.ToStdString(), downloadPath.ToStdString());
                }).detach();
        }
        wxMessageBox("Rozpoczêto pobieranie wybranych plików.", "Pobieranie", wxOK | wxICON_INFORMATION);
    }
}

void WxWidgetsGUI::UpdateFileList(const std::vector<std::string>& files) {
    for (const auto& file : files) {
        fileListBox->AppendString(file);
    }
}

void WxWidgetsGUI::OnSelectedFile(wxCommandEvent& event) {
    wxArrayInt checkedItems;
    fileListBox->GetCheckedItems(checkedItems);
    downloadFileButton->Enable(checkedItems.GetCount() > 0);
}