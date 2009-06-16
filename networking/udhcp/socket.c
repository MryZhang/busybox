/* vi: set sw=4 ts=4: */
/*
 * socket.c -- DHCP server client/server socket creation
 *
 * udhcp client/server
 * Copyright (C) 1999 Matthew Ramsay <matthewr@moreton.com.au>
 *			Chris Trew <ctrew@moreton.com.au>
 *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <net/if.h>
#include <features.h>
#if (defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1) || defined _NEWLIB_VERSION
#include <netpacket/packet.h>
#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif

#include "common.h"


int FAST_FUNC udhcp_read_interface(const char *interface, int *ifindex, uint32_t *nip, uint8_t *mac)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *our_ip;

	memset(&ifr, 0, sizeof(ifr));
	fd = xsocket(AF_INET, SOCK_RAW, IPPROTO_RAW);

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy_IFNAMSIZ(ifr.ifr_name, interface);
	if (nip) {
		if (ioctl_or_perror(fd, SIOCGIFADDR, &ifr,
			"is interface %s up and configured?", interface)
		) {
			close(fd);
			return -1;
		}
		our_ip = (struct sockaddr_in *) &ifr.ifr_addr;
		*nip = our_ip->sin_addr.s_addr;
		DEBUG("ip of %s = %s", interface, inet_ntoa(our_ip->sin_addr));
	}

	if (ifindex) {
		if (ioctl_or_warn(fd, SIOCGIFINDEX, &ifr) != 0) {
			close(fd);
			return -1;
		}
		DEBUG("adapter index %d", ifr.ifr_ifindex);
		*ifindex = ifr.ifr_ifindex;
	}

	if (mac) {
		if (ioctl_or_warn(fd, SIOCGIFHWADDR, &ifr) != 0) {
			close(fd);
			return -1;
		}
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
		DEBUG("adapter hardware address %02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	close(fd);
	return 0;
}

/* 1. None of the callers expects it to ever fail */
/* 2. ip was always INADDR_ANY */
int FAST_FUNC udhcp_listen_socket(/*uint32_t ip,*/ int port, const char *inf)
{
	int fd;
	struct sockaddr_in addr;

	DEBUG("Opening listen socket on *:%d %s", port, inf);
	fd = xsocket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	setsockopt_reuseaddr(fd);
	if (setsockopt_broadcast(fd) == -1)
		bb_perror_msg_and_die("SO_BROADCAST");

	/* NB: bug 1032 says this doesn't work on ethernet aliases (ethN:M) */
	if (setsockopt_bindtodevice(fd, inf))
		xfunc_die(); /* warning is already printed */

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	/* addr.sin_addr.s_addr = ip; - all-zeros is INADDR_ANY */
	xbind(fd, (struct sockaddr *)&addr, sizeof(addr));

	return fd;
}
