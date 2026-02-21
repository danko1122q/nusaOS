/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <DNS.h>

#define ICMP_ECHO_REQUEST  8
#define ICMP_ECHO_REPLY    0
#define PING_PAYLOAD_SIZE  56
#define PING_INTERVAL_SEC  1

// nusaOS libc tidak punya inet_aton/inet_ntoa, implementasi sendiri
static const char* ping_inet_ntoa(struct in_addr addr) {
	static char buf[16];
	uint32_t ip = ntohl(addr.s_addr);
	snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
		(ip >> 24) & 0xFF,
		(ip >> 16) & 0xFF,
		(ip >>  8) & 0xFF,
		(ip      ) & 0xFF);
	return buf;
}

struct ICMPHeader {
	uint8_t  type;
	uint8_t  code;
	uint16_t checksum;
	uint16_t id;
	uint16_t sequence;
} __attribute__((packed));

struct PingPacket {
	ICMPHeader header;
	uint8_t    payload[PING_PAYLOAD_SIZE];
} __attribute__((packed));

static int     s_sockfd   = -1;
static int     s_sent     = 0;
static int     s_received = 0;
static char    s_host[256] = {0};

static uint16_t icmp_checksum(const void* data, size_t len) {
	uint32_t sum = 0;
	const uint8_t* b = (const uint8_t*) data;
	for (size_t i = 0; i + 1 < len; i += 2)
		sum += (uint16_t)(b[i] << 8 | b[i + 1]);
	if (len & 1)
		sum += (uint16_t)(b[len - 1] << 8);
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);
	return (uint16_t) ~sum;
}

static void print_stats(int sig) {
	printf("\n--- %s ping statistics ---\n", s_host);
	int lost = s_sent - s_received;
	printf("%d packets transmitted, %d received", s_sent, s_received);
	if (s_sent > 0)
		printf(", %d%% packet loss", (lost * 100) / s_sent);
	printf("\n");
	if (s_sockfd >= 0)
		close(s_sockfd);
	exit(0);
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: ping <host>\n");
		return 1;
	}

	strncpy(s_host, argv[1], sizeof(s_host) - 1);

	struct in_addr dest_addr;
	if (dns_resolve(s_host, &dest_addr) < 0) {
		fprintf(stderr, "ping: cannot resolve '%s'\n", s_host);
		return 1;
	}

	// SOCK_RAW dengan protocol 0 - sesuai kernel ICMPSocket
	s_sockfd = socket(AF_INET, SOCK_RAW, 1);
	if (s_sockfd < 0) {
		perror("ping: socket");
		return 1;
	}

	signal(SIGINT, print_stats);

	// Connect ke destination agar kernel setup route dan mark socket Connected
	{
		struct sockaddr_in dst;
		memset(&dst, 0, sizeof(dst));
		dst.sin_family = AF_INET;
		dst.sin_addr   = dest_addr;
		dst.sin_port   = 0;
		if (connect(s_sockfd, (struct sockaddr*) &dst, sizeof(dst)) < 0) {
			perror("ping: connect");
			return 1;
		}
	}

	printf("PING %s: %d data bytes\n", s_host, PING_PAYLOAD_SIZE);

	uint16_t pid = (uint16_t) getpid();
	uint16_t seq = 0;

	while (1) {
		PingPacket pkt;
		memset(&pkt, 0, sizeof(pkt));
		pkt.header.type     = ICMP_ECHO_REQUEST;
		pkt.header.code     = 0;
		pkt.header.id       = htons(pid);
		pkt.header.sequence = htons(seq);
		pkt.header.checksum = 0;
		for (int i = 0; i < PING_PAYLOAD_SIZE; i++)
			pkt.payload[i] = (uint8_t) i;
		pkt.header.checksum = icmp_checksum(&pkt, sizeof(pkt));

		struct timeval send_time;
		gettimeofday(&send_time, nullptr);

		struct sockaddr_in dest;
		memset(&dest, 0, sizeof(dest));
		dest.sin_family = AF_INET;
		dest.sin_addr   = dest_addr;

		ssize_t sent = sendto(s_sockfd, &pkt, sizeof(pkt), 0,
		                      (struct sockaddr*) &dest, sizeof(dest));
		if (sent < 0) {
			perror("ping: sendto");
			sleep(PING_INTERVAL_SEC);
			seq++; s_sent++;
			continue;
		}
		s_sent++;

		// Terima reply
		uint8_t recv_buf[1024];
		struct sockaddr_in src;
		socklen_t src_len = sizeof(src);

		ssize_t recv_len = recvfrom(s_sockfd, recv_buf, sizeof(recv_buf), 0,
		                            (struct sockaddr*) &src, &src_len);

		struct timeval recv_time;
		gettimeofday(&recv_time, nullptr);

		if (recv_len < 0) {
			printf("recvfrom error: %s (errno=%d)\n", strerror(errno), errno);
			seq++;
			sleep(PING_INTERVAL_SEC);
			continue;
		}

		if (recv_len == 0) {
			printf("recvfrom returned 0 bytes\n");
			seq++;
			sleep(PING_INTERVAL_SEC);
			continue;
		}

		long rtt_us = (recv_time.tv_sec  - send_time.tv_sec)  * 1000000L
		            + (recv_time.tv_usec - send_time.tv_usec);
		double rtt_ms = rtt_us / 1000.0;

		// Skip IPv4 header (20 byte) karena kernel kirim full packet
		if (recv_len < (ssize_t)(20 + (int)sizeof(ICMPHeader))) {
			seq++; sleep(PING_INTERVAL_SEC);
			continue;
		}

		ICMPHeader* reply = (ICMPHeader*)(recv_buf + 20);
		if (reply->type != ICMP_ECHO_REPLY || ntohs(reply->id) != pid) {
			printf("Filtered packet: type=%d id=%d (expected type=%d id=%d)\n",
			       reply->type, ntohs(reply->id), ICMP_ECHO_REPLY, pid);
			seq++; sleep(PING_INTERVAL_SEC);
			continue;
		}

		s_received++;
		printf("%zd bytes from %s: icmp_seq=%d ttl=64 time=%.3f ms\n",
		       recv_len - 20,
		       ping_inet_ntoa(src.sin_addr),
		       ntohs(reply->sequence),
		       rtt_ms);

		seq++;
		sleep(PING_INTERVAL_SEC);
	}

	return 0;
}