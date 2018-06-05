//============================================================================
// Name        : AirPortTalk.cpp
// Author      : Jon Dellaria
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <DLog.h>
//#include "../../DLiriusLib/Audio.h"
#include "RAOPClient.h"

#include "RTSPClient.h"

#include "MP3Stream.h"
#include "APTalk.h"
#include "MusicDB.h"
#include "configurationFile.h"

#ifdef USE_SOUND_CARD
#include <alsa/asoundlib.h>

int openSoundCard();
int closeSoundCard();

#define PCM_DEVICE "default"
snd_pcm_t *pcm_handle;
snd_pcm_uframes_t frames;
__u8 *buff;
unsigned int pcm;
int buff_size, loops;
int rate, channels, seconds;
unsigned int tmp, dir;
snd_pcm_hw_params_t *params;
#endif

int playStream();
int main_event_handler();
int eventHandler();
int PlaySong(string audioFileName, data_type_t adt);
int fd_event_callback();
int FDhandler();
int rd_fd_event_callback();

using namespace std;

int songFD = 0;
AudioStream auds;
RAOPClient raop;
DLog myLog;
int dataGramSocket;
int FDflags;

extern int playAutomatic;

#define SERVER_PORT 5000
#define DATA_GRAM_SERVER_PORT 1234

#define GET_BIGENDIAN_INT(x) (*(__u8*)(x)<<24)|(*((__u8*)(x)+1)<<16)|(*((__u8*)(x)+2)<<8)|(*((__u8*)(x)+3))

typedef enum playActions {
	PLAY_ACTION_STOP = 0,
	PLAY_ACTION_PAUSE,
	PLAY_ACTION_PLAY,
	PLAY_ACTION_QUIT,
	PLAY_ACTION_EXIT,
	PLAY_ACTION_VOLUME,
	PLAY_ACTION_NEXTALBUM,
	PLAY_ACTION_NEXTSONG,
	PLAY_ACTION_UPDATE,
	PLAY_ACTION_NORMAL,
	PLAY_ACTION_PLAY_TILL_END,
} playActions;

typedef enum finishSongActions {
	NETWORK_ACTION_CONNECT = 0,
	NETWORK_ACTION_DISCONNECT,
	NETWORK_ACTION_NORMAL,
	NETWORK_ACTION_WAIT,
} NetworkActions;

playActions playMode = PLAY_ACTION_PLAY;
playActions exitMode = PLAY_ACTION_PLAY;
NetworkActions networkMode = NETWORK_ACTION_DISCONNECT;

configurationFile myConfig;

int startDatagramServer(int portNumber, struct sockaddr_in *server);
int getDatagramMessage(int sock, char *buffer, struct sockaddr *from);
int sendDatagramMessage(int sock, char *buffer, struct sockaddr *to);
int closeDatagram(int sock);

int main(int argc, char* const argv[])
{
	int dataGramPort=DATA_GRAM_SERVER_PORT;
	struct sockaddr_in server;
	struct playQRecord pQR;

	int returnValue;
	string message;
	char ibuffer [33];


	pQR.id = 0; // start with no song to be played.
	if (argc == 2) // if there is an argument, then assume it is the configuration file
	{
		myConfig.getConfiguration(argv[1]);
	}
	else //otherwise assume there is a file in the default with the name "config.conf"
	{
		myConfig.getConfiguration("config.xml");
	}

	myLog.logFileName = myConfig.logFileName;
	myLog.printFile = myConfig.logPrintFile;
	myLog.printScreen = myConfig.logPrintScreen;
	myLog.printTime = myConfig.logPrintTime;

	if (myConfig.logValue.find("logDebug")!=string::npos)
	{
		myLog.logValue = logDebug;
		message = "myLog.logValue = logDebug";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logInformation")!=string::npos)
	{
		myLog.logValue = logInformation;
		message = "myLog.logValue = logInformation";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logWarning")!=string::npos)
	{
		myLog.logValue = logWarning;
		message = "myLog.logValue = logWarning";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logError")!=string::npos)
	{
		myLog.logValue = logError;
		message = "myLog.logValue = logError";
		myLog.print(logInformation, message);
	}
	dataGramSocket = startDatagramServer(dataGramPort,(struct sockaddr_in *) &server);
	while (exitMode != PLAY_ACTION_EXIT)
	{
		playMode = PLAY_ACTION_PLAY;
		networkMode = NETWORK_ACTION_DISCONNECT;
		message = "airportAddress is: ";
		message.append(myConfig.airportAddress + "\n");
		myLog.print(logWarning, message);

		OpenDBConnection();
		while (playMode != PLAY_ACTION_QUIT)
		{
			message = __func__;
			message.append(": Wile Loop - ");
			message.append("networkMode = ");
			sprintf(ibuffer, "%d", networkMode);
			message.append(ibuffer);
			sprintf(ibuffer, "%d", playMode);
			message.append("playMode = ");
			message.append(ibuffer);
			myLog.print(logDebug, message);

			if (networkMode == NETWORK_ACTION_DISCONNECT)
			{
				raop.Close();
				networkMode = NETWORK_ACTION_WAIT;
				message = __func__;
				message.append(": NETWORK_ACTION_DISCONNECT");
				myLog.print(logInformation, message);
				sleep(2);
			}
			if (networkMode == NETWORK_ACTION_CONNECT) //this should run first to set up raop and rstp connection
			{
				raop.Open();
				if (raop.Connect(myConfig.airportAddress.c_str(), SERVER_PORT) == 0)
				{
					networkMode = NETWORK_ACTION_NORMAL;
					message = __func__;
					message.append(": NETWORK_ACTION_CONNECT: SUCCEEDED!");
					myLog.print(logInformation, message);

					playMode = PLAY_ACTION_PLAY;
				}
				else
				{
					raop.Close();
					networkMode = NETWORK_ACTION_WAIT;
					message = __func__;
					message.append(": raop.Connect did not succeed");
					myLog.print(logWarning, message);
					playMode = PLAY_ACTION_PLAY;
					sleep(20);
				}
			}

			if (networkMode == NETWORK_ACTION_WAIT)
			{
				message = __func__;
				message.append(": NETWORK_ACTION_WAIT: networkMode = NETWORK_ACTION_WAIT");
				myLog.print(logDebug, message);
				if (isSongAvailableInPlayQ())
				{
					networkMode = NETWORK_ACTION_CONNECT;
					playMode = PLAY_ACTION_PLAY;
				}
				else
				{
					message = __func__;
					message.append(": NETWORK_ACTION_WAIT: No song to play");
					myLog.print(logInformation, message);
					playMode = PLAY_ACTION_PAUSE;
					sleep(5);
				}
			}
			if (networkMode == NETWORK_ACTION_NORMAL)
			{
				message = __func__;
				message.append(": NETWORK_ACTION_NORMAL");
				myLog.print(logDebug, message);
				while (playMode == PLAY_ACTION_PLAY)
				{
					pQR = getNextSongRecord();
					if (pQR.id != 0)
					{
						if (asciiToUtf8(pQR.location, 255) != 1)
						{
							message = __func__;
							message.append(": NETWORK_ACTION_NORMAL UTF Conversion error FileName - ");
							message.append(pQR.location);
							myLog.print(logError, message);
						}
						message = __func__;
						message.append(": PlaySong FileName - ");
						message.append(pQR.location);
						myLog.print(logWarning, message);
						songFD = PlaySong(pQR.location,AUD_TYPE_NONE);
					}
					else
					{
						playMode = PLAY_ACTION_STOP;
						networkMode = NETWORK_ACTION_DISCONNECT;
						message = __func__;
						message.append(": NETWORK_ACTION_NORMAL: No song to play");
						myLog.print(logInformation, message);
						sleep(5);
					}
				}
				returnValue = eventHandler();
				networkMode = NETWORK_ACTION_DISCONNECT;
			}
			returnValue = eventHandler();
		}
		message = __func__;
		message.append(": AirportTalk closing all connections and resting for 5 seconds.");
		myLog.print(logWarning, message);
		CloseDBConnection();
		raop.Close();
		sleep(5);
	}
	closeDatagram(dataGramSocket);
	message = __func__;
	message.append(": AirportTalk exiting Normally");
	myLog.print(logWarning, message);
}


#ifndef USE_SOUND_CARD
int PlaySong(string audioFileName, data_type_t adt)  // orig
{
	__u8 *buf;
	int size;
	int returnValue;
	string message;
	char ibuffer [33];
	string sbuffer;

	songFD = auds.Open(audioFileName,adt);
	message = __func__;
	message.append(": opening file:");
	message.append(audioFileName);
	myLog.print(logDebug, message);
	returnValue=0;
	playMode = PLAY_ACTION_PLAY;

	while(playMode == PLAY_ACTION_PLAY)
	{
		message = __func__;
		message.append(": playMode == PLAY_ACTION_PLAY");
		myLog.print(logDebug, message);
		if(raop.wblk_remsize == 0) // make sure that we have nothing in the buffer
		{
			returnValue = auds.GetNextSample(&buf, (int*)&size);
			if(returnValue != 0)
			{
				message = __func__;
				message.append(": auds.GetNextSample - != 0: ");
				sprintf(ibuffer, "%d", size);
				message.append(ibuffer);
				myLog.print(logDebug, message);
				playMode = PLAY_ACTION_NEXTSONG;

			}
			else
			{
				message = __func__;
				message.append(": auds.GetNextSample - ");
				message.append(" == 0");
				myLog.print(logDebug, message);
//				playMode = PLAY_ACTION_NEXTSONG;
//				DBGMSG("%s:GetNextSample size=%d\n",__func__,size);
			}
		}
		if (size > 0)// as long as we have a positive size send the information to the airport
		{
			message = __func__;
			message.append(": raop.SendSample - > 0: ");
			sprintf(ibuffer, "%d", size);
			message.append(ibuffer);
			myLog.print(logDebug, message);
			raop.SendSample(buf,size);
			FDhandler();
		}
		else // otherwise move to the next song
		{
			message = __func__;
			message.append(": size <= 0 - ");
			myLog.print(logDebug, message);
			playMode = PLAY_ACTION_NEXTSONG;

		}
		eventHandler();

	}
	playMode = PLAY_ACTION_PLAY; // Set playmode back to play... since we are out of the play loop
	message = __func__;
	message.append(": auds.Close");
	myLog.print(logWarning, message);
	auds.Close();

}
#else
int PlaySong(string audioFileName, data_type_t adt)
{
	__u8 *buf;
	int size;
	int returnValue;
	string message;

	printf("PlaySong: auds.Open %s\n",audioFileName.c_str());
	songFD = auds.Open(audioFileName,adt);
	message = __func__;
	message.append(": opening file:");
	message.append(audioFileName);
	myLog.print(logDebug, message);
	returnValue=0;
	playMode = PLAY_ACTION_PLAY;
	data_source_t ds;
	__u8 buffer[MAX_SAMPLES_IN_CHUNK*4+16];

	playMode = PLAY_ACTION_PLAY;


	while(playMode == PLAY_ACTION_PLAY)
	{
		message = __func__;
		message.append(": playMode == PLAY_ACTION_PLAY");
		myLog.print(logDebug, message);
		buff_size = frames * channels * 2;
		if(raop.wblk_remsize == 0) // make sure that we have nothing in the buffer
		{
//				DBGMSG("%s:GetNextSample size=%d\n",__func__,size);
		}

		size = frames * channels * 2;
		songFD = auds.GetNextSample(( __u8 *)buffer, &buff_size);

		if (pcm = snd_pcm_writei(pcm_handle, buffer, frames) == -EPIPE) {
			printf("XRUN.\n");
			snd_pcm_prepare(pcm_handle);
		} else if (pcm < 0) {
			printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
		}

		if (size > 0)// as long as we have a positive size send the information to the airport
		{
			 //auds.WritePCM(buffer, data, size, auds.chunk_size, &ds);
			ds.type = MEMORY;
			ds.u.fd = songFD;
//			auds.WritePCM(buf, &buff, &size, auds.chunk_size, &ds);
//			raop.SendSample(buf,size);
//			FDhandler();
		}
		else // otherwise move to the next song
		{
			playMode = PLAY_ACTION_NEXTSONG;
		}

//		returnValue = eventHandler();
	}
	auds.Close();
}
#endif


#ifdef USE_SOUND_CARD
/*
 * asound library must be included for these routines
 */
int main(int argc, char **argv) {
	struct sockaddr_in server;
	struct playQRecord pQR;
	NetworkActions networkMode = NETWORK_ACTION_DISCONNECT;
	int returnValue;
	string message;

	pQR.id = 0; // start with no song to be played.
	if (argc == 2) // if there is an argument, then assume it is the configuration file
	{
		myConfig.getConfiguration(argv[1]);
	}
	else //otherwise assume there is a file in the default with the name "config.conf"
	{
		myConfig.getConfiguration("config.xml");
	}

	myLog.logFileName = myConfig.logFileName;
	myLog.printFile = myConfig.logPrintFile;
	myLog.printScreen = myConfig.logPrintScreen;
	myLog.printTime = myConfig.logPrintTime;

	if (myConfig.logValue.find("logDebug")!=string::npos)
	{
		myLog.logValue = logDebug;
		message = "myLog.logValue = logDebug";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logInformation")!=string::npos)
	{
		myLog.logValue = logInformation;
		message = "myLog.logValue = logInformation";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logWarning")!=string::npos)
	{
		myLog.logValue = logWarning;
		message = "myLog.logValue = logWarning";
		myLog.print(logInformation, message);
	}
	if (myConfig.logValue.find("logError")!=string::npos)
	{
		myLog.logValue = logError;
		message = "myLog.logValue = logError";
		myLog.print(logInformation, message);
	}

	openSoundCard();

	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/02 Daydreaming.mp3",AUD_TYPE_MP3);
	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/03 Decks Dark.mp3",AUD_TYPE_MP3);
	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/04 Desert Island Disk.mp3",AUD_TYPE_MP3);
//	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/05 Ful Stop.mp3",AUD_TYPE_MP3);
//	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/06 Glass Eyes.mp3",AUD_TYPE_MP3);
//	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/07 Identikit.mp3",AUD_TYPE_MP3);
//	PlaySong("/home/jdellaria/Desktop/Music/Radiohead/A Moon Shaped Pool/08 The Numbers.mp3",AUD_TYPE_MP3);


	closeSoundCard();

	return 0;
}

int openSoundCard()
{

	rate 	 = 44100; //atoi(argv[1]);
	channels = 2; // atoi(argv[2]);
	seconds  = 2; //atoi(argv[3]);
	/* Open the PCM device in playback mode */
	if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE,
					SND_PCM_STREAM_PLAYBACK, 0) < 0)
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
					PCM_DEVICE, snd_strerror(pcm));

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
					SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
						SND_PCM_FORMAT_S16_LE) < 0)
		printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, (unsigned int*)&rate, 0) < 0)
		printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

	/* Write parameters */
	if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

	/* Resume information */
	printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

	printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

	snd_pcm_hw_params_get_channels(params, &tmp);
	printf("channels: %i ", tmp);

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	printf("rate: %d bps\n", tmp);

	printf("seconds: %d\n", seconds);

	/* Allocate buffer to hold single period */
	snd_pcm_hw_params_get_period_size(params, &frames, 0);
	printf("frames: %d\n", frames);
	buff_size = frames * channels * 2 /* 2 -> sample size */;
	printf("buff_size: %d\n", buff_size);
	buff = (__u8 *) malloc(buff_size);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

}

int closeSoundCard()
{
	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buff);
}
#endif

#define MAIN_EVENT_TIMEOUT 3 // sec unit

int FDhandler()
{
	fd_set rdfds,wrfds;
	int fdmax=0;
	struct timeval tout;
	tout.tv_sec = MAIN_EVENT_TIMEOUT;
	tout.tv_usec=0;

	FD_ZERO(&wrfds);
	FD_SET(raop.sfd, &wrfds);
	FD_SET(raop.sfd, &rdfds);
	fdmax=raop.sfd +1;

	select(fdmax+1,&rdfds,&wrfds,NULL,&tout);
	if( (FD_ISSET(raop.sfd,&rdfds)) && (FDflags&RAOP_FD_READ) )
	{
//		DBGMSG("main rd event flags=%d\n",FDflags);
		rd_fd_event_callback();
	}
	if( (FD_ISSET(raop.sfd,&wrfds)) && (FDflags&RAOP_FD_WRITE))
	{
//		DBGMSG("main wr event flags=%d\n",FDflags);
		fd_event_callback();
	}
	return 0;
}

int rd_fd_event_callback()
{
	int i;
	__u8 buf[256];
	int rsize;

	i=read(raop.sfd,buf,sizeof(buf));
	if(i>0)
	{
		rsize=GET_BIGENDIAN_INT(buf+0x2c);
		raop.size_in_aex=rsize;
		gettimeofday(&raop.last_read_tv,NULL);
//		DBGMSG("%s: read %d bytes, rsize=%d\n ***\n%s\n***\n", __func__, i,rsize,buf);
		if (raop.wblk_remsize > 0)
		{
			fd_event_callback();
		}
		return 0;
	}
	if(i<0) ERRMSG("%s: read error: %s\n", __func__, strerror(errno));
	if(i==0)
	{
		 INFMSG("%s: read, disconnected on the other end\n", __func__);
		playMode = PLAY_ACTION_QUIT;
	}
	return -1;
}

int fd_event_callback()
{
	int i;
	int rsize;

	if(!raop.wblk_remsize) {
		ERRMSG("%s: write is called with remsize=0\n", __func__);
		return -1;
	}
	i=write(raop.sfd,raop.data+raop.wblk_wsize,raop.wblk_remsize);
	if(i<0){
		ERRMSG("%s: write error: %s\n", __func__, strerror(errno));
		return -1;
	}
	if(i==0){
		INFMSG("%s: write, disconnected on the other end\n", __func__);
		return -1;
	}
	raop.wblk_wsize+=i;
	raop.wblk_remsize-=i;
	if(!raop.wblk_remsize)
	{
		FDflags = RAOP_FD_READ;
	}

//	DBGMSG("%d bytes are sent, remaining size=%d\n",i,raop.wblk_remsize);
	return 0;
}

int eventHandler()
{
	int n;
	int iVolume;
	char buffer[1024];
	struct sockaddr_in from;
	char *ps=NULL;
	int returnValue = PLAY_ACTION_NORMAL;
	string message;

//	playMode = PLAY_ACTION_PLAY;
	bzero(buffer,1024);
	n = getDatagramMessage(dataGramSocket, buffer, (struct sockaddr *)&from);
	if (n > 0)
	{
//		DBGMSG("Datagram message '%s': ",buffer );
		if(strstr(buffer,"quit") != NULL )
		{
			playMode = PLAY_ACTION_QUIT;
			message = __func__;
			message.append(": Play Quit Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"pause") != NULL )
		{
			playMode = PLAY_ACTION_PAUSE;
			message = __func__;
			message.append(": Play Quit Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playautomatic") != NULL )
		{
			playAutomatic = 1;
			networkMode = NETWORK_ACTION_CONNECT;
			playMode = PLAY_ACTION_PLAY;
			message = __func__;
			message.append(": Play Automatic Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"playmanual") != NULL )
		{
			playAutomatic = 0;
			message = __func__;
			message.append(": Play Manual Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"stop") != NULL )
		{
			playMode = PLAY_ACTION_STOP;
			message = __func__;
			message.append(": Play Stop Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"play") != NULL )
		{
			playMode = PLAY_ACTION_PLAY;
			networkMode = NETWORK_ACTION_CONNECT;
//			playMode = PLAY_ACTION_PLAY;
			message = __func__;
			message.append(": Play Play Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"exit") != NULL )
		{
			playMode = PLAY_ACTION_QUIT;
			exitMode = PLAY_ACTION_EXIT;
			playAutomatic = 0;
			message = __func__;
			message.append(": Exit Signal Received");
			myLog.print(logWarning, message);
		}

		else if( (ps = strstr(buffer,"volume")) != NULL )
		{
			iVolume = atoi(ps+7);
			raop.UpdateVolume(iVolume);
			message = __func__;
			message.append(": Volume Signal Received with a value of:");
			message.append(ps+7);
			myLog.print(logWarning, message);
//			playMode = PLAY_ACTION_VOLUME;
		}
		else if(strstr(buffer,"nextalbum") != NULL )
		{
			playMode = PLAY_ACTION_NEXTALBUM;
//			raop.SevenSecondSilent();
// xx			sleep(2);
// xx			raop.Flush();
			skipToNextAlbum();
			message = __func__;
			message.append(": Next Album Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"nextsong") != NULL ) // next is for going to the next song via the web site... without finishing the song.
		{
			playMode = PLAY_ACTION_NEXTSONG;
			returnValue = PLAY_ACTION_NEXTSONG;
//			raop.SevenSecondSilent();
//	xx		sleep(1);
//	xx		raop.Flush();
			message = __func__;
			message.append(": Next Song Signal Received");
			myLog.print(logWarning, message);
		}
		else if(strstr(buffer,"next") != NULL )// next is for going to the next song naturally.
		{
			playMode = PLAY_ACTION_NEXTSONG;
			returnValue = PLAY_ACTION_NEXTSONG;
//			raop.SevenSecondSilent();
// xx			sleep(2);
// xx			raop.Flush();
			message = __func__;
			message.append(": Next (Song) Signal Received");
			myLog.print(logWarning, message);
		}

		else if(strstr(buffer,"update") != NULL )
		{
//			playMode = PLAY_ACTION_UPDATE;
		}
	}
	usleep(1000); // let other processes have the CPU for 1000 microseconds

	return (returnValue);
}

void error(char *msg)
{
	perror(msg);
 //   exit(0);
}

int startDatagramServer(int portNumber, struct sockaddr_in *server)
{
	int sock;
	int length;

	sock=socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) error("Opening socket");
	length = sizeof(struct sockaddr_in);
	bzero(server,length);
	server->sin_family=AF_INET;
	server->sin_addr.s_addr=INADDR_ANY;
	server->sin_port=htons(portNumber);
	if (bind(sock,(struct sockaddr *)server,length)<0)
		error("binding");

	return (sock);
}

int getDatagramMessage(int sock, char *buffer, struct sockaddr *from)
{
	int n;
	socklen_t fromlen = sizeof(struct sockaddr_in);
	n = recvfrom(sock,buffer,1024,MSG_DONTWAIT,from,&fromlen);
	return(n);

}

int sendDatagramMessage(int sock, char *buffer, struct sockaddr *to)
{
	int n;
	int tolen = sizeof(struct sockaddr_in);

	sendto(sock,buffer,strlen(buffer),0,to,tolen);
	return(n);

}

int closeDatagram(int sock)
{
	close (sock);
//	printf("Server closing down...\n");
}
