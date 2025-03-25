#include "wx/wxprec.h"
#include "P2P.h"
#include "WxWidgetsGUI.h"
#include <wininet.h>
#include <sstream>
#include <thread>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")

/*
	Peer2Peer listener function, it listens for incoming messages and handles them accordingly.
	* JOIN - a new peer has joined the network
	* CONFIRMATION - confirmation message sent to a new peer that it indeed joined the network
	* USERS - list of all connected users in the network - used for updating the peer list on each peer
	* SEARCH - search for a file in the network that might be shared by other peers
	* SEARCH_RESPONSE - response to a SEARCH message, contains a list of files that match the search query (or a NOT_FOUND message)
	* GET_FILE - request to download a file from another peer
	* DISCONNECT - message sent when a peer disconnects from the network
*/
void P2P::StartListening() {
    char buffer[1024];
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0, (SOCKADDR*)&clientAddr, &clientAddrLen);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string message(buffer);
			wxTheApp->CallAfter([message]() {
				wxGetApp().Log(message);
				});
            if (message == "JOIN") {
                handleJoin(clientAddr);
            }
            else if (message == "CONFIRMATION") {
                handleConfirmation(clientAddr);
            }
            else if (message.rfind("USERS|", 0) == 0) {
                handleUsersList(message);
            }
            else if (message.rfind("SEARCH:", 0) == 0) {
                std::string fileName = message.substr(7);
                handleSearch(fileName, clientAddr);
            }
            else if (message.rfind("SEARCH_RESPONSE:", 0) == 0) {
                std::string searchResponse = message.substr(16);
                handleFoundFiles(clientAddr, searchResponse);
            }
            else if (message.rfind("GET_FILE:", 0) == 0) {
                std::string fileDetails = message.substr(9);
                handleGetFileRequest(fileDetails, clientAddr);
            }
            else if (message.rfind("DISCONNECT", 0) == 0) {
                handleDisconnect(clientAddr);
            }
        }
    }
}
/*
	Disconnect function, it sends a DISCONNECT message to all connected peers and closes the UDP socket.
*/
void P2P::disconnect() {
    std::string disconnectMessage = "DISCONNECT";

    for (const auto& user : connectedUsers) {
        if (user != myInfo) {
            size_t colonPos = user.find(':');
            if (colonPos != std::string::npos) {
                std::string userIP = user.substr(0, colonPos);
                unsigned short userPort = static_cast<unsigned short>(std::stoi(user.substr(colonPos + 1)));

                sockaddr_in userAddr;
                userAddr.sin_family = AF_INET;
                inet_pton(AF_INET, userIP.c_str(), &userAddr.sin_addr);
                userAddr.sin_port = htons(userPort);

                sendto(udpSocket, disconnectMessage.c_str(), static_cast<int>(disconnectMessage.size()), 0, (SOCKADDR*)&userAddr, sizeof(userAddr));
            }
        }
    }

    closesocket(udpSocket);
}

/*
	HandleDisconnect function, it removes a disconnected peer from the connectedUsers list and updates the peer list in the GUI.
*/
void P2P::handleDisconnect(sockaddr_in clientAddr) {
	char clientIP[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
	unsigned short clientPort = ntohs(clientAddr.sin_port);
	std::string disconnectedUser = std::string(clientIP) + ":" + std::to_string(clientPort);
	connectedUsers.erase(std::remove(connectedUsers.begin(), connectedUsers.end(), disconnectedUser), connectedUsers.end());
	wxTheApp->CallAfter([this]() {
		wxGetApp().UpdatePeerList();
		});
}

/*
	HandleJoin function, it handles a JOIN message from a new peer, sends a CONFIRMATION message to the new peer, and sends a USERS message to all connected peers (to update the Peers list).
*/
void P2P::handleJoin(sockaddr_in& clientAddr) {
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
    unsigned short clientPort = ntohs(clientAddr.sin_port);

    connectedUsers.push_back(std::string(clientIP) + ":" + std::to_string(clientPort));
    wxTheApp->CallAfter([this]() {
        wxGetApp().UpdatePeerList();
        });

	// Send an CONFIRMATION message to the new peer
    std::string confirmationMessage = "CONFIRMATION";
    wxTheApp->CallAfter([confirmationMessage, clientIP]() {
        wxGetApp().Log(confirmationMessage + " to " + std::string(clientIP));
        });
    sendto(udpSocket, confirmationMessage.c_str(), static_cast<int>(confirmationMessage.size()), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));

	// Send a USERS message to all connected peers
    std::string userListMessage = "USERS";
    for (const auto& user : connectedUsers) {
        userListMessage += "|" + user;
    }

    for (const auto& user : connectedUsers) {
        if (user != myInfo) {
            size_t colonPos = user.find(':');
            if (colonPos != std::string::npos) {
                std::string userIP = user.substr(0, colonPos);
                unsigned short userPort = static_cast<unsigned short>(std::stoi(user.substr(colonPos + 1)));

                sockaddr_in userAddr;
                userAddr.sin_family = AF_INET;
                inet_pton(AF_INET, userIP.c_str(), &userAddr.sin_addr);
                userAddr.sin_port = htons(userPort);
                sendto(udpSocket, userListMessage.c_str(), static_cast<int>(userListMessage.size()), 0, (SOCKADDR*)&userAddr, sizeof(userAddr));
            }
        }
    }
}

/*
	HandleConfirmation function, it updates the GUI to show that the peer is connected. GUI also shows new buttons, such as "Share file" and "Download file".
*/
void P2P::handleConfirmation(sockaddr_in& clientAddr) {
    wxTheApp->CallAfter([]() {
        wxGetApp().SetConnectedGUI();
        });
}

/*
	HandleUsersList function, it updates the connectedUsers list with the list of users received from another peer.
*/
void P2P::handleUsersList(std::string message) {
    connectedUsers.clear();
    std::istringstream ss(message.substr(6)); // Pomijamy "USERS|"
    std::string token;
    while (std::getline(ss, token, '|')) {
        connectedUsers.push_back(token);
    }
    wxTheApp->CallAfter([]() {
        wxGetApp().UpdatePeerList();
        });
}

/*
	HandleSearch function, it searches for a file in the sharedFiles list and sends a SEARCH_RESPONSE message to the peer that requested the file.
*/
void P2P::handleSearch(std::string requested, sockaddr_in& clientAddr) {
    // Convert requested string to lowercase
    std::transform(requested.begin(), requested.end(), requested.begin(), ::tolower);

    bool found = false;
    for (const auto& file : sharedFiles) {
        size_t colonPos = file.find(':');
        if (colonPos != std::string::npos) {
            std::string sharedFileName = file.substr(0, colonPos);
			// Ignore case when comparing
            std::string sharedFileNameLower = sharedFileName;
            std::transform(sharedFileNameLower.begin(), sharedFileNameLower.end(), sharedFileNameLower.begin(), ::tolower);

            if (sharedFileNameLower.find(requested) != std::string::npos) {
                std::string searchResponse = "SEARCH_RESPONSE:" + sharedFileName;
                sendto(udpSocket, searchResponse.c_str(), static_cast<int>(searchResponse.size()), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
                found = true;
            }
        }
    }
    if (!found) {
        std::string searchResponse = "SEARCH_RESPONSE:\\NOT_FOUND";
        sendto(udpSocket, searchResponse.c_str(), static_cast<int>(searchResponse.size()), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));
    }
}

/*
	HandleFoundFiles function, it updates the GUI with the list of files found in the network that match the search query.
*/
void P2P::handleFoundFiles(sockaddr_in& peerAddr, std::string message) {
    std::string token;
    std::istringstream ss(message);
    std::vector<std::string> foundFiles;
    while (std::getline(ss, token, '|')) {
        if (token != "\\NOT_FOUND") {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(peerAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
            std::string listItem = std::string(ipStr) + ":" + std::to_string(ntohs(peerAddr.sin_port)) + " " + token;
            foundFiles.push_back(listItem);
        }
    }
    wxTheApp->CallAfter([foundFiles]() {
        wxGetApp().UpdateFileList(foundFiles);
        });
}

/*
	HandleGetFileRequest function, it handles a GET_FILE request from a peer, finds the requested file in the sharedFiles list, and sends the file to the peer.
*/
void P2P::handleGetFileRequest(const std::string& fileDetails, sockaddr_in& clientAddr) {
	// Find in sharedFiles list
    wxTheApp->CallAfter([fileDetails]() {
        wxGetApp().Log("File requested: " + fileDetails);
        });
    auto it = std::find_if(sharedFiles.begin(), sharedFiles.end(), [&](const std::string& file) {
        return file.find(fileDetails) != std::string::npos;
        });

    if (it != sharedFiles.end()) {
        size_t colonPos = it->find(':');
        std::string filePath = it->substr(colonPos + 1);
        wxTheApp->CallAfter([fileDetails]() {
            wxGetApp().Log("Sending file: " + fileDetails);
            });
		// Create a new thread to send the file
        std::thread(&P2P::sendFile, this, filePath, clientAddr).detach();
    }
}

/*
	SendFile function, it sends a file to a peer using a TCP connection.
*/
void P2P::sendFile(const std::string& filePath, sockaddr_in clientAddr) {
	// Create a new TCP socket
    SOCKET tcpServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpServerSocket == INVALID_SOCKET) {
        wxMessageBox("Nie uda³o siê utworzyæ socketu TCP do wys³ania pliku.", "B³¹d", wxOK | wxICON_ERROR);
        return;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(0);

    if (bind(tcpServerSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        wxMessageBox("Nie uda³o siê zwi¹zaæ socketu TCP.", "B³¹d", wxOK | wxICON_ERROR);
        closesocket(tcpServerSocket);
        return;
    }

    int addrLen = sizeof(serverAddr);
    getsockname(tcpServerSocket, (SOCKADDR*)&serverAddr, &addrLen);
    unsigned short assignedPort = ntohs(serverAddr.sin_port);

    if (listen(tcpServerSocket, SOMAXCONN) == SOCKET_ERROR) {
        wxMessageBox("Nie uda³o siê rozpocz¹æ nas³uchiwania na socketcie TCP.", "B³¹d", wxOK | wxICON_ERROR);
        closesocket(tcpServerSocket);
        return;
    }

	// Send the assigned TCP port (for file transfer) to the client using the UDP socket
    std::string portMessage = "FILE_PORT:" + std::to_string(assignedPort);
    sendto(udpSocket, portMessage.c_str(), static_cast<int>(portMessage.size()), 0, (SOCKADDR*)&clientAddr, sizeof(clientAddr));

    SOCKET clientSocket = accept(tcpServerSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        wxMessageBox("Nie uda³o siê zaakceptowaæ po³¹czenia od klienta.", "B³¹d", wxOK | wxICON_ERROR);
        closesocket(tcpServerSocket);
        return;
    }

	// Send the file to the client
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        wxMessageBox("Nie uda³o siê otworzyæ pliku do odczytu.", "B³¹d", wxOK | wxICON_ERROR);
        closesocket(clientSocket);
        closesocket(tcpServerSocket);
        return;
    }

    char buffer[4096];
    while (inFile.read(buffer, sizeof(buffer))) {
        send(clientSocket, buffer, sizeof(buffer), 0);
    }
    int remainingBytes = static_cast<int>(inFile.gcount());
    if (remainingBytes > 0) {
        send(clientSocket, buffer, remainingBytes, 0);
    }

    inFile.close();
    closesocket(clientSocket);
    closesocket(tcpServerSocket);
}

/*
	DownloadFile function, it downloads a file from a peer using a TCP connection.
*/
void P2P::downloadFile(const std::string& fileInfo, const std::string& downloadPath) {
    size_t spacePos = fileInfo.find(' ');
    if (spacePos == std::string::npos) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nieprawid³owy format informacji o pliku.", "B³¹d", wxOK | wxICON_ERROR);
            });
        return;
    }

    std::string peerInfo = fileInfo.substr(0, spacePos);
    std::string fileName = fileInfo.substr(spacePos + 1);

    size_t colonPos = peerInfo.find(':');
    if (colonPos == std::string::npos) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nieprawid³owy format informacji o peerze.", "B³¹d", wxOK | wxICON_ERROR);
            });
        return;
    }

    std::string peerIP = peerInfo.substr(0, colonPos);
    unsigned short peerPort = static_cast<unsigned short>(std::stoi(peerInfo.substr(colonPos + 1)));

	// Create a temporary UDP socket to send a GET_FILE request
    SOCKET tempUdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (tempUdpSocket == INVALID_SOCKET) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê utworzyæ tymczasowego socketu UDP.", "B³¹d", wxOK | wxICON_ERROR);
            });
        return;
    }

	// Get a first available port for the temporary UDP socket
    sockaddr_in tempAddr;
    tempAddr.sin_family = AF_INET;
    tempAddr.sin_addr.s_addr = INADDR_ANY;
    tempAddr.sin_port = htons(0);

    if (bind(tempUdpSocket, (SOCKADDR*)&tempAddr, sizeof(tempAddr)) == SOCKET_ERROR) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê zwi¹zaæ tymczasowego socketu UDP.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tempUdpSocket);
        return;
    }
	wxTheApp->CallAfter([tempAddr]() {
		char tempAddrStr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &tempAddr.sin_addr, tempAddrStr, INET_ADDRSTRLEN);
		wxGetApp().Log("Temporary UDP socket bound to " + std::string(tempAddrStr) + ":" + std::to_string(ntohs(tempAddr.sin_port)));
		});

	// Send a GET_FILE request to the peer
    std::string requestMessage = "GET_FILE:" + fileName;

    sockaddr_in peerAddr;
    peerAddr.sin_family = AF_INET;
    inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);
    peerAddr.sin_port = htons(peerPort);

    int sendResult = sendto(tempUdpSocket, requestMessage.c_str(), static_cast<int>(requestMessage.size()), 0, (SOCKADDR*)&peerAddr, sizeof(peerAddr));
    wxTheApp->CallAfter([requestMessage, peerAddr]() {
        char peerAddrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peerAddr.sin_addr, peerAddrStr, INET_ADDRSTRLEN);
        wxGetApp().Log(requestMessage + " to " + std::string(peerAddrStr) + ":" + std::to_string(ntohs(peerAddr.sin_port)));
        });
    if (sendResult == SOCKET_ERROR) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê wys³aæ ¿¹dania GET_FILE.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tempUdpSocket);
        return;
    }

	// Set timeout for FILE_PORT response
    DWORD timeout = 10000; // 10 seconds
    setsockopt(tempUdpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	// Get the FILE_PORT response from the peer
    char buffer[1024];
    int peerAddrLen = sizeof(peerAddr);
    int bytesReceived = recvfrom(tempUdpSocket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR*)&peerAddr, &peerAddrLen);
    if (bytesReceived == SOCKET_ERROR) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie otrzymano odpowiedzi od peera.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tempUdpSocket);
        return;
    }

    buffer[bytesReceived] = '\0';
    std::string portMessage(buffer);
    if (portMessage.rfind("FILE_PORT:", 0) != 0) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Otrzymano nieprawid³ow¹ odpowiedŸ od peera.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tempUdpSocket);
        return;
    }

    unsigned short filePort = static_cast<unsigned short>(std::stoi(portMessage.substr(10)));

	// Close the temporary UDP socket
    closesocket(tempUdpSocket);

	// Create a new TCP socket to download the file
    SOCKET tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcpSocket == INVALID_SOCKET) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê utworzyæ socketu TCP.", "B³¹d", wxOK | wxICON_ERROR);
            });
        return;
    }

	// Set timeout for the TCP socket
    setsockopt(tcpSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

	// Connect to the peer on the file transfer port
    sockaddr_in fileServerAddr;
    fileServerAddr.sin_family = AF_INET;
    inet_pton(AF_INET, peerIP.c_str(), &fileServerAddr.sin_addr);
    fileServerAddr.sin_port = htons(filePort);

    if (connect(tcpSocket, (SOCKADDR*)&fileServerAddr, sizeof(fileServerAddr)) == SOCKET_ERROR) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê po³¹czyæ z peerem na porcie transferu pliku.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tcpSocket);
        return;
    }

	// Download the file and save it to the download path
    std::string fullFilePath = downloadPath + "\\" + fileName;

    std::ofstream outFile(fullFilePath, std::ios::binary);
    if (!outFile.is_open()) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Nie uda³o siê otworzyæ pliku do zapisu.", "B³¹d", wxOK | wxICON_ERROR);
            });
        closesocket(tcpSocket);
        return;
    }

    char fileBuffer[4096];
    int bytesRead;
    while ((bytesRead = recv(tcpSocket, fileBuffer, sizeof(fileBuffer), 0)) > 0) {
        outFile.write(fileBuffer, bytesRead);
    }

    outFile.close();
    closesocket(tcpSocket);

    if (bytesRead == SOCKET_ERROR) {
        wxTheApp->CallAfter([]() {
            wxMessageBox("Wyst¹pi³ b³¹d podczas pobierania pliku.", "B³¹d", wxOK | wxICON_ERROR);
            });
        return;
    }

    wxTheApp->CallAfter([fileName]() {
        wxMessageBox("Pomyœlnie pobrano plik: " + fileName, "Sukces", wxOK | wxICON_INFORMATION);
        });
}

/*
	Host function, it initializes the Winsock library, creates a UDP socket. It's used to create a new P2P network.
*/
void P2P::host() {
    std::string localIP;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        wxMessageBox("Failed to initialize Winsock.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;;
    serverAddr.sin_port = htons(0);

    bind(udpSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));

    int addrLen = sizeof(serverAddr);
    getsockname(udpSocket, (SOCKADDR*)&serverAddr, &addrLen);
	wxGetApp().Log("Hosted on " + std::to_string(ntohs(serverAddr.sin_port)));

    unsigned short port = ntohs(serverAddr.sin_port);

    char hostName[256];
    if (gethostname(hostName, sizeof(hostName)) == 0) {
        addrinfo hints = { 0 }, * res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        if (getaddrinfo(hostName, nullptr, &hints, &res) == 0) {
            sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), ipStr, sizeof(ipStr));
            localIP = ipStr;
            freeaddrinfo(res);
        }
        else {
            wxMessageBox("Failed to resolve hostname.", "Error", wxOK | wxICON_ERROR);
            return;
        }
    }
    else {
        wxMessageBox("Failed to get hostname.", "Error", wxOK | wxICON_ERROR);
        return;
    }

    myInfo = localIP + ":" + std::to_string(port);

    connectedUsers.push_back(myInfo);

    wxGetApp().SetConnectedGUI();
    wxGetApp().UpdatePeerList();

	// Start listening thread
    std::thread(&P2P::StartListening, this).detach();
}

/*
	Join function, it initializes the Winsock library, creates a UDP socket, and sends a JOIN message to the peer that the user wants to connect to.
*/
bool P2P::join(std::string peerInfo) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        wxMessageBox("Nie uda³o siê zainicjowaæ biblioteki Winsock.", "B³¹d", wxOK | wxICON_ERROR);
        return false;
    }

    size_t colonPos = peerInfo.find(':');
    if (colonPos != std::string::npos) {
        std::string peerIP = peerInfo.substr(0, colonPos);
        unsigned short hostPort = static_cast<unsigned short>(std::stoi(peerInfo.substr(colonPos + 1)));

        std::string localIP;

        // Pobranie lokalnego adresu IP
        char hostName[256];
        if (gethostname(hostName, sizeof(hostName)) == 0) {
            addrinfo hints = { 0 }, * res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
            if (getaddrinfo(hostName, nullptr, &hints, &res) == 0) {
                sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(res->ai_addr);
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(addr->sin_addr), ipStr, sizeof(ipStr));
                localIP = ipStr;
                freeaddrinfo(res);
            }
            else {
                wxMessageBox("Nie uda³o siê rozwi¹zaæ nazwy hosta.", "B³¹d", wxOK | wxICON_ERROR);
                WSACleanup();
                return false;
            }
        }
        else {
            wxMessageBox("Nie uda³o siê pobraæ nazwy hosta.", "B³¹d", wxOK | wxICON_ERROR);
            WSACleanup();
            return false;
        }

        udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket == INVALID_SOCKET) {
            wxMessageBox("Nie uda³o siê utworzyæ socketu.", "B³¹d", wxOK | wxICON_ERROR);
            WSACleanup();
            return false;
        }

        sockaddr_in peerAddr;
        peerAddr.sin_family = AF_INET;
        inet_pton(AF_INET, peerIP.c_str(), &peerAddr.sin_addr);
        peerAddr.sin_port = htons(hostPort);

        std::string joinMessage = "JOIN";
        int result = sendto(udpSocket, joinMessage.c_str(), static_cast<int>(joinMessage.size()), 0, (SOCKADDR*)&peerAddr, sizeof(peerAddr));
        if (result == SOCKET_ERROR) {
            wxMessageBox("Nie uda³o siê wys³aæ wiadomoœci JOIN.", "B³¹d", wxOK | wxICON_ERROR);
            closesocket(udpSocket);
            WSACleanup();
            return false;
        }
        else {
			// Get local IP and port and set myInfo
            sockaddr_in localAddr;
            int addrLen = sizeof(localAddr);
            getsockname(udpSocket, (SOCKADDR*)&localAddr, &addrLen);
            unsigned short localPort = ntohs(localAddr.sin_port);
            myInfo = localIP + ":" + std::to_string(localPort);

			// Create a new thread to start listening
            std::thread(&P2P::StartListening, this).detach();

            return true;
        }
    }
    WSACleanup();
    return false;
}

/*
	SearchFile function, it sends a SEARCH message to all connected peers in the network.
*/
void P2P::searchFile(std::string fileName) {
    std::string searchMessage = "SEARCH:" + fileName;
    for (const auto& user : connectedUsers) {
        if (user != myInfo)
        {
            size_t colonPos = user.find(':');
            if (colonPos != std::string::npos) {
                wxTheApp->CallAfter([searchMessage, user]() {
                    wxGetApp().Log(searchMessage + " to " + user);
                    });
                std::string userIP = user.substr(0, colonPos);
                unsigned short userPort = static_cast<unsigned short>(std::stoi(user.substr(colonPos + 1)));

                sockaddr_in userAddr;
                userAddr.sin_family = AF_INET;
                inet_pton(AF_INET, userIP.c_str(), &userAddr.sin_addr);
                userAddr.sin_port = htons(userPort);

                sendto(udpSocket, searchMessage.c_str(), static_cast<int>(searchMessage.size()), 0, (SOCKADDR*)&userAddr, sizeof(userAddr));
            }
        }
    }
}

/*
	ShareFile function, it adds a file to the sharedFiles list and notifies the user that the file has been shared.
*/
void P2P::shareFile(wxFileDialog& dialog) {
    std::string filePath = dialog.GetPath().ToStdString();
    std::string fileName = dialog.GetFilename().ToStdString();
    sharedFiles.push_back(fileName + ":" + filePath);
    wxMessageBox("Plik udostêpniony: " + fileName, "Udostêpnianie pliku", wxOK | wxICON_INFORMATION);
}