/****************************************************************************
 *
 * Copyright 2025 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include <netdb.h>
#include <pthread.h>
#include "csifw_ping_generator.h"
#include <lwip/prot/icmp.h>
#include "netutils/netlib.h"
#ifdef __LINUX__
#include <sim/ailite_sim_compat.h>
#endif

#define ERR_OK OK
#define PING_TH_NAME "csifw_ping_th"

struct icmp_echo_hdr *p_iecho_;       /* Echo Header */
struct sockaddr *socketAddr_;         /* Sokcet Address */
pthread_t csi_ping_thread_;           /* CSI Ping Thread */
unsigned int ping_count_;         /* Ping Count */
int ping_socket_;                      /* Ping Socket */
struct addrinfo *addr_info_;          /* Address Information */
bool ping_thread_stop_;                /* Ping Thread Stop Status */
int ping_Interval_;



void *pingThreadFun(void *vargp);
static int setIcmp4Config(struct addrinfo *hints);
static int ping_generator_close_socket(int *ping_socket, struct addrinfo **addr_info_list);
static int ping_generator_open_socket(int *ping_socket, struct sockaddr **socketAddr, struct addrinfo **addr_info_list);


static u16_t standard_chksum(void *dataptr, u16_t len)
{
	if (!dataptr) {
		printf("Invalid data pointer for checksum");
		return 0;
	}
	if (len == 0) {
		printf("Invalid length 0 for checksum");
		return 0;
	}

	u32_t acc = 0;
	u16_t src;
	u8_t *octetptr = (u8_t *) dataptr;
	/* dataptr may be at odd or even addresses */
	while (len > 1) {
		/* declare first octet as most significant
		   thus assume network order, ignoring host order */
		src = (*octetptr) << 8;
		octetptr++;
		/* declare second octet as least significant */
		src |= (*octetptr);
		octetptr++;
		acc += src;
		len -= 2;
	}

	if (len > 0) {
		/* accumulate remaining octet */
		src = (*octetptr) << 8;
		acc += src;
	}
	/* add deferred carry bits */
	acc = (acc >> 16) + (acc & 0x0000ffffUL);
	if ((acc & 0xffff0000UL) != 0) {
		acc = (acc >> 16) + (acc & 0x0000ffffUL);
	}
	/* This maybe a little confusing: reorder sum using htons()
	   instead of ntohs() since it has a little less call overhead.
	   The caller must invert bits for Internet sum ! */
	u16_t checksum = htons((u16_t)acc);
	printf("Calculated checksum: 0x%04X", checksum);
	return checksum;
}

static void ping_prepare_echo(struct icmp_echo_hdr *p_iecho, u16_t len)
{
	if (!p_iecho) {
		printf("Invalid ICMP echo header pointer");
		return;
	}
	if (len < sizeof(struct icmp_echo_hdr)) {
		printf("Invalid length %u (min %zu required)", 
			len, sizeof(struct icmp_echo_hdr));
		return;
	}

	size_t i = 0;
	int icmp_hdrlen = sizeof(struct icmp_echo_hdr);
	ICMPH_CODE_SET(p_iecho, 0);
	p_iecho->id = 0xAFAF;		//PING_ID;
	p_iecho->seqno = htons(1);

	printf("Preparing ICMP echo packet (id=0x%X, seq=%d, len=%u)", ntohs(p_iecho->id), ntohs(p_iecho->seqno), len);
	for (i = icmp_hdrlen; i < len; i++) {
		((char *)p_iecho)[i] = (char)i;
	}

	ICMPH_TYPE_SET(p_iecho, ICMP_ECHO);
	p_iecho->chksum = 0;
	p_iecho->chksum = ~standard_chksum(p_iecho, len);
}

static int ping_send(int ping_socket, struct icmp_echo_hdr *p_iecho, struct sockaddr *socketAddr)
{
	ping_count_++;
	if (!(ping_count_ % 1000)) {
		printf("Ping #%d - Sending ICMP echo (socket=%d, seq=%d, id=%d)",
			ping_count_, ping_socket, 
			ntohs(p_iecho->seqno), ntohs(p_iecho->id));
	}

	int ret;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	size_t icmplen = sizeof(struct icmp_echo_hdr) + 8;

	ret = sendto(ping_socket, p_iecho, icmplen, 0, socketAddr, addrlen);
	if (ret <= 0) {
		printf("sendto() failed - ret=%d, errno=%d (%s)", ret, errno, strerror(errno));
		return 0;
	}
	return 1;
}

void *pingThreadFun(void *vargp)
{

	int ping_socket = ping_socket_;
	struct icmp_echo_hdr *p_iecho = p_iecho_;
	struct sockaddr *socketAddr = socketAddr_;
	while (!ping_thread_stop_) {
		usleep(ping_Interval_);
		if (ping_send(ping_socket, p_iecho, socketAddr) != 1) {
			printf("Ping send failed :%d", 0);
		}
	}                                                    
	printf("[THREAD] : EXIT");
	return NULL;
}

int ping_generator_start(void)
{
	if (ping_Interval_ == 0) {
		printf("Ping interval is %d ==> NO PING START", ping_Interval_);
		return 1;
	}
	ping_count_ = 0;
	csi_ping_thread_ = 0;
	ping_thread_stop_ = false;
	if (pthread_create(&csi_ping_thread_, NULL, pingThreadFun, NULL) != 0) {
		printf("Failed to create Ping thread - errno=%d (%s)", errno, strerror(errno));
		return 0;
	}
	if (pthread_setname_np(csi_ping_thread_, PING_TH_NAME) != 0) {
		printf("Failed to set thread name '%s' - errno=%d (%s)", PING_TH_NAME, errno, strerror(errno));
	}
	printf("[PING Thread] created and initialized successfully");
	return 1;
}

int csi_ping_generator_initialize(int collection_interval_ms)
{
	ping_Interval_ = collection_interval_ms * 1000;
	printf("Starting ping thread (ID: %lu)", pthread_self());
	int ping_socket = -1;
	struct addrinfo *addr_info_list = NULL;
	struct sockaddr *socketAddr = NULL;

	printf("Opening ping socket");
	if (ping_generator_open_socket(&ping_socket, &socketAddr, &addr_info_list) != 1) {
		printf("Failed to open ping socket :%d", 0);
		return 0;
	}
	printf("Ping socket opened successfully (fd=%d)", ping_socket);

	size_t icmplen = sizeof(struct icmp_echo_hdr) + 8;
	struct icmp_echo_hdr *p_iecho = NULL;

	printf("Allocating ICMP echo buffer (%zu bytes) for socket %d", icmplen, ping_socket);
	p_iecho = (struct icmp_echo_hdr *)malloc(icmplen);
	if (!p_iecho) {
		printf("Failed to allocate %zu bytes for ICMP echo (errno=%d: %s)", 
			icmplen, errno, strerror(errno));
		ping_generator_close_socket(&ping_socket, &addr_info_list);
		return 0;
	}
	printf("ICMP echo buffer allocated successfully at %p (%zu bytes)", p_iecho, icmplen);
	ping_prepare_echo(p_iecho, (u16_t) icmplen);

	addr_info_ = addr_info_list;
	p_iecho_ = p_iecho;
	socketAddr_ = socketAddr;
	ping_socket_ = ping_socket;
    
	return 1;
}

int csi_ping_generator_cleanup(void)
{
	if (p_iecho_) {
		free(p_iecho_);
	}
	if (ping_generator_close_socket(&ping_socket_, &addr_info_) != 1) {
		printf("Failed to close ping socket :%d", 0);
		return 0;
	}
	return 1;
}

int ping_generator_stop(void)
{
	if (ping_Interval_ == 0) {
		printf("Ping interval is 0 ==> PING STOP NOT REQUIRED", ping_Interval_);
		return 1;
	}
	ping_thread_stop_ = true;
	pthread_join(csi_ping_thread_, NULL);
	printf("[PING Thread] stopped");
	return 1;
}

static int setIcmp4Config(struct addrinfo *hints)
{
	memset(hints, 0, sizeof(struct addrinfo));
	hints->ai_family = AF_INET;
	hints->ai_socktype = SOCK_RAW;
	hints->ai_protocol = IPPROTO_ICMP;
	return 1;
}

static int ping_generator_open_socket(int *ping_socket, struct sockaddr **socketAddr, struct addrinfo **addr_info_list)
{
	const char *tAddr;
	char ipv4_buf[16];
	uint8_t ipv4_address[4];
	if (netlib_get_ipv4_gateway_addr("wlan0", (struct in_addr *)ipv4_address) == 0) {
		snprintf(ipv4_buf, 16, "%u.%u.%u.%u", ipv4_address[0], ipv4_address[1], ipv4_address[2], ipv4_address[3]);
	}

	struct timeval tv;
	struct addrinfo hints;
	struct addrinfo *rp = NULL;

	if (setIcmp4Config(&hints) != 1) {
		printf("Failed to set ICMP4 config: invalid IP address");
		return 0;
	}
	tAddr = ipv4_buf;
	printf("PING %s (%s)bytes of data.", tAddr, tAddr);

	if (getaddrinfo(tAddr, NULL, &hints, addr_info_list) != 0) {
		printf("Failed to get address info for %s - errno=%d (%s)", tAddr, errno, strerror(errno));
		return 0;
	}
	for (rp = *addr_info_list; rp != NULL; rp = rp->ai_next) {
		if ((*ping_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0) {
			continue;
		} else {
			printf("Socket created successfully (fd=%d, family=%d, type=%d, proto=%d)",
						*ping_socket, rp->ai_family, rp->ai_socktype, rp->ai_protocol);
			break;
		}
	}
	printf("Using socket %d for ping operations", *ping_socket);
	if (rp == NULL) {
		printf("Failed to create raw socket for %s - errno=%d (%s)",
					tAddr, errno, strerror(errno));
		return 0;
	}

	*socketAddr = rp->ai_addr;
	if (!(*socketAddr)) {
		printf("Invalid socket address for %s",socketAddr);
		return 0;
	}
	if ((*socketAddr)->sa_family != AF_INET) {
		printf("Invalid socket family %d (expected AF_INET)",
				(*socketAddr)->sa_family);
		return 0;
	}

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	if (setsockopt(*ping_socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) != ERR_OK) {
		printf("setsockopt failed for socket %d - errno=%d (%s)",
				*ping_socket, errno, strerror(errno));
		return 0;
	}
	printf("Socket options set successfully (fd=%d, timeout=%d sec)", 
			*ping_socket, tv.tv_sec);
	return 1;
}

static int ping_generator_close_socket(int *ping_socket, struct addrinfo **addr_info_list)
{
	int res = 1;
	if (*addr_info_list) {
		freeaddrinfo(*addr_info_list);
		printf("Freed address info resources");
	}

	if (*ping_socket >= 0) {
		if (close(*ping_socket) < 0) {
			printf("Failed to close socket %d - errno=%d (%s)",
					*ping_socket, errno, strerror(errno));
			res = 0;
		}
		printf("Ping generator socket %d closed successfully", *ping_socket);
		*ping_socket = -1;
	}
	return res;
}

