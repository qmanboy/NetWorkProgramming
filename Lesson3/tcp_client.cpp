#include <bitset>
#include <iostream>
#include <sys/socket.h> // for socket
#include <netinet/tcp.h> // for tcp_nodelay
#include <netinet/in.h> // for sockaddr_in
#include <cstring> //for string operations
#include <netdb.h> //for gethostbyname
#include <arpa/inet.h> //for inet_ntoa/aton
#include <unistd.h> //for close(sock)
#include <cassert> //for assert

addrinfo* connect_to_target_server(const std::string hostname, ushort port, int &sockfd)
{
    addrinfo hints =                                                                            //if hints - set the .ai parameters for sure
    {   
        .ai_flags = AI_CANONNAME,                                                               //hostname 
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP
    };

    std::string endIp;
    
    addrinfo *servinfo = nullptr;                                                               // w/o hints
    int status = 0;
    bool connected = false;

    if ((status = getaddrinfo(hostname.c_str(), nullptr, &hints, &servinfo)) != 0)              // get and resolve the host parameters to $servinfo(instead of gethostbyname)
    
    {
        std::string msg("Getaddrinfo error: " + status);
        throw std::runtime_error(msg);
    }

    for (auto const *s = servinfo; s != nullptr; s = s->ai_next)
    {
        assert(s->ai_family == s->ai_addr->sa_family);

        if (AF_INET == s->ai_family)                                                            //check IPv4 
        {
            char ip[INET_ADDRSTRLEN];                                                           //INET_ADDRSTRLEN - length of ip char data
            
            sockaddr_in *const sin = reinterpret_cast<sockaddr_in* const>(s->ai_addr);          //write to sin sockaddr_in from $servinfo

            sin->sin_family = s->ai_family;
            sin->sin_port = htons(port);

            std::cout << "Trying IP address: " << inet_ntop(sin->sin_family, &(sin->sin_addr) , ip, INET_ADDRSTRLEN) << std::endl;
            

            if (connect(sockfd, reinterpret_cast<sockaddr*>(sin), sizeof(sockaddr_in)) == 0)    //connect to server 
            {
                connected = true;
                break;
            }
        }
        else if (AF_INET6 == s->ai_family)
        {
            char ip6[INET6_ADDRSTRLEN];
            sockaddr_in6 *const sin6 = reinterpret_cast<sockaddr_in6* const>(s->ai_addr);

            sin6->sin6_family = s->ai_family;
            sin6->sin6_port = htons(port);

            std::cout << "Trying IPv6 address: " << inet_ntop(sin6->sin6_family, &(sin6->sin6_addr) , ip6, INET6_ADDRSTRLEN) << std::endl;
            
            if (connect(sockfd, reinterpret_cast<sockaddr*>(sin6), sizeof(sockaddr_in6)) == 0)  //connect to server 
            {
                connected = true;
                break;
            }
        }
    } //for
    
    if (!connected) 
    {
        std::string msg{"Connection error: " + errno};
        throw std::runtime_error(msg);
    }
    else
    {
        std::cout << "Connected to server " << servinfo->ai_canonname << " port " << port << std::endl;
    }
    return servinfo;
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "Data is not full" << std::endl;
        return EXIT_FAILURE;    
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);                                       //open tcp socket

    addrinfo* serverinfo = connect_to_target_server(argv[1], std::stoi(argv[2]), sock);                         //connect to server function, return server's addrinfo struct     
    
    std::string hostname = serverinfo->ai_canonname;                                            //get $hostname to string for request formation
    std::string request = { "GET / HTTP/1.1\r\nHost: " + hostname + "\r\n\r\n" };               // \r - too impotant for correct request(apache "400 bad request" w/o \r)
    
    do 
    {
        std::cout << "Input request to server\n>";
        std::getline(std::cin, request);       
        
        send(sock, request.c_str(), request.length(), 0);                                           //sending $request to server

        if (request != "exit") 
        {
            std::cout << "Answer from server:" << std::endl;
            char buffer[256] = {0};                                                                     //buffer for answer from server
            auto recv_bytes = recv(sock, buffer, sizeof(buffer),0);                                     //write to $buffer answer from server, $recv_bytes - answer's size in bytes
            
            buffer[recv_bytes] = '\0';                                                                  //add "feof" in buffer's end 
            std::cout << buffer << std::endl; 
        }
        
    } while (request != "exit");
    
    close(sock);                                                                                //close tcp socket

    return EXIT_SUCCESS;
}