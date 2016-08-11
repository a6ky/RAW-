#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define CLT_PORT 6666
#define SRV_PORT 1666
#define SRC_ADDR "127.0.0.1"
#define DST_ADDR "127.0.0.1"

#define DGRAM_SIZE 512
#define MSG_SIZE   484

struct udpheader {
	unsigned short int src_port;	// порт источника
	unsigned short int dst_port;	// порт назначения
	unsigned short int length;	// длина заголовка UDP + данные
	unsigned short int csum;	// контрольная сумма UDP (можно писать ноль)
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
	unsigned char      tos;		// type of sevice
	unsigned short int length;	// длина IP-датаграммы
	unsigned short int id;		// идентификатор
	unsigned short int offset;	// используется для фрагментированных датаграмм
	unsigned char      ttl;		// время жизни (количество прыжков)
	unsigned char      protocol;	// используемый транспортный протокол
	unsigned short int csum;	// контрольная сумма заголовка
	unsigned int       src_ip;	// IP-адрес источника
	unsigned int       dst_ip;	// IP-адрес назначения
};	// 20 bytes 

int main()
{
	int                sockfd;
	unsigned short int udp_size;
	char               datagram[DGRAM_SIZE];
	char               *data, *ptr;
	struct sockaddr_in addr;
	unsigned short int ihl, ip_size;

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("socket");
		exit(-1);
	}

	bzero(datagram, MSG_SIZE);

	data = datagram + sizeof(struct ipheader) + sizeof(struct udpheader);
	bzero(data, MSG_SIZE);
	strcpy(data, "Hello (ip udp)");

	ip_size  = sizeof(struct ipheader) + sizeof (struct udpheader) + strlen(data);
	udp_size = sizeof(struct udpheader) + strlen(data); 
	ihl      = sizeof(struct ipheader) / sizeof(unsigned int);

	struct ipheader *iph = (struct ipheader *) datagram;
	iph->version  = 4;
	iph->ihl      = ihl;
	iph->tos      = 0;
	iph->length   = htons(ip_size);
	iph->id       = 0;
	iph->offset   = 0;
	iph->ttl      = 128;
	iph->protocol = IPPROTO_UDP;
	iph->csum     = 0;
	inet_pton(AF_INET, SRC_ADDR, &(iph->src_ip));
	inet_pton(AF_INET, DST_ADDR, &(iph->dst_ip));

	struct udpheader *udph = (struct udpheader *) (datagram + sizeof(struct ipheader));
	udph->src_port = htons(CLT_PORT);   
	udph->dst_port = htons(SRV_PORT);  
	udph->length   = htons(udp_size); 
	udph->csum     = 0; 

	int one = 1;
	const int *val = &one;
	if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
		perror("setsockopt");
		exit(-1);
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;

	printf("send data to server: %s\n", data);
	if (sendto(sockfd, datagram, iph->length, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("sendto");
		exit(-1);
	}

	if (close(sockfd) < 0) {
		perror("sendto");
		exit(-1);
	}

	exit(0);
}

