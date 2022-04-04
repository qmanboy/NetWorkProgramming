#include <iostream>
#include <sys/socket.h> // for socket
#include <netinet/tcp.h> // for tcp_nodelay
#include <netinet/in.h> // for sockaddr_in
#include <cstring> //for string operations
#include <netdb.h> //for gethostbyname, addrinfo
#include <arpa/inet.h> //for inet_ntoa/aton
#include <unistd.h> //for close(sock)

int main(int argc, char* argv[])
{
    int sock, sock6, newsock;
    if (argc < 2)
    {
        std::cout << "Port not exist" << std::endl;
        return EXIT_FAILURE;    
    }
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    std::string port = argv[1];
    sock6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_IPV6);

    struct sockaddr_in servaddress = {0};   //server socket info
    struct sockaddr_in clientaddress = {0}; //client socket info                                             
    socklen_t client_len = sizeof(clientaddress);


    struct sockaddr_in6 servaddr6 = {0};
    struct sockaddr_in6 clientaddr6 = {0};
    socklen_t client_len6 = sizeof(clientaddr6);

    servaddress.sin_addr.s_addr = inet_addr("127.0.0.1");  
    servaddress.sin_family = AF_INET;                                               //IPv4
    servaddress.sin_port = htons(std::stoi(port));                                     //(htons(port_number))host_to_network_short cast

    servaddr6.sin6_addr = in6addr_any;
    servaddr6.sin6_family = AF_INET6;                                              //IPv4
    servaddress.sin_port = htons(std::stoi(port)); 
    if (bind(sock, reinterpret_cast<sockaddr*>(&servaddress), sizeof(servaddress)) < 0)
    {
        throw std::runtime_error("Bind is not OK");
        return EXIT_FAILURE;
    }

    std::cout << "Bind OK" << std::endl;
    if (listen(sock,10) < 0)
    {
        throw std::runtime_error("Listen is not OK");
        return EXIT_FAILURE;
    }
    
    while (true) 
    {
        std::cout << "Server is listening on " << inet_ntoa(servaddress.sin_addr) << " port " << ntohs(servaddress.sin_port) << "..." << std::endl;
        newsock = accept(sock, reinterpret_cast<sockaddr*>(&clientaddress), &client_len);
        if (newsock < 0)
        {
            throw std::runtime_error("Accept error");
        }
        else 
        {
            std::cout << "Client ip " << inet_ntoa(clientaddress.sin_addr) << " port " << clientaddress.sin_port << std::endl;
            char buf[128];
            std::string answer;
            do
            {
                std::cout << "Waiting request..." << std::endl;
                auto recv_bytes = recv(newsock, buf, sizeof(buf),0);
                std::cout << "Bytes received: " << recv_bytes << std::endl;
                buf[recv_bytes] = '\0';
                std::cout << "Client send request:\n" << buf  << std::endl;
                answer = buf;
                if (answer == "exit") 
                    std::cout << "Close connection with " << inet_ntoa(clientaddress.sin_addr) << std::endl;
                else    
                {
                    std::cout << "Server send answer.." << std::endl;
                    send(newsock, buf, recv_bytes, 0);
                } 

            } while (answer != "exit"); 
        }
        close(newsock);
    }
    close(sock);
    return EXIT_SUCCESS;
}