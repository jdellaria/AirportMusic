/*
 * RTSPClient.h
 *
 *  Created on: Feb 11, 2010
 *      Author: jdellaria
 */

#ifndef RTSPCLIENT_H_
#define RTSPCLIENT_H_
#include <asm/types.h>
#include <iostream>
#include <vector>
#include <string>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "TCPSocket.h"
using namespace std;

typedef struct key_data_t {
	  vector<string> key;
	  vector<string> data;
}key_data_t;

class RTSPClient {
public:
	RTSPClient();
	virtual ~RTSPClient();
	void Open();
	int Close();
	int Disconnect();
	int SetUserAgent(const char *name);
	int AddExthds(string key, string data);
	int MarkDeleteExthds(string key);
	int RemoveAllExthds();
	int Connect(const char *host, __u16 destport, char *sid);
	char* LocalIP();
	__u16 GetServerPort();
	int AnnouceSDP(char *sdp);
	int Setup(key_data_t *kd);
	string kdLookup(key_data_t keyd, string key);
	int SetParameter(char *para);
	int Record();
	int Teardown();
	int Flush();
	int ExecRequest(const char *cmd, const char *content_type, char *content, int get_response, key_data_t hds, key_data_t *kds);

	TCPSocket rtspTcpConnection;
	int fd;
	char url[128];
	int cseq;
	key_data_t kd;
	key_data_t exthds;
	string session;
	string transport;
	__u16 server_port;
	struct in_addr host_addr;
	struct in_addr local_addr;
	const char *useragent;
};

#endif /* RTSPCLIENT_H_ */
