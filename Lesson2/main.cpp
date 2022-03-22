#include <iostream>
#include <fstream>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_packet.h>
#include <netinet/if_ether.h>

#include <netinet/in.h>
#include <cerrno>
#include <cstring>

#include <chrono>

struct pcap_timeval
    {
        int32_t tv_sec;
        int32_t tv_usec;
    };

struct pcap_file_header
{
	const uint32_t magic = 0xa1b2c3d4;
	const uint16_t version_major = 2;
	const uint16_t version_minor = 4;
	int32_t thiszone = 0;
	uint32_t sigfigs = 0;
	uint32_t snaplen = 256 * 256 - 1;
	uint32_t linktype = 1;
};


struct pcap_sf_pkthdr
{
	struct pcap_timeval ts;
	uint32_t caplen;
	uint32_t len;
};

const auto BUFFER_SIZE_HDR = sizeof(pcap_sf_pkthdr);
const auto BUFFER_SIZE_ETH = 14;
const auto BUFFER_SIZE_IP = (256 * 256 - 1) - BUFFER_SIZE_ETH;
const auto BUFFER_OFFSET_ETH = sizeof(pcap_sf_pkthdr);
const auto BUFFER_OFFSET_IP = BUFFER_OFFSET_ETH + BUFFER_SIZE_ETH;

//поиск и возврат интерфейса
struct ifreq get_ifr(const std::string &if_name, int sock)
{
    struct ifreq ifr = {0};
    std::copy(if_name.begin(),if_name.end(), ifr.ifr_name);
    if (-1 == ioctl(sock, SIOCGIFINDEX, &ifr))
    {
        std::cerr << "Unable to find interface '" << if_name << "'." << std::endl;
    }
    
    return ifr;
};

//получение индекса интерфейса
auto get_if_index(const std::string& if_name, int sock)
{
    const auto if_index = get_ifr(if_name, sock).ifr_ifindex;
    std::cout << "Device index = " << if_index << std::endl;
    
    return if_index;
};

//получение структуры адреса для bind
auto get_if_address(const std::string& if_name, int sock)
{
    struct sockaddr_ll iface_addr =
    {
        .sll_family = AF_PACKET,
        .sll_protocol = htons(ETH_P_IP),
        .sll_ifindex = get_if_index(if_name, sock),
        // Captured IP packets sent and received by the network interface the
        // specified IP address is associated with.
        //.sin_addr = { .s_addr = *reinterpret_cast<const in_addr_t*>(remote_host->h_addr) }
    };

    return iface_addr;
};

class Sniffer
{
public:

    Sniffer(const std::string &if_name, const std::string &pcap_filename, const int socket_descriptor):
            if_name_(if_name), pcap_filename_(pcap_filename),sock_descriptor_(socket_descriptor), of_(pcap_filename_, std::ios::binary)
    {
        init();
    }

    ~Sniffer()
    {
        stop_capture();
    }

    bool initialized() const { return initialized_; }

    bool start_capture()
    {
        if (started_ || !initialized_) return false;
        started_ = true;

        if (!switch_promisc(true)) return false;
        std::cout << "Starting capture on interface " << if_name_ << std::endl;

        while (started_) 
        {
            if (!capture()) return false;
        }

        return true;
    }

    bool stop_capture()
    {
        if (!started_) return false;
        started_ = false;
        switch_promisc(false);

        return true;
    }


    bool switch_promisc(bool enabled)
    {
        //структура интерфейса
    auto interface = get_ifr(if_name_, sock_descriptor_);
    
    //запись имени сетевого интерфейса
    //strncpy(interface.ifr_name, (interface.ifr_name).c_str(), sizeof(intrfc_name));
    
    //поиск сетевого интерфейса по имени
    if (ioctl(sock_descriptor_, SIOGIFINDEX, &interface) < 0)
    {
        std::cout << "Interfaces status: " << strerror(errno) << std::endl;
        return false;
    }

    //считывание флагов сетевого интерфейса
    if (ioctl(sock_descriptor_, SIOCGIFFLAGS, &interface) < 0)
    {
        std::cout << "Flags status: " << strerror(errno) << std::endl;
        return false;
    }

    //вывод текущих флагов
    //std::cout << interface.ifr_ifru.ifru_flags << std::endl;
    
    //установка флагов
    if (enabled)
        interface.ifr_ifru.ifru_flags |= IFF_PROMISC;
    else 
        interface.ifr_flags &= ~IFF_PROMISC;

    //установка флагов на интерфейс
    if (ioctl(sock_descriptor_, SIOCSIFFLAGS, &interface) < 0)
    {    
        std::cout << "Promisc mode status: " << strerror(errno) << std::endl;
        return false;
    }

    //вывод текущих флагов
    //std::cout << interface.ifr_ifru.ifru_flags << std::endl;
    return true;
    }
    bool capture()
    {
        char buffer[BUFFER_SIZE_HDR + (256 * 256 - 1)] = {0};
        buffer[BUFFER_OFFSET_ETH + ethernet_proto_type_offset] = 0x08;
        struct pcap_sf_pkthdr* pkt = reinterpret_cast<struct pcap_sf_pkthdr*>(buffer);

        //read the next packet
        int rc = recv(sock_descriptor_, buffer + BUFFER_OFFSET_ETH, BUFFER_SIZE_IP, 0);

        if (rc == -1) 
        {
            std::cerr << "recv() failed: " << strerror(errno) << std::endl;
            return false;
        }

        if (!rc) return false;

        std::cout << rc << "bytes received..." << std::endl;

        using namespace std::chrono;
        auto cur_time = duration_cast<microseconds>(time_point_cast<microseconds>(system_clock::now()).time_since_epoch());
        auto t_s = seconds(duration_cast<seconds>(cur_time));
        auto u_s = cur_time - duration_cast<microseconds>(t_s);

        pkt->ts.tv_sec = t_s.count();
        pkt->ts.tv_usec = u_s.count();
        pkt->caplen = rc + BUFFER_SIZE_HDR;
        pkt->len = rc + BUFFER_SIZE_HDR;
        of_.write(reinterpret_cast<const char*>(buffer), rc + (BUFFER_SIZE_HDR*2));
        //of_ << std::ios::hex <<(reinterpret_cast<const char*>(buffer)) << std::endl;
        of_.flush();

        return true;
    }

protected:
    bool init()
    {
        if (!bind_socket()) return false;
        if (!switch_promisc(true)) return false;
        if (!write_pcap_header()) return false;
        initialized_ = true;

        return true;
    }

    bool bind_socket()
    {
        const auto iface_addr = get_if_address(if_name_, sock_descriptor_);
        if (sock_descriptor_ < 0)
        {
            std::cerr << "Socket error." << std:: endl;
            return false;
        }

        //check too long

        if (-1 == bind(sock_descriptor_, reinterpret_cast<const struct sockaddr*>(&iface_addr), sizeof(iface_addr)))
        {
            std::cerr << "bind failed " << std::endl;
            return false;
        }
        return true;
    }
    
    bool write_pcap_header()
    {
        if (!of_) 
        {
            std::cerr << "\"" << pcap_filename_ << "\"" << "failed" << std::endl;
            return false; 
        }
        try 
        {
            //Disable buffering
            of_.rdbuf()->pubsetbuf(0,0);

            //Create a PCAP file header
            struct pcap_file_header hdr;

            //Write the PCAP file header to our capture file.
            //of_ << std::ios::hex <<(reinterpret_cast<const char*>(&hdr)) << std::endl;
            of_.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
        }
        catch(const std::ios_base::failure& fail)
        {
            std::cerr << fail.what() << std::endl;
            return false;
        }

        return true;
    }

private:
    const size_t ethernet_proto_type_offset = 12;

    int sock_descriptor_;
    const std::string pcap_filename_;
    const std::string if_name_;
    
    std::ofstream of_;
    bool started_ = false;
    bool initialized_ = false;
};

int main() 
{
    int sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    try 
    {
        Sniffer sniffer("eth0", "pcap_file.pcap", sock);
        sniffer.start_capture();
    }
    catch(...)
    {
        std::cerr << "Unknown exception!" << std::endl;
        return EXIT_FAILURE;
    }
    //std::cout << set_promisc(true, sock, "eth0") << std::endl;
    
    return EXIT_SUCCESS;
}