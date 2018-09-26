/*
 * RTSPClient.cpp
 *
 *  Created on: Feb 11, 2010
 *      Author: jdellaria
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/poll.h>
#include "TCPSocket.h"
#include "RTSPClient.h"
#include "APMusic.h"
#include <DLog.h>

extern DLog myLog;

RTSPClient::RTSPClient() {
	fd = 0;
	url[0] = NULL;
	cseq = 0;
	session.clear();
	transport.clear();
	server_port = 0;
	useragent = NULL;
}

RTSPClient::~RTSPClient() {
	// TODO Auto-generated destructor stub
}

void RTSPClient::Open()
{
	useragent="RTSPClient";
	return;
}

int RTSPClient::Close()
{

	Disconnect();
	RemoveAllExthds();
	close(fd);//close RTSP stream to AirPort
	return 0;
}

int RTSPClient::Disconnect()
{
	close(fd);
	return 0;
}

int RTSPClient::SetUserAgent(const char *name)
{
	useragent=name;
	return 0;
}

int RTSPClient::AddExthds(string key, string data)
{
	exthds.key.push_back(key);
	exthds.data.push_back(data);
	return 0;
}

int RTSPClient::MarkDeleteExthds(string key)
{
	vector<string>::iterator i;
	int keyindex = 0;
	int dataindex = 0;
	for(i = exthds.key.begin(); i < exthds.key.end(); i++)
	{
		if ((*i).compare(key) == 0)
		{
			exthds.key.erase(i);
			//Get corresponding data
			for(i = exthds.data.begin(); i < exthds.data.end(); i++)
			{
				if (keyindex == dataindex)
				{
					exthds.data.erase(i);
				}
				dataindex++;
			}
			return 0;
		}
		keyindex++;
	}
	return -1;
}

int RTSPClient::RemoveAllExthds()
{
	exthds.key.clear();
	exthds.data.clear();
	return 0;
}

int RTSPClient::Connect(const char *host, __u16 destport, char *sid)
{
	__u16 myport=0;
	struct sockaddr_in name;
	socklen_t namelen=sizeof(name);

	if((fd=rtspTcpConnection.Open(NULL, &myport))==-1) return -1;
	if(rtspTcpConnection.ConnectByHost(host, destport)) return -1;
	getsockname(fd, (struct sockaddr*)&name, &namelen);
	memcpy(&local_addr,&name.sin_addr,sizeof(struct in_addr));
	sprintf(url,"rtsp://%s/%s",inet_ntoa(name.sin_addr),sid);
	getpeername(fd, (struct sockaddr*)&name, &namelen);
	memcpy(&host_addr,&name.sin_addr,sizeof(struct in_addr));
	return 0;
}

char* RTSPClient::LocalIP()
{
	return inet_ntoa(local_addr);
}

__u16 RTSPClient::GetServerPort()
{
	return server_port;
}

int RTSPClient::AnnouceSDP(char *sdp)
{
	key_data_t nullKey;

	return (ExecRequest("ANNOUNCE", "application/sdp", sdp, 1, nullKey, &kd));
}

/*
 * key_data type data look up
 */
string RTSPClient::kdLookup(key_data_t keyd, string key)
{
	int index, retVal;
	for(index = 0; index < keyd.key.size(); index++)
	{
		retVal = keyd.key[index].compare(key.c_str());
		if(!retVal)
		{
			return keyd.data[index];
		}
	}
	return "";
}

int RTSPClient::Setup(key_data_t *kd)
{
	key_data_t rkd;
	key_data_t hds;
	const char delimiters[] = ";";
	char *bufChar=NULL;
	string buf;
	char *token,*pc;
	int rval=-1;
	string message;

	hds.key.push_back("Transport");
	hds.data.push_back("RTP/AVP/TCP;unicast;interleaved=0-1;mode=record");
	if(ExecRequest("SETUP", NULL, NULL, 1, hds, &rkd)) return -1;
	session=kdLookup(rkd, "Session");
	if(session.size() == 0)
	{
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": no session in response");
		myLog.print(logError, message);
		goto erexit;
	}
	transport=kdLookup(rkd, "Transport");
	if(transport.size() == 0)
	{
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": no transport in response");
		myLog.print(logError, message);
		goto erexit;
	}
	buf.append(transport);
	bufChar = (char*)malloc(buf.size()+1);
	if(bufChar == NULL)
	{
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": error allocating memory");
		myLog.print(logError, message);
		goto erexit;
	}
	strcpy(bufChar,buf.c_str());
	token=strtok(bufChar,delimiters);
	server_port=0;
	while(token)
	{
		if((pc=strstr(token,"="))){
			*pc=0;
			if(!strcmp(token,"server_port")){
				server_port=atoi(pc+1);
//				DBGMSG("RTSPClient::%s: server_port response:%d\n",__func__,server_port);
				break;
			}
//			DBGMSG("RTSPClient::%s: token:%s = %s server_port=%d\n",__func__,token, pc+1,server_port);

		}
		token=strtok(NULL,delimiters);
	}
	if(server_port==0){
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": no server_port in response");
		myLog.print(logError, message);
		goto erexit;
	}

	rval=0;
 erexit:
	if(bufChar) free(bufChar);
	*kd=rkd;
	return rval;
}

int RTSPClient::SetParameter(char *para)
{
	key_data_t hds;
	return ExecRequest("SET_PARAMETER", "text/parameters", para, 1, hds, &kd);
}

int RTSPClient::Record()
{
	key_data_t hds;
	string message;

	if(session.size() == 0){
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": no session in progress");
		myLog.print(logError, message);
		return -1;
	}
	hds.key.push_back("Range");
	hds.data.push_back("npt=0-");
    hds.key.push_back("RTP-Info");
	hds.data.push_back("seq=0;rtptime=0");
	return ExecRequest("RECORD",NULL,NULL,1,hds,&kd);
}

int RTSPClient::Flush()
{
	key_data_t hds;

	hds.key.push_back("RTP-Info");
	hds.data.push_back("seq=0;rtptime=0");
	return ExecRequest("FLUSH", NULL, NULL, 1, hds, &kd);
}

int RTSPClient::Teardown()
{
	key_data_t hds;
	return ExecRequest( "TEARDOWN", NULL, NULL, 0, hds, &kd);
}
/*
 * send RTSP request, and get responce if it's needed
 * if this gets a success, *kd is allocated or reallocated (if *kd is not NULL)
 */
int RTSPClient::ExecRequest(const char *cmd, const char *content_type,
		char *content, int get_response, key_data_t hds, key_data_t *kds)
{
	char line[1024];
	char req[1024];
	char reql[128];
	const char delimiters[] = " ";
	char *token,*dp;
	int i,j,dsize,rval;
	int timeout=5000; // msec unit
	string message;

    sprintf(req, "%s %s RTSP/1.0\r\nCSeq: %d\r\n",cmd,url,++cseq );
    if( session.size() != 0)
    {
		sprintf(reql,"Session: %s\r\n", session.c_str() );
		strncat(req,reql,sizeof(req));
	}

	i=0;
	for(i = 0; i < hds.key.size(); i++)
	{
		sprintf(reql,"%s: %s\r\n", hds.key[i].c_str(), hds.data[i].c_str());
		strncat(req,reql,sizeof(req));
	}
	if( content_type && content)
	{
		sprintf(reql, "Content-Type: %s\r\nContent-Length: %d\r\n",
			content_type, (int)strlen(content));
		strncat(req,reql,sizeof(req));
	}
	sprintf(reql, "User-Agent: %s\r\n", useragent );
	strncat(req,reql,sizeof(req));

	i=0;
	for(i = 0; i < exthds.key.size(); i++)
	{
		sprintf(reql,"%s: %s\r\n", exthds.key[i].c_str(), exthds.data[i].c_str());
		strncat(req,reql,sizeof(req));
//		DBGMSG("%s i = %d\n",__func__, i);
	}
	strncat(req,"\r\n",sizeof(req));

	if( content_type && content)
	{
		strncat(req,content,sizeof(req));
	}

	rval=write(fd,req,strlen(req));
//	DBGMSG("%s: write %d: %d: data:***begin\n%s\n***end\n",__func__, strlen(req),rval,req);
	if( !get_response )
	{
		return 0;
	}

	if(read_line(fd,line,sizeof(line),timeout,0)<=0)
	{
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": request failed");
		myLog.print(logError, message);
		return -1;
	}
	token = strtok(line, delimiters);
	token = strtok(NULL, delimiters);
	if(token==NULL || strcmp(token,"200"))
	{
		message = "RTSPClient.cpp ";
		message.append(__func__);
		message.append(": ");
		message.append(cmd);
		message.append(" request failed, error ");
		message.append(token);
		myLog.print(logError, message);
		return -1;
	}

	i=0;
	while(read_line(fd,line,sizeof(line),timeout,0)>0)
	{
		timeout=1000; // once it started, it shouldn't take a long time

		if(i && line[0]==' ')
		{
			for(j=0;j<strlen(line);j++) if(line[j]!=' ') break;
			dsize+=strlen(line+j);
			message = "RTSPClient.cpp ";
			message.append(__func__);
			message.append(": ");
			message.append(cmd);
			message.append(" We need to look at this!!! line: ");
			message.append(line+j);
			myLog.print(logError, message);
			continue;
		}
		dp=strstr(line,":");
		if(!dp)
		{
			message = "RTSPClient.cpp ";
			message.append(__func__);
			message.append(": ");
			message.append(cmd);
			message.append(" Request failed, bad header");
			myLog.print(logError, message);
			kds->data.clear();
			kds->key.clear();
			return -1;
		}
		*dp=0;
		kds->key.push_back(line);
//		DBGMSG("%s: kds.key > %s\n",__func__, line);
		dsize=strlen(dp+1)+1;
		kds->data.push_back(dp+1);
		i++;
	}
	return 0;
}

/*
 * read one line from the file descriptor
 * timeout: msec unit, -1 for infinite
 * if CR comes then following LF is expected
 * returned string in line is always null terminated, maxlen-1 is maximum string length
 */
int read_line(int fd, char *line, int maxlen, int timeout, int no_poll)
{
	int i,rval;
	int count=0;
	struct pollfd pfds; //={events:POLLIN};
	char ch;
	string message;

	*line=0;
	pfds.events = POLLIN;
	pfds.fd=fd;
	for(i=0;i<maxlen;i++){
		if(no_poll || poll(&pfds, 1, timeout))
			rval=read(fd,&ch,1);
		else return 0;

		if(rval==-1){
			if(errno==EAGAIN) return 0;
			message = "RTSPClient.cpp ";
			message.append(__func__);
			message.append(": read error: ");
			message.append(strerror(errno));
			myLog.print(logError, message);
			return -1;
		}
		if(rval==0){
			message = "RTSPClient.cpp ";
			message.append(__func__);
			message.append(": disconnected on the other end");
			myLog.print(logInformation, message);
			return -1;
		}
		if(ch=='\n'){
			*line=0;
			return count;
		}
		if(ch=='\r') continue;
		*line++=ch;
		count++;
		if(count>=maxlen-1) break;
	}
	*line=0;
	return count;
}
