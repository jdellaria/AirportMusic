/*
 * AudioStream.cpp
 *
 *  Created on: Feb 17, 2010
 *      Author: jdellaria
 */

#include "AudioStream.h"
#include "APMusic.h"
#include "MP3Stream.h"
#include <DLog.h>

extern DLog myLog;

AudioStream::AudioStream() {
	stream = 0;
	sample_rate = DEFAULT_SAMPLE_RATE;
	data_type_t data_type;
	channels = 2;
}

AudioStream::~AudioStream() {
	// TODO Auto-generated destructor stub
}
//static data_type_t get_data_type(const char *fname);


int AudioStream::Open(string audioFileName, data_type_t adt)
{
	int rval=-1;
	int err;
	string message;

	channels=2; //default is stereo
	if(adt==AUD_TYPE_NONE)
		data_type=GetDataType(audioFileName.c_str());
	else
		data_type=adt;
	switch(data_type){

	case AUD_TYPE_URLMP3:
	case AUD_TYPE_MP3:
		rval = mp3Stream.Open(audioFileName);
		break;
	case AUD_TYPE_NONE:
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(" unknown audio data type");
		myLog.print(logError, message);
		break;
	}
	return rval;
 erexit:
	message = "AudioStream.cpp :";
	message.append(__func__);
	message.append(": error");
	myLog.print(logError, message);
	Close();
	return NULL;
}

int AudioStream::Close()
{
	string message;

	switch(data_type)
	{
		case AUD_TYPE_URLMP3:
		case AUD_TYPE_MP3:
			mp3Stream.Close();
			break;
		case AUD_TYPE_NONE:
			message = "AudioStream.cpp :";
			message.append(__func__);
			message.append(": ### shouldn't come here");
			myLog.print(logError, message);
			break;
	}

	return 0;
}

int AudioStream::GetNextSample( __u8 **data, int *size)
{
	int rval;
	string message;

	switch(data_type){
	case AUD_TYPE_URLMP3:
	case AUD_TYPE_MP3:
		rval=mp3Stream.GetNextSample( data, size);
		break;
	case AUD_TYPE_NONE:
		message = "AudioStream.cpp :";
		message.append(__func__);
		message.append(": ### shouldn't come here");
		myLog.print(logError, message);
		return -1;
	}
	return rval;
}

int AudioStream::WritePCM( __u8 *buffer, __u8 **data, int *size, int bsize, data_source_t *ds)
{
	__u8 one[4];
	int count=0;
	int bpos=0;
	__u8 *bp=buffer;
	int i,nodata=0;
	__s16 *resamp=NULL, *pr=NULL;

	bits_write(&bp,1,3,&bpos); // channel=1, stereo
	bits_write(&bp,0,4,&bpos); // unknown
	bits_write(&bp,0,8,&bpos); // unknown
	bits_write(&bp,0,4,&bpos); // unknown
	if(bsize!=4096)
		bits_write(&bp,1,1,&bpos); // hassize
	else
		bits_write(&bp,0,1,&bpos); // hassize
	bits_write(&bp,0,2,&bpos); // unused
	bits_write(&bp,1,1,&bpos); // is-not-compressed
	if(bsize!=4096){
		bits_write(&bp,(bsize>>24)&0xff,8,&bpos); // size of data, integer, big endian
		bits_write(&bp,(bsize>>16)&0xff,8,&bpos);
		bits_write(&bp,(bsize>>8)&0xff,8,&bpos);
		bits_write(&bp,bsize&0xff,8,&bpos);
	}
	while(1){
		if(pr){
			if(channels==1)
				*((__s16*)one)=*pr;
			else
				*((__s16*)one)=*pr++;
			*((__s16*)one+1)=*pr++;
		}else {
			switch(ds->type){
			case DESCRIPTOR:
				if(channels==1){
					if(read(ds->u.fd, one, 2)!=2) nodata=1;
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(read(ds->u.fd, one, 4)!=4) nodata=1;
				}
				break;
			case STREAM:
				if(channels==1){
					if(fread(one,1,2,ds->u.inf)!=2) nodata=1;
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(fread(one,1,4,ds->u.inf)!=4) nodata=1;
				}
				break;
			case MEMORY:
				if(channels==1){
					if(ds->u.mem.size<=count*2) nodata=1;
					*((__s16*)one)=ds->u.mem.data[count];
					*((__s16*)one+1)=*((__s16*)one);
				}else{
					if(ds->u.mem.size<=count*4) nodata=1;
					*((__s16*)one)=ds->u.mem.data[count*2];
					*((__s16*)one+1)=ds->u.mem.data[count*2+1];
				}
				break;
			}
		}
		if(nodata) break;

		bits_write(&bp,one[1],8,&bpos);
		bits_write(&bp,one[0],8,&bpos);
		bits_write(&bp,one[3],8,&bpos);
		bits_write(&bp,one[2],8,&bpos);
		if(++count==bsize) break;
	}
	if(!count){
		*size=bp-buffer;
		if(bpos) *size+=1;
		*data=buffer;
		return -1; // when no data at all, it should stop playing
	}
	/* when readable size is less than bsize, fill 0 at the bottom */
	for(i=0;i<(bsize-count)*4;i++){
		bits_write(&bp,0,8,&bpos);
	}
	*size=bp-buffer;
	if(bpos) *size+=1;
	*data=buffer;
	return 0;
}

int AudioStream::ClacChunkSize(int sample_rate)
{
	int bsize=MAX_SAMPLES_IN_CHUNK;
	int ratio=DEFAULT_SAMPLE_RATE*100/sample_rate;
	// to make suer the resampled size is <= 4096
	if(ratio>100) bsize=bsize*100/ratio-1;
	return bsize;
}

data_type_t AudioStream::GetDataType(const char *fname)
{
	int i;
	for(i=strlen(fname)-1;i>=0;i--)
		if(fname[i]=='.') break;
	if(i<0) return AUD_TYPE_PCM;
	if(i>=strlen(fname)-1) return AUD_TYPE_NONE;
	if(!strcasecmp(fname+i+1,"mp3")) return AUD_TYPE_MP3;
	if(strstr(fname,"http")==fname) return AUD_TYPE_URLMP3;
	return AUD_TYPE_NONE;
}
