/*
 * Full-Featured Traceroute Implementation in C
 * 
 * Complete traceroute with multiple queries per hop, configurable timeouts,
 * and detailed output formatting.
 * 
 * Compile: gcc -o traceroute traceroute.c -lws2_32 (Windows)
 *          gcc -o traceroute traceroute.c (Linux)
 * 
 * Run: traceroute.exe google.com -m 20 -q 3 -t 2 (Windows as Admin)
 *      sudo ./traceroute google.com -m 20 -q 3 -t 2 (Linux)
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
    typedef int socklen_t;
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
#define DEFAULT_TIMEOUT 2
#define DEFAULT_QUERIES 3
#define PACKET_SIZE 64
#define RECV_BUFFER_SIZE 512

/* Configuration structure */
typedef struct {
    char destination[256];
    int max_hops;
    int timeout_sec;
    int num_queries;
    int packet_size;
} traceroute_config_t;

/* ICMP Header */
typedef struct {
    unsigned char type;
    unsigned char code;
    unsigned short checksum;
    unsigned short id;
    unsigned short sequence;
} icmp_header_t;

/* Probe result */
typedef struct {
    char ip_addr[64];
    double rtt;
    int received;
} probe_result_t;

/* Calculate checksum */
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

/* Create ICMP Echo Request */
int create_icmp_packet(char *packet, int packet_id, int sequence, int size) {
    icmp_header_t *icmp = (icmp_header_t *)packet;
    char *data = packet + sizeof(icmp_header_t);
    int data_len = size - sizeof(icmp_header_t);
    
    icmp->type = 8;  /* Echo Request */
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = packet_id;
    icmp->sequence = sequence;
    
    /* Fill data with pattern */
    for (int i = 0; i < data_len; i++) {
        data[i] = 'A' + (i % 26);
    }
    
    icmp->checksum = calculate_checksum((unsigned short *)packet, size);
    
    return size;
}

/* Resolve hostname to IP */
int resolve_hostname(const char *hostname, char *ip_addr) {
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) {
        return -1;
    }
    
    struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
    if (addr_list[0] != NULL) {
        strcpy(ip_addr, inet_ntoa(*addr_list[0]));
        return 0;
    }
    
    return -1;
}

/* Reverse DNS lookup */
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

/* Perform single probe */
int probe_once(const char *dest_ip, int ttl, int packet_id, 
               int sequence, int timeout_sec, int packet_size,
               probe_result_t *result) {
    int send_sock, recv_sock;
    struct sockaddr_in dest_addr, recv_addr;
    char packet[256];
    char recv_buffer[RECV_BUFFER_SIZE];
    struct timeval timeout, start_time, end_time;
    socklen_t addr_len = sizeof(recv_addr);
#ifdef _WIN32
    DWORD tick_start, tick_end;
#endif
    
    /* Initialize result */
    result->received = 0;
    result->rtt = 0.0;
    strcpy(result->ip_addr, "");
    
    /* Create sockets */
    send_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    recv_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    
    if (send_sock < 0 || recv_sock < 0) {
        return -1;
    }
    
    /* Set TTL */
    if (setsockopt(send_sock, IPPROTO_IP, IP_TTL, 
                   (char *)&ttl, sizeof(ttl)) < 0) {
        close(send_sock);
        close(recv_sock);
        return -1;
    }
    
    /* Set timeout */
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;
    setsockopt(recv_sock, SOL_SOCKET, SO_RCVTIMEO, 
               (char *)&timeout, sizeof(timeout));
    
    /* Setup destination */
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    
    /* Create and send packet */
    int pkt_len = create_icmp_packet(packet, packet_id, sequence, packet_size);
    
#ifdef _WIN32
    tick_start = GetTickCount();
#else
    gettimeofday(&start_time, NULL);
#endif
    
    if (sendto(send_sock, packet, pkt_len, 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        close(send_sock);
        close(recv_sock);
        return -1;
    }
    
    /* Receive response */
    int bytes = recvfrom(recv_sock, recv_buffer, sizeof(recv_buffer), 0,
                        (struct sockaddr *)&recv_addr, &addr_len);
    
#ifdef _WIN32
    tick_end = GetTickCount();
#else
    gettimeofday(&end_time, NULL);
#endif
    
    if (bytes > 0) {
#ifdef _WIN32
        result->rtt = (double)(tick_end - tick_start);
#else
        result->rtt = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                      (end_time.tv_usec - start_time.tv_usec) / 1000.0;
#endif
        result->received = 1;
        strcpy(result->ip_addr, inet_ntoa(recv_addr.sin_addr));
    }
    
    close(send_sock);
    close(recv_sock);
    
    return 0;
}

/* Probe a single hop multiple times */
void probe_hop(const char *dest_ip, int ttl, traceroute_config_t *config,
               probe_result_t *results) {
    for (int i = 0; i < config->num_queries; i++) {
        if (probe_once(dest_ip, ttl, 12345, ttl * 10 + i,
                      config->timeout_sec, config->packet_size,
                      &results[i]) < 0) {
            results[i].received = 0;
        }
        
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
    }
}

/* Print results for a hop */
void print_hop_results(int ttl, probe_result_t *results, int num_queries,
                      const char *dest_ip) {
    char hostname[256];
    char unique_ips[10][64];
    int num_unique = 0;
    
    printf("%2d  ", ttl);
    
    /* Find unique IP addresses */
    for (int i = 0; i < num_queries; i++) {
        if (results[i].received) {
            int found = 0;
            for (int j = 0; j < num_unique; j++) {
                if (strcmp(unique_ips[j], results[i].ip_addr) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found && num_unique < 10) {
                strcpy(unique_ips[num_unique++], results[i].ip_addr);
            }
        }
    }
    
    /* Print hostnames for unique IPs */
    for (int i = 0; i < num_unique; i++) {
        get_hostname(unique_ips[i], hostname, sizeof(hostname));
        if (strcmp(hostname, unique_ips[i]) != 0) {
            printf("%s (%s)", hostname, unique_ips[i]);
        } else {
            printf("%s", unique_ips[i]);
        }
        if (i < num_unique - 1) {
            printf("  ");
        }
    }
    
    if (num_unique > 0) {
        printf("  ");
    }
    
    /* Print RTT for each query */
    for (int i = 0; i < num_queries; i++) {
        if (results[i].received) {
            printf("%.3f ms", results[i].rtt);
        } else {
            printf("*");
        }
        
        if (i < num_queries - 1) {
            printf("  ");
        }
    }
    
    printf("\n");
}

/* Check if destination reached */
int check_destination_reached(probe_result_t *results, int num_queries,
                              const char *dest_ip) {
    for (int i = 0; i < num_queries; i++) {
        if (results[i].received && 
            strcmp(results[i].ip_addr, dest_ip) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Main traceroute function */
void traceroute(traceroute_config_t *config) {
    char dest_ip[64];
    probe_result_t *results;
    int reached = 0;
    
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return;
    }
#endif
    
    /* Print header */
    printf("Traceroute to %s, %d hops max, %d byte packets\n",
           config->destination, config->max_hops, config->packet_size);
    
    /* Resolve destination */
    if (resolve_hostname(config->destination, dest_ip) != 0) {
        fprintf(stderr, "Error: Cannot resolve hostname '%s'\n", 
                config->destination);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    printf("Destination IP: %s\n\n", dest_ip);
    
    /* Allocate results array */
    results = (probe_result_t *)malloc(sizeof(probe_result_t) * 
                                       config->num_queries);
    if (!results) {
        fprintf(stderr, "Memory allocation failed\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    
    /* Probe each hop */
    for (int ttl = 1; ttl <= config->max_hops && !reached; ttl++) {
        probe_hop(dest_ip, ttl, config, results);
        print_hop_results(ttl, results, config->num_queries, dest_ip);
        
        if (check_destination_reached(results, config->num_queries, dest_ip)) {
            printf("\nReached destination %s\n", dest_ip);
            reached = 1;
        }
    }
    
    if (!reached) {
        printf("\nDid not reach destination within %d hops\n", 
               config->max_hops);
    }
    
    free(results);
    
#ifdef _WIN32
    WSACleanup();
#endif
}

/* Print usage information */
void print_usage(const char *program) {
    printf("Usage: %s <destination> [options]\n\n", program);
    printf("Options:\n");
    printf("  -m <max_hops>    Maximum number of hops (default: %d)\n", MAX_HOPS);
    printf("  -t <timeout>     Timeout in seconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -q <queries>     Number of queries per hop (default: %d)\n", DEFAULT_QUERIES);
    printf("  -s <size>        Packet size in bytes (default: %d)\n", PACKET_SIZE);
    printf("  -h               Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s google.com\n", program);
    printf("  %s 8.8.8.8 -m 20 -q 3 -t 2\n", program);
    printf("  %s example.com -m 15 -q 5\n\n", program);
    printf("Note: Requires Administrator/root privileges\n");
#ifdef _WIN32
    printf("      Run PowerShell as Administrator\n");
#else
    printf("      Run with: sudo %s <destination>\n", program);
#endif
}

/* Parse command line arguments */
int parse_args(int argc, char *argv[], traceroute_config_t *config) {
    /* Set defaults */
    config->max_hops = MAX_HOPS;
    config->timeout_sec = DEFAULT_TIMEOUT;
    config->num_queries = DEFAULT_QUERIES;
    config->packet_size = PACKET_SIZE;
    
    if (argc < 2) {
        return -1;
    }
    
    strcpy(config->destination, argv[1]);
    
    /* Parse options */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config->max_hops = atoi(argv[++i]);
            if (config->max_hops < 1 || config->max_hops > 255) {
                fprintf(stderr, "Error: max_hops must be 1-255\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config->timeout_sec = atoi(argv[++i]);
            if (config->timeout_sec < 1 || config->timeout_sec > 60) {
                fprintf(stderr, "Error: timeout must be 1-60 seconds\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {
            config->num_queries = atoi(argv[++i]);
            if (config->num_queries < 1 || config->num_queries > 10) {
                fprintf(stderr, "Error: queries must be 1-10\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            config->packet_size = atoi(argv[++i]);
            if (config->packet_size < 28 || config->packet_size > 1500) {
                fprintf(stderr, "Error: packet size must be 28-1500 bytes\n");
                return -1;
            }
        }
        else if (strcmp(argv[i], "-h") == 0) {
            return -1;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    traceroute_config_t config;
    
    if (parse_args(argc, argv, &config) != 0) {
        print_usage(argv[0]);
        return 1;
    }
    
    traceroute(&config);
    
    return 0;
}
