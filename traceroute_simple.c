/*
 * Simple Traceroute Implementation in C
 * 
 * A minimal educational implementation demonstrating how traceroute works
 * using raw sockets and ICMP packets with incrementing TTL values.
 * 
 * Compile: gcc -o traceroute_simple traceroute_simple.c -lws2_32 (Windows)
 *          gcc -o traceroute_simple traceroute_simple.c (Linux)
 * 
 * Run: traceroute_simple.exe google.com (Windows - as Administrator)
 *      sudo ./traceroute_simple google.com (Linux)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <netinet/ip_icmp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/time.h>
#endif

#define MAX_HOPS 30
#define TIMEOUT_SEC 2
#define PACKET_SIZE 64

/* ICMP Header Structure */
struct icmp_header {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short sequence;
};

/* Calculate Internet Checksum (RFC 1071) */
unsigned short calculate_checksum(unsigned short *buf, int len) {
    unsigned long sum = 0;
    
    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(unsigned char *)buf;
    }
    
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    
    return (unsigned short)(~sum);
}

/* Create ICMP Echo Request packet */
int create_icmp_packet(char *packet, int packet_id, int sequence) {
    struct icmp_header *icmp = (struct icmp_header *)packet;
    char *data = packet + sizeof(struct icmp_header);
    int data_len = PACKET_SIZE - sizeof(struct icmp_header);
    
    /* Fill ICMP header */
    icmp->type = 8;  /* Echo Request */
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = packet_id;
    icmp->sequence = sequence;
    
    /* Fill data */
    memset(data, 'T', data_len);
    
    /* Calculate checksum */
    icmp->checksum = calculate_checksum((unsigned short *)packet, PACKET_SIZE);
    
    return PACKET_SIZE;
}

/* Resolve hostname to IP address */
int resolve_hostname(const char *hostname, char *ip_addr) {
    struct hostent *host;
    struct in_addr **addr_list;
    
    host = gethostbyname(hostname);
    if (host == NULL) {
        return -1;
    }
    
    addr_list = (struct in_addr **)host->h_addr_list;
    if (addr_list[0] != NULL) {
        strcpy(ip_addr, inet_ntoa(*addr_list[0]));
        return 0;
    }
    
    return -1;
}

/* Get hostname from IP address (reverse DNS) */
void get_hostname(const char *ip_addr, char *hostname, int len) {
    struct hostent *host;
    struct in_addr addr;
    
    addr.s_addr = inet_addr(ip_addr);
    host = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);
    
    if (host != NULL && host->h_name != NULL) {
        strncpy(hostname, host->h_name, len - 1);
        hostname[len - 1] = '\0';
    } else {
        strncpy(hostname, ip_addr, len - 1);
        hostname[len - 1] = '\0';
    }
}

/* Perform traceroute to destination */
void traceroute(const char *destination, int max_hops) {
    int send_sock, recv_sock;
    struct sockaddr_in dest_addr, recv_addr;
    char packet[PACKET_SIZE];
    char recv_buffer[512];
    char ip_addr[64];
    char hostname[256];
    int ttl;
    int addr_len = sizeof(recv_addr);
    struct timeval timeout;
    struct timeval start_time, end_time;
    double rtt;
    int reached = 0;
    
    /* Windows compatibility - use DWORD for getting tick count */
#ifdef _WIN32
    DWORD tick_start, tick_end;
#endif
    
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return;
    }
#endif
    
    /* Resolve destination */
    printf("Traceroute to %s\n", destination);
    if (resolve_hostname(destination, ip_addr) != 0) {
        fprintf(stderr, "Error: Cannot resolve hostname '%s'\n", destination);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    printf("Destination IP: %s\n\n", ip_addr);
    
    /* Setup destination address */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(ip_addr);
    
    /* Iterate through TTL values */
    for (ttl = 1; ttl <= max_hops && !reached; ttl++) {
        printf("%2d  ", ttl);
        fflush(stdout);
        
        /* Create sockets */
        send_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        recv_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        
        if (send_sock < 0 || recv_sock < 0) {
            fprintf(stderr, "Error: Cannot create raw socket\n");
            fprintf(stderr, "Please run as Administrator/root\n");
#ifdef _WIN32
            WSACleanup();
#endif
            return;
        }
        
        /* Set TTL */
        if (setsockopt(send_sock, IPPROTO_IP, IP_TTL, 
                       (char *)&ttl, sizeof(ttl)) < 0) {
            fprintf(stderr, "Error setting TTL\n");
            close(send_sock);
            close(recv_sock);
            continue;
        }
        
        /* Set receive timeout */
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;
        setsockopt(recv_sock, SOL_SOCKET, SO_RCVTIMEO, 
                   (char *)&timeout, sizeof(timeout));
        
        /* Create and send ICMP packet */
        int packet_len = create_icmp_packet(packet, 12345, ttl);
        
#ifdef _WIN32
        tick_start = GetTickCount();
#else
        gettimeofday(&start_time, NULL);
#endif
        
        if (sendto(send_sock, packet, packet_len, 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            printf("* ");
            close(send_sock);
            close(recv_sock);
            continue;
        }
        
        /* Receive response */
        int bytes = recvfrom(recv_sock, recv_buffer, sizeof(recv_buffer), 0,
                            (struct sockaddr *)&recv_addr, &addr_len);
        
#ifdef _WIN32
        tick_end = GetTickCount();
        rtt = (double)(tick_end - tick_start);
#else
        gettimeofday(&end_time, NULL);
        rtt = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
              (end_time.tv_usec - start_time.tv_usec) / 1000.0;
#endif
        
        if (bytes > 0) {
            /* Get responding IP */
            char *resp_ip = inet_ntoa(recv_addr.sin_addr);
            
            /* Try reverse DNS lookup */
            get_hostname(resp_ip, hostname, sizeof(hostname));
            
            /* Print result */
            if (strcmp(hostname, resp_ip) != 0) {
                printf("%s (%s)  %.3f ms\n", hostname, resp_ip, rtt);
            } else {
                printf("%s  %.3f ms\n", resp_ip, rtt);
            }
            
            /* Check if we reached destination */
            if (strcmp(resp_ip, ip_addr) == 0) {
                reached = 1;
            }
        } else {
            printf("* * *\n");
        }
        
        close(send_sock);
        close(recv_sock);
        
        /* Small delay between probes */
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    
    if (reached) {
        printf("\nReached destination!\n");
    } else {
        printf("\nDid not reach destination within %d hops\n", max_hops);
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <destination> [max_hops]\n", argv[0]);
        printf("Example: %s google.com\n", argv[0]);
        printf("         %s 8.8.8.8 15\n", argv[0]);
        printf("\nNote: Requires Administrator/root privileges\n");
#ifdef _WIN32
        printf("      Run PowerShell as Administrator\n");
#else
        printf("      Run with: sudo %s <destination>\n", argv[0]);
#endif
        return 1;
    }
    
    const char *destination = argv[1];
    int max_hops = (argc > 2) ? atoi(argv[2]) : MAX_HOPS;
    
    if (max_hops < 1 || max_hops > 255) {
        fprintf(stderr, "Error: max_hops must be between 1 and 255\n");
        return 1;
    }
    
    traceroute(destination, max_hops);
    
    return 0;
}
