/*
 * TCPSocket.h
 *
 *  Created on: Apr 6, 2010
 *      Author: jdellaria
 */

#ifndef TCPSOCKET_H_
#define TCPSOCKET_H_

#include <asm/types.h>

class TCPSocket {
public:
	TCPSocket();
	virtual ~TCPSocket();

	int Open(char *hostname, unsigned short *port);
	int Close();
	int Connect(struct sockaddr_in dest_addr);
	int ConnectByHost(const char *host, __u16 destport);
	int BindHost(char *hostname, unsigned long ulAddr,unsigned short *port);

	int sd;
};

#endif /* TCPSOCKET_H_ */
