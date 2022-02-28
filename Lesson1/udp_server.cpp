#include <iostream>
#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT 27015

void close(SOCKET& _socket)
{
    closesocket(_socket);
    WSACleanup();
}

int main()
{
    constexpr int maxlen = 1024;
    
    std::cout << "Server started..." << std::endl;

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
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(DEFAULT_PORT);

    result = bind(SendRecvSocket, (sockaddr*)&ServerAddr, ServerAddrSize);
    
    if (result == SOCKET_ERROR)
    {
        std::cout << "Bind failed: " << WSAGetLastError() << std::endl;
        close(SendRecvSocket);
        return 2;
    }

    std::cout << "Bind - OK." << std::endl;

    while(true) 
    {
        result = recvfrom(SendRecvSocket, recvbuf, maxlen, 0, (sockaddr*)&ClientAddr, &ClientAddrSize);
        if (result > 0) 
        {
            recvbuf[result] = '\0';
            std::cout << "Get from " << inet_ntoa(ClientAddr.sin_addr) << " message: " << recvbuf << std::endl; //обратный резолв IP адреса клиента
            if (std::strcmp(recvbuf, "exit") == 0)  //реализация закрытия сервера
            {
                close(SendRecvSocket);
                return 0; 
            }
            if (sendto(SendRecvSocket, recvbuf, strlen(recvbuf), 0 , (sockaddr*)&ClientAddr, ClientAddrSize) < 1)
            {
                std::cout << "Client not found." << std::endl;
            };
        }
        else 
        {
            std::cout << "Recv failed: " << WSAGetLastError() << std::endl;
            close(SendRecvSocket);
            return 3;
        }
    }    
    delete recvbuf;
    delete result_string;
    return 0;
}