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
#define SRV_ADDR "127.0.0.1"

#define DGRAM_SIZE 512
#define MSG_SIZE   504

struct udpheader {
	unsigned short int src_port;	// порт источника
	unsigned short int dst_port;	// порт назначения
	unsigned short int length;	// длина заголовка UDP + данные
	unsigned short int csum;	// контрольная сумма UDP (можно писать ноль)
};	// 8 bytes

int main()
{
	int                sockfd;
	struct udpheader   udph;
	unsigned short int udp_size;
	char               datagram[DGRAM_SIZE];
	char               message[MSG_SIZE], *ptr;
	struct sockaddr_in addr;

	bzero(message, MSG_SIZE);
	strcpy(message, "Hello (udp)");

	udp_size = strlen(message) + sizeof(udph);    // общая длинна пакета

	udph.src_port = htons(CLT_PORT);       
	udph.dst_port = htons(SRV_PORT);   
	udph.length   = htons(udp_size); 
	udph.csum     = 0; 
 
	bzero(datagram, DGRAM_SIZE);
	ptr = datagram;

	memcpy(ptr, &udph, sizeof(udph));
	ptr += sizeof(udph);
	memcpy(ptr, message, strlen(message));

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
	if (sockfd == -1) {
		perror("socket");
		exit(-1);
	}

	bzero(&addr, sizeof(addr));

	addr.sin_family = AF_INET;
	inet_pton(AF_INET, SRV_ADDR, &(addr.sin_addr));
	addr.sin_port = htons(SRV_PORT);

	printf("send message to server: %s\n", message);
	if (sendto(sockfd, datagram, udp_size, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("sendto");
		exit(-1);
	}

	if (close(sockfd) < 0){
		perror("sendto");
		exit(-1);
	}

	exit(0);
}
