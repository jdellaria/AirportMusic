/*
 * TCPSocket.cpp
 *
 *  Created on: Apr 6, 2010
 *      Author: jdellaria
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/poll.h>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "TCPSocket.h"
#include "APMusic.h"

#include <DLog.h>

extern DLog myLog;

TCPSocket::TCPSocket() {
	// TODO Auto-generated constructor stub

}

TCPSocket::~TCPSocket() {
	// TODO Auto-generated destructor stub
}


/*
 * open tcp port
 */
int TCPSocket::Open(char *hostname, unsigned short *port)
{
	/* socket creation */
	sd = socket(AF_INET, SOCK_STREAM, 0);
	if(sd<0) {
//		ERRMSG("cannot create tcp socket\n");
		return -1;
	}
	if(BindHost(hostname,0, port)) {
		close(sd);
		return -1;
	}

	return sd;
}

int TCPSocket::Close()
{
	close(sd);
}
/*
 * create tcp connection
 * as long as the socket is not non-blocking, this can block the process
 * nsport is network byte order
 */
int TCPSocket::Connect(struct sockaddr_in dest_addr)
{
	string message;
	char destAddrNumber[100];
	if(connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr))){
		SLEEP_MSEC(100L);
		// try one more time
		if(connect(sd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)))
		{
			message = "TCPSocket.cpp ";
			message.append(__func__);
			message.append(": addr=");
			message.append(inet_ntoa(dest_addr.sin_addr));
			message.append(", port = ");
			sprintf(destAddrNumber, "%d",ntohs(dest_addr.sin_port));
			message.append(destAddrNumber);
			myLog.print(logError, message);
			return -1;
		}
	}
	return 0;
}


int TCPSocket::ConnectByHost(const char *host, __u16 destport)
{
	struct sockaddr_in addr;
	struct hostent *h;
	string message;

	h = gethostbyname(host);
	if(h) {
		addr.sin_family = h->h_addrtype;
		memcpy((char *) &addr.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
	}else{
		addr.sin_family = AF_INET;
		if((addr.sin_addr.s_addr=inet_addr(host))==0xFFFFFFFF)
		{
			message = "TCPSocket.cpp ";
			message.append(__func__);
			message.append(": host: ");
			message.append(host);
			myLog.print(logError, message);
			return -1;
		}
	}
	addr.sin_port=htons(destport);

	return Connect(addr);
}

/* bind an opened socket to specified hostname and port.
 * if hostname=NULL, use INADDR_ANY.
 * if *port=0, use dynamically assigned port
 */
int TCPSocket::BindHost(char *hostname, unsigned long ulAddr,unsigned short *port)
{
	struct sockaddr_in my_addr;
	socklen_t nlen=sizeof(struct sockaddr);
	struct hostent *h;
	string message;

	memset(&my_addr, 0, sizeof(my_addr));
	/* use specified hostname */
	if(hostname){
		/* get server IP address (no check if input is IP address or DNS name */
		h = gethostbyname(hostname);
		if(h==NULL) {
			if(strstr(hostname, "255.255.255.255")==hostname){
				my_addr.sin_addr.s_addr=-1;
			}else{
				if((my_addr.sin_addr.s_addr=inet_addr(hostname))==0xFFFFFFFF)
				{
					message = "TCPSocket.cpp ";
					message.append(__func__);
					message.append(": host: ");
					message.append(hostname);
					myLog.print(logError, message);
					return -1;
				}
			}
			my_addr.sin_family = AF_INET;
		}else{
			my_addr.sin_family = h->h_addrtype;
			memcpy((char *) &my_addr.sin_addr.s_addr,
			       h->h_addr_list[0], h->h_length);
		}
	} else {
		// if hostname=NULL, use INADDR_ANY
		if(ulAddr)
			my_addr.sin_addr.s_addr = ulAddr;
		else
			my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_family = AF_INET;
	}

	/* bind a specified port */
	my_addr.sin_port = htons(*port);

	if(bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr))<0)
	{
		message = "TCPSocket.cpp ";
		message.append(__func__);
		message.append(": bind error: ");
		message.append(strerror(errno));
		myLog.print(logError, message);
		return -1;
	}

	if(*port==0){
		getsockname(sd, (struct sockaddr *) &my_addr, &nlen);
		*port=ntohs(my_addr.sin_port);
	}

	return 0;
}
