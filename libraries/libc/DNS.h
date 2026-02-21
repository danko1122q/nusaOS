/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 */

#pragma once

#include <stdint.h>
#include <kernel/api/ipv4.h>

__DECL_BEGIN

// Resolve hostname ke IPv4 address
// Return: IP dalam network byte order, atau 0 jika gagal
// Contoh: dns_resolve("google.com", &addr)
int dns_resolve(const char* hostname, struct in_addr* result);

// Set DNS server (default: 8.8.8.8)
void dns_set_nameserver(uint32_t ip_network_order);

__DECL_END