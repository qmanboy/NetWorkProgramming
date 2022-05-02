#include <bitset>
#include <iostream>
#include <fstream>
#include <sys/socket.h> // for socket
#include <netinet/tcp.h> // for tcp_nodelay
#include <netinet/in.h> // for sockaddr_in
#include <cstring> //for string operations
#include <netdb.h> //for gethostbyname
#include <arpa/inet.h> //for inet_ntoa/aton
#include <unistd.h> //for close(sock)
#include <cassert> //for assert

bool check_buffer(char* buf, int buf_size, std::string string)  //check the sum of recv bytes from server
{
    for (size_t idx = 0; idx < buf_size; idx++)
    {
        if (buf[idx] != string[idx])
            return false;
    }
    return true;
}

class NewFile
{
    private:
        std::ofstream m_file;
        std::string m_filename;
    public:
        NewFile(std::string filename) 
        : m_filename(filename)
        {
            m_file.open(filename);
        }
        ~NewFile()
        {
            m_file.close();
        }
        void add_info(std::string data)
        {
            m_file << data;
        }
};

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
    
    std::string filepath{"/home/serge/dev/mai2n.cpp"};
    
    std::string request{};
    char buff[1024] = {0}; //buff for Rx blocksize

    auto recv_bytes = recv(sock, buff, sizeof(buff),0);
    buff[recv_bytes] = '\0';

    int block_size = std::stoi(buff); //get the blocksize from recv
    char buffer[block_size]{};
    
    std::cout << "block_size = " << block_size << std::endl;                                                      
    
    do 
    {
        std::cout << "Download file from server? (\"yes\" or \"exit\")\n>";
        std::getline(std::cin, request);
        if (request == "yes")
        {   
            NewFile file("/home/serge/dev/download.cpp");
            
            send(sock, filepath.c_str(), filepath.length(), 0);                                           //sending $filepath to server
            
            uint16_t sum_bytes = 0;

            while (true)
            {
                auto recv_bytes = recv(sock, buffer, sizeof(buffer),0);                                     //write to $buffer answer from server, $recv_bytes - answer's size in bytes
                sum_bytes += recv_bytes;

                buffer[recv_bytes] = '\0';

                if (check_buffer(buffer, recv_bytes, "File not found"))
                {
                    std::cout << buffer << std::endl;
                    break;
                }                 
                
                std::string sum = std::to_string(sum_bytes);

                if (check_buffer(buffer, recv_bytes, sum))
                {
                    std::cout << "Download finished" << std::endl;
                    break;
                }
                file.add_info(buffer);
            }
        }
        else if (request == "exit")  
        {
            send(sock, request.c_str(), request.length(), 0);
        }

    } while (request != "exit");
    
    close(sock);                                                                                //close tcp socket

    return EXIT_SUCCESS;
}