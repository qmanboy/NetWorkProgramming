#include <iostream>
#include <sys/socket.h> // for socket
#include <netinet/tcp.h> // for tcp_nodelay
#include <netinet/in.h> // for sockaddr_in
#include <cstring> //for string operations
#include <netdb.h> //for gethostbyname, addrinfo
#include <arpa/inet.h> //for inet_ntoa/aton
#include <unistd.h> //for close(sock)
#include <thread> //for threads
#include <future> //for future
#include <vector> //for tasks
#include <fstream> //for file
#include <filesystem> //for file_size

#define _BLOCKSIZE 2048

bool send_file(const std::string file_path, int& sock_descriptor, uint16_t block_size)
{
    uint16_t file_size = std::filesystem::file_size(std::filesystem::path(file_path));
    std::cout << "File size = " << file_size << std::endl;
    
    char buffer[block_size];
    std::ifstream file_stream(file_path);
    
    if (!file_stream) return false;

    std::cout << "Sending file " << file_path << "..." << std::endl;

    size_t buf_full_count = file_size/block_size;
    uint16_t file_endsize = file_size % block_size;

    size_t idx = 0;

    while (idx++ < buf_full_count)
    {
        file_stream.read(&buffer[0], block_size);
        send(sock_descriptor, buffer, block_size, 0);
    }

    char buff[file_endsize]{};
    file_stream.read(&buff[0], file_endsize);
    send(sock_descriptor, buff, file_endsize, 0);  

    file_stream.close();
    std::cout << "File sended..." << std::endl;
    return true;
}

bool check_filesize(uint16_t filesize, std::string str_filesize)
{
    return (filesize == std::stoi(str_filesize));
}

int accept_client(int &server_sock)
{
    struct sockaddr_storage client_addr;
    socklen_t client_addr_length = sizeof(client_addr);
    std::array<char, INET_ADDRSTRLEN> addr;
    int client_sock(accept(server_sock,
                    reinterpret_cast<sockaddr*>(&client_addr), &client_addr_length));
    std::cout << "Client from " << inet_ntop(AF_INET, &(reinterpret_cast<const
    sockaddr_in * const>(&client_addr)->sin_addr), &addr[0], addr.size())
    << " port " << ntohs(reinterpret_cast<const sockaddr_in * const>(&client_addr)->sin_port) << " connected..."
    << std::endl;
    return client_sock;
}

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
    servaddress.sin_port = htons(std::stoi(port));                                  //(htons(port_number))host_to_network_short cast

    servaddr6.sin6_addr = in6addr_any;
    servaddr6.sin6_family = AF_INET6;                                               //IPv4
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

    std::vector<std::future<void>> pending_tasks;

    std::cout << "Server listening on " << inet_ntoa(servaddress.sin_addr) << " port " << ntohs(servaddress.sin_port) << "..." << std::endl;
        
    
    while (true) 
    {
        auto newsock = std::move(accept_client(sock));
        if (newsock < 0)
        {
            throw std::runtime_error("Accept error");
        }
        else 
        {
            pending_tasks.push_back(std::async(std::launch::async, [&](int &&client_sock)
            {
                std::cout << "Client tid " << std::this_thread::get_id() << std::endl;
                
                char buf[_BLOCKSIZE];
                std::cout << "Sending blocksize to client..." << std::endl;
                std::string size = std::to_string(_BLOCKSIZE);
                
                if (send(client_sock, size.c_str(), size.size(), 0))
                {
                    std::cout << "Blocksize sending successfuly " << std::endl;
                }

                std::string file_path{};
                std::string answer{};
                do
                {
                    int recv_bytes{};
                    do 
                    {
                        recv_bytes = recv(client_sock, buf, sizeof(buf),0);
                    } 
                    while (recv_bytes < 0);
                    
                    buf[recv_bytes] = '\0';

                    std::cout << "Client id " << std::this_thread::get_id() << " send request: \"" << buf  << "\"" <<std::endl;
                    answer = buf;
                    if (answer == "exit") 
                        std::cout << "Close connection with client id " << std::this_thread::get_id() << std::endl;
                    else    
                    {
                        //sending file
                        if (!send_file(buf, client_sock, _BLOCKSIZE))
                        {
                            std::string file_not_found{"File not found"};
                            send(client_sock, file_not_found.c_str(), file_not_found.size() ,0);
                            continue;
                        }                            
                        
                        uint16_t filesize = std::filesystem::file_size(std::filesystem::path(buf));
                        
                        std::string fsize = std::to_string(filesize);
                        filesize += fsize.size();
                        fsize = std::to_string(filesize);
                        
                        std::this_thread::sleep_for(std::chrono::microseconds(5));
                        
                        send(client_sock, fsize.c_str(), fsize.size(),0);
                    }

                } while (answer != "exit");
            }, std::move(newsock)));
        }
        std::cout << "Switch tasks..." << std::endl;
        for (auto task = pending_tasks.begin(); task != pending_tasks.end();)
        {
            if (std::future_status::ready == task->wait_for(std::chrono::milliseconds(1)))
            {
                auto fu = task++;
                pending_tasks.erase(fu);
            }
            else ++task;
        }
    }
    close(sock);
    return EXIT_SUCCESS;
}