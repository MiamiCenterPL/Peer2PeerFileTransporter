#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include <string>
#include <vector>
#include <wx/filedlg.h>

class P2P {
public:
	// Variables;
	std::vector<std::string> connectedUsers;
	std::string myInfo;
	std::vector<std::string> sharedFiles;
	// Functions;
	void host();
	bool join(std::string peerInfo);
	void StartListening();
	void searchFile(std::string fileName);
	void shareFile(wxFileDialog& dialog);
	void downloadFile(const std::string& fileInfo, const std::string& downloadPath);
	void sendFile(const std::string& filePath, sockaddr_in clientAddr);
	void disconnect();
private:
	// Variables:
	SOCKET udpSocket;

	// Functions;
	void handleJoin(sockaddr_in &clientAddr);
	void handleConfirmation(sockaddr_in &clientAddr);
	void handleUsersList(std::string message);
	void handleGetFileRequest(const std::string& fileDetails, sockaddr_in& clientAddr);
	void handleFoundFiles(sockaddr_in& peerAddr, std::string message);
	void handleSearch(std::string requested, sockaddr_in& clientAddr);
	void handleDisconnect(sockaddr_in clientAddr);
};