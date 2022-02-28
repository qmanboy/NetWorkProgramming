#include <iostream>
#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 27015

int main()
{
    constexpr int maxlen = 1024;

    WORD wVersionRequested = MAKEWORD(2, 2);
    WSADATA wsaData;
    auto result = WSAStartup(wVersionRequested, &wsaData);
    
    if (result != 0)
    {
        std::cout << "Error: Winsock DLL not found." << std::endl;
        return 1;
    }

    std::cout << "Winsock DLL - OK." << std::endl;

    SOCKET SendRecvSocket;
    sockaddr_in ServerAddr, ClientAddr;
    
    int ServerAddrSize = sizeof(ServerAddr);
    int ClientAddrSize = sizeof(ClientAddr);

    char* recvbuf = new char[maxlen];
    char* result_string = new char[maxlen];

    SendRecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ServerAddr.sin_port = htons(DEFAULT_PORT);

    while (true) 
    {
        result = recvfrom(SendRecvSocket, recvbuf, maxlen, 0, (sockaddr*)&ServerAddr, &ServerAddrSize);
        if (result > 0) 
        {
            recvbuf[result] = '\0';
            std::cout << "Get message from server " << inet_ntoa(ServerAddr.sin_addr) << ": " << recvbuf << std::endl;
        }
        std::cout << "Input message to sending > ";
        std::cin.getline(result_string, maxlen);
        sendto(SendRecvSocket, result_string, strlen(result_string), 0, (sockaddr*)&ServerAddr, sizeof(ServerAddr));
    }
    delete recvbuf;
    delete result_string;
    return 0;

}