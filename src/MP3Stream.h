/*
 * MP3Stream.h
 *
 *  Created on: Feb 24, 2010
 *      Author: jdellaria
 */

#ifndef MP3STREAM_H_
#define MP3STREAM_H_
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <asm/types.h>

#include "AudioStream.h"
#include "APMusic.h"

using namespace std;
//#define MUSICBUFFERLENGTH 2048
#define MP3PLAYER "mpg123"

class MP3Stream {
public:
	MP3Stream();
	virtual ~MP3Stream();
	int GetNextSample( __u8 **data, int *size);
	int Open(string audioFileName);
	int Close();

	int startPlay(string fileName);
	int ExecuteDecoder(char* const argv[], int *infd, int *outfd, int *errfd);
	int StopDecoder();
	int WaitForDecoderToExit();

	int dfd;
	int dpid;
	int fdin;
	__u8 buffer[MAX_SAMPLES_IN_CHUNK*4+16];
	string mp3FileName;

	int inputFileDescriptor;
	int outputFileDescriptor;
	int errorFileDescriptor;
	int audioFileDescriptorOutput;
	int programID;
	int readFlag;
	int reset;
	int speed;
	int channels;
	int fmt;
};


#endif /* MP3STREAM_H_ */
