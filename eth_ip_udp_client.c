#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <sys/ioctl.h>

#define CLT_PORT 6666
#define SRV_PORT 1488

#define SRC_ADDR "10.42.0.1"
#define DST_ADDR "10.42.0.120"

#define ETH_FRAME_SIZE 526
#define IP_UDP_SIZE 512
#define MSG_SIZE   484

#define ETH_HEADER_LEN 14

#define SR_MAC0	0x74
#define SR_MAC1	0xf0
#define SR_MAC2	0x6d
#define SR_MAC3	0x92
#define SR_MAC4	0x84
#define SR_MAC5	0x57

#define DS_MAC0	0x94
#define DS_MAC1	0x39
#define DS_MAC2	0xe5
#define DS_MAC3	0x9b
#define DS_MAC4	0xab
#define DS_MAC5	0x67

/* * Сетевой интерфейс * */
#define DEFAULT_IF	"wlp4s0"

struct udpheader {
	unsigned short int src_port;   // порт источника
	unsigned short int dst_port;   // порт назначения
	unsigned short int length;     // длина заголовка UDP + данные
	unsigned short int csum;	   // контрольная сумма UDP (можно писать ноль)
};	// 8 bytes 

struct ipheader {
/* ihl(ip header length) - длина в 32-битных словах, version - версия протокола IP (IPv4) */
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int ihl:4;
	unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int version:4;
	unsigned int ihl:4;
#else
# error	"Please fix <bits/endian.h>"
#endif
	unsigned char      tos;         // type of sevice
	unsigned short int length;		// длина IP-датаграммы
	unsigned short int id;		    // идентификатор
	unsigned short int offset;		// используется для фрагментированных датаграмм
	unsigned char      ttl;			// время жизни (количество прыжков)
	unsigned char      protocol;	// используемый транспортный протокол
	unsigned short int csum;	    // контрольная сумма заголовка
	unsigned int       src_ip;		// IP-адрес источника
	unsigned int       dst_ip;		// IP-адрес назначения
};	// 20 bytes 

unsigned short int checksum(unsigned short int *buffer, int size) 
{
	uint32_t cksum = 0;
	while (size > 1) {
		cksum += *buffer++;
		size  -= sizeof(unsigned short int);   
	}
	if (size) {
		cksum += *(unsigned short int*)buffer;   
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
    
	return (unsigned short int)(~cksum);
}

int main()
{
	int                sockfd;
	struct ifreq       if_idx;
	struct ipheader    iph;
	struct udpheader   udph;
	struct sockaddr_ll addrll;
	struct sockaddr_in addr;
	char               message[MSG_SIZE], *ptr;
	unsigned short int udp_size;
	unsigned short int datagram_size, ip_size, udp_checksum;

	unsigned char src_mac[6] = {SR_MAC0, SR_MAC1, SR_MAC2, 
								SR_MAC3, SR_MAC4, SR_MAC5};
	unsigned char dst_mac[6] = {DS_MAC0, DS_MAC1, DS_MAC2, 
								DS_MAC3, DS_MAC4, DS_MAC5};

	void *buffer = (void *)calloc(ETH_FRAME_SIZE, 1);
	char *data = (char *)buffer + ETH_HEADER_LEN;

	sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sockfd == -1) {
		perror("socket");
		exit(-1);
	}

	strncpy(if_idx.ifr_name, DEFAULT_IF, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
		perror("SIOCGIFINDEX");
		exit(-1);
	}

	addrll.sll_family   = AF_INET;	
	addrll.sll_protocol = htons(ETH_P_IP);
	addrll.sll_ifindex  = if_idx.ifr_ifindex;
	addrll.sll_hatype   = ARPHRD_ETHER;
	addrll.sll_pkttype  = PACKET_HOST;
	addrll.sll_halen    = ETH_ALEN;

	/* * Destination MAC * */
	addrll.sll_addr[0] = src_mac[0];
	addrll.sll_addr[1] = src_mac[1];
	addrll.sll_addr[2] = src_mac[2];
	addrll.sll_addr[3] = src_mac[3];
	addrll.sll_addr[4] = src_mac[4];
	addrll.sll_addr[5] = src_mac[5];
	
	/* * MAC - end * */
	addrll.sll_addr[6]  = 0x00;
	addrll.sll_addr[7]  = 0x00;

	/* * set the frame header * */
	memcpy((void*)buffer, (void*)dst_mac, ETH_ALEN);
	memcpy((void*)(buffer + ETH_ALEN), (void*)src_mac, ETH_ALEN);

	char ether_frame[2];
	ether_frame[0] = ETH_P_IP / 256;
	ether_frame[1] = ETH_P_IP % 256;
	
	memcpy((void*)(buffer + ETH_ALEN + ETH_ALEN), (void*)ether_frame, 2);  

	bzero(message, MSG_SIZE);
	strcpy(message, "Hello (eth ip udp)");

	datagram_size = sizeof(iph) + sizeof(udph) + strlen(message); // общая длинна пакета
	ip_size       = sizeof(iph) / sizeof(unsigned int);
	udp_size      = sizeof(udph) + strlen(message); 

	/* заполняем ip заголовок */
	iph.version  = 4;
	iph.ihl      = ip_size;
	iph.tos      = 0;
	iph.length   = htons(datagram_size);
	iph.id       = 0;
	iph.offset   = 0;
	iph.ttl      = 128;
	iph.protocol = IPPROTO_UDP;
	iph.csum     = 0;
	inet_pton(AF_INET, SRC_ADDR, &(iph.src_ip));
	inet_pton(AF_INET, DST_ADDR, &(iph.dst_ip));

	iph.csum = checksum((unsigned short int *)&iph, iph.ihl * 4);

	/* заполняем udp заголовок */
	udph.src_port = htons(CLT_PORT);   
	udph.dst_port = htons(SRV_PORT);  
	udph.length   = htons(udp_size); 
	udph.csum     = 0; 

	ptr = data;

	/* * Подсчет чексуммы udp заголовка * */
	bzero(data, IP_UDP_SIZE);
	memcpy(ptr, &iph.src_ip, sizeof(iph.src_ip));  
	ptr          += sizeof(iph.src_ip);
	udp_checksum += sizeof(iph.src_ip);
	memcpy(ptr, &iph.dst_ip, sizeof(iph.dst_ip));
	ptr          += sizeof(iph.dst_ip);
	udp_checksum += sizeof(iph.dst_ip);
	ptr++;
	udp_checksum += 1;
	memcpy(ptr, &iph.protocol, sizeof(iph.protocol));
	ptr          += sizeof(iph.protocol);
	udp_checksum += sizeof(iph.protocol);
	memcpy(ptr, &udph.length, sizeof(udph.length));
	ptr          += sizeof(udph.length);
	udp_checksum += sizeof(udph.length);
	memcpy(ptr, &udph, sizeof(udph));
	ptr          += sizeof(udph);
	udp_checksum += sizeof(udph);
		
	int i;
	for(i = 0; i < strlen(message); i++, ptr++)
		*ptr = message[i];
			
	udp_checksum += strlen(message);
	udph.csum = checksum((unsigned short int *)data, udp_checksum); 
	/* * * * * * * * * * * * * * * * * * * */		
 
 	bzero(data, IP_UDP_SIZE);
	ptr = data;
	memcpy(ptr, &iph, sizeof(iph));
	ptr += sizeof(iph);
	memcpy(ptr, &udph, sizeof(udph));
	ptr += sizeof(udph);
	memcpy(ptr, message, strlen(message));

	datagram_size += ETH_HEADER_LEN;

	printf("send message to server: %s\n", message);
	if (sendto(sockfd, buffer, datagram_size, 0, (struct sockaddr *)&addrll, sizeof(addrll)) < 0) {
		perror("sendto");
		exit(-1);
	}

	if (close(sockfd) < 0) {
		perror("sendto");
		exit(-1);
	}

	exit(0);
}

