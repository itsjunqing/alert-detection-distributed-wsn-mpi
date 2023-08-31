#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h> 
#include <unistd.h>
#include <string.h>  


int getMACAddress(char* MACAddress);
int getIPAddress(char *IPAddress);


int getMACAddress(char* MACAddress) {
	struct ifreq ifr;
	struct ifconf ifc;
	char buf[1024];
	int success = 0;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	// if (sock == -1) { /* handle error*/ };

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	// if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { /* handle error */ }

	struct ifreq* it = ifc.ifc_req;
	const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it) {
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
						success = 1;
						break;
				}
			}
		}
	}

	unsigned char mac_address[6];

	if (success) memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
	sprintf(MACAddress, "%02X:%02X:%02X:%02X:%02X:%02X", mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
	return 1;
}


int getIPAddress(char *IPAddress) {
	/**
	 * A function that gets the IP address of the machine
	 */
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	char host[NI_MAXHOST];

	if (getifaddrs(&ifaddr) == -1) {
		perror("getifaddrs");
		exit(EXIT_FAILURE);
	}

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL)
			continue;
		s = getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

		int mac = strcmp(ifa->ifa_name,"en0");
		int ubuntu = strcmp(ifa->ifa_name, "enp0s3");
		int monarch = strcmp(ifa->ifa_name, "eth0");

		if((mac || ubuntu || monarch) && (ifa->ifa_addr->sa_family==AF_INET)) {
			if (s != 0) {
				perror("getnameinfo() failed\n");
				exit(EXIT_FAILURE);
			}
			sprintf(IPAddress, "%s", host);
		}
	}
	freeifaddrs(ifaddr); 

	return 1;
} 

