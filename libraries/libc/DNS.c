/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2016-2024 Byteduck */

#include "DNS.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define DNS_PORT       53
#define DNS_MAX_PKT    512
#define DNS_TIMEOUT_S  5
#define DNS_TYPE_A     1
#define DNS_CLASS_IN   1

/* DNS packet header */
struct dns_header {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
} __attribute__((packed));

/* DNS answer fixed part */
struct dns_answer {
	uint16_t type;
	uint16_t cls;
	uint32_t ttl;
	uint16_t rdlength;
} __attribute__((packed));

/* DNS question fixed part */
struct dns_question {
	uint16_t qtype;
	uint16_t qclass;
} __attribute__((packed));

/* Default nameserver: 8.8.8.8 */
static uint32_t s_nameserver = 0x08080808; /* network byte order */
static uint16_t s_query_id   = 1;

void dns_set_nameserver(uint32_t ip_network_order) {
	s_nameserver = ip_network_order;
}

/* Encode "google.com" -> \x06google\x03com\x00 into buf
 * Returns bytes written, or -1 on error */
static int encode_name(const char* hostname, uint8_t* buf, int bufsize) {
	int total = 0;
	const char* p = hostname;
	while (*p) {
		const char* dot = p;
		while (*dot && *dot != '.') dot++;
		int label_len = (int)(dot - p);
		if (label_len == 0 || label_len > 63)
			return -1;
		if (total + 1 + label_len >= bufsize)
			return -1;
		buf[total++] = (uint8_t) label_len;
		memcpy(buf + total, p, label_len);
		total += label_len;
		p = dot;
		if (*p == '.') p++;
	}
	if (total + 1 >= bufsize)
		return -1;
	buf[total++] = 0; /* root label */
	return total;
}

/* Skip a DNS name (handles compression pointers)
 * Returns new offset after name */
static int skip_name(const uint8_t* buf, int len, int offset) {
	while (offset < len) {
		uint8_t label_len = buf[offset];
		if (label_len == 0)
			return offset + 1;
		if ((label_len & 0xC0) == 0xC0)
			return offset + 2; /* compression pointer, 2 bytes */
		offset += 1 + label_len;
	}
	return offset;
}

/* Build DNS query packet into out_buf
 * Returns packet size, or -1 on error */
static int build_query(const char* hostname, uint16_t id, uint8_t* out_buf, int bufsize) {
	if (bufsize < (int)(sizeof(struct dns_header) + 255 + sizeof(struct dns_question)))
		return -1;

	struct dns_header* hdr = (struct dns_header*) out_buf;
	hdr->id      = htons(id);
	hdr->flags   = htons(0x0100); /* RD = 1, recursion desired */
	hdr->qdcount = htons(1);
	hdr->ancount = 0;
	hdr->nscount = 0;
	hdr->arcount = 0;

	int offset = sizeof(struct dns_header);

	/* Encode hostname */
	int name_len = encode_name(hostname, out_buf + offset, bufsize - offset);
	if (name_len < 0)
		return -1;
	offset += name_len;

	/* Question section */
	if (offset + (int)sizeof(struct dns_question) > bufsize)
		return -1;
	struct dns_question* q = (struct dns_question*)(out_buf + offset);
	q->qtype  = htons(DNS_TYPE_A);
	q->qclass = htons(DNS_CLASS_IN);
	offset += sizeof(struct dns_question);

	return offset;
}

/* Parse DNS response, extract first A record
 * Returns 0 on success and fills result, -1 on error */
static int parse_response(const uint8_t* buf, int len, uint16_t expected_id, struct in_addr* result) {
	if (len < (int)sizeof(struct dns_header))
		return -1;

	const struct dns_header* hdr = (const struct dns_header*) buf;
	if (ntohs(hdr->id) != expected_id)
		return -1;

	uint16_t flags = ntohs(hdr->flags);
	if (!(flags & 0x8000))  /* QR bit must be 1 (response) */
		return -1;
	if ((flags & 0x000F) != 0) /* rcode must be 0 (no error) */
		return -1;

	uint16_t ancount = ntohs(hdr->ancount);
	if (ancount == 0)
		return -1;

	/* Skip question section */
	int offset = sizeof(struct dns_header);
	uint16_t qdcount = ntohs(hdr->qdcount);
	for (int i = 0; i < qdcount; i++) {
		offset = skip_name(buf, len, offset);
		offset += sizeof(struct dns_question);
		if (offset > len) return -1;
	}

	/* Parse answer section */
	for (int i = 0; i < ancount; i++) {
		if (offset >= len) break;

		offset = skip_name(buf, len, offset);
		if (offset + (int)sizeof(struct dns_answer) > len)
			return -1;

		const struct dns_answer* ans = (const struct dns_answer*)(buf + offset);
		offset += sizeof(struct dns_answer);

		uint16_t type     = ntohs(ans->type);
		uint16_t rdlength = ntohs(ans->rdlength);

		if (type == DNS_TYPE_A && rdlength == 4) {
			if (offset + 4 > len) return -1;
			memcpy(&result->s_addr, buf + offset, 4);
			return 0;
		}

		offset += rdlength;
	}

	return -1; /* no A record found */
}

int dns_resolve(const char* hostname, struct in_addr* result) {
	if (!hostname || !result)
		return -1;

	/* Cek apakah sudah berupa IP address (x.x.x.x) */
	uint32_t ip = 0;
	int octet = 0, dots = 0;
	int is_ip = 1;
	for (const char* p = hostname; *p; p++) {
		if (*p >= '0' && *p <= '9') {
			octet = octet * 10 + (*p - '0');
			if (octet > 255) { is_ip = 0; break; }
		} else if (*p == '.') {
			ip = (ip << 8) | octet;
			octet = 0; dots++;
		} else {
			is_ip = 0; break;
		}
	}
	if (is_ip && dots == 3) {
		ip = (ip << 8) | octet;
		result->s_addr = htonl(ip);
		return 0;
	}

	/* Buat UDP socket ke DNS server */
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		return -1;

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	server.sin_family      = AF_INET;
	server.sin_port        = htons(DNS_PORT);
	server.sin_addr.s_addr = s_nameserver;

	uint16_t query_id = s_query_id++;

	uint8_t query_buf[DNS_MAX_PKT];
	int query_len = build_query(hostname, query_id, query_buf, sizeof(query_buf));
	if (query_len < 0) {
		close(sock);
		return -1;
	}

	if (sendto(sock, query_buf, query_len, 0, (struct sockaddr*) &server, sizeof(server)) < 0) {
		close(sock);
		return -1;
	}

	uint8_t resp_buf[DNS_MAX_PKT];
	ssize_t resp_len = recvfrom(sock, resp_buf, sizeof(resp_buf), 0, NULL, NULL);
	close(sock);

	if (resp_len < 0)
		return -1;

	return parse_response(resp_buf, (int) resp_len, query_id, result);
}