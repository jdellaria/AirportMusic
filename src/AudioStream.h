/*
 * AudioStream.h
 *
 *  Created on: Feb 17, 2010
 *      Author: jdellaria
 */

#ifndef AUDIOSTREAM_H_
#define AUDIOSTREAM_H_
#include <asm/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
//#include <samplerate.h>
#include <signal.h>

#include "MP3Stream.h"

using namespace std;
typedef enum data_type_t {
	AUD_TYPE_NONE = 0,
	AUD_TYPE_ALAC,
	AUD_TYPE_WAV,
	AUD_TYPE_MP3,
	AUD_TYPE_OGG,
	AUD_TYPE_AAC,
	AUD_TYPE_URLMP3,
	AUD_TYPE_PLS,
	AUD_TYPE_PCM,
	AUD_TYPE_FLAC,
} data_type_t;

typedef enum data_source_type_t {
	DESCRIPTOR=0,
	STREAM,
	MEMORY,
}data_source_type_t;

typedef struct mem_source_t {
	int size;
	__s16 *data;
}mem_source_t;
typedef struct data_source_t {
	data_source_type_t type;
	union{
		int fd;
		FILE *inf;
		mem_source_t mem;
	}u;
}data_source_t;


#define DEFAULT_SAMPLE_RATE 44100
#define MAX_SAMPLES_IN_CHUNK 4096

/*
 * if I send very small chunk of data, AEX is going to disconnect.
 * To avoid it, apply this size as the minimum size of chunk.
 */
#define MINIMUM_SAMPLE_SIZE 32

class AudioStream {
public:
	AudioStream();
	virtual ~AudioStream();

	int WritePCM( __u8 *buffer, __u8 **data, int *size, int bsize, data_source_t *ds);
	int Open(string audioFileName, data_type_t adt);
	int Close();
	int GetNextSample( __u8 **data, int *size);
	int ClacChunkSize(int sample_rate);
	data_type_t GetDataType(const char *fname);

	void *stream;
	int sample_rate;
	void (*sigchld_cb)(void *, siginfo_t *);
	int chunk_size;
	data_type_t data_type;
	int channels;
//	SRC_STATE *resamp_st;
//	SRC_DATA resamp_sd;
	__u16 *resamp_buf;
	MP3Stream mp3Stream;
};

/* write bits filed data, *bpos=0 for msb, *bpos=7 for lsb
   d=data, blen=length of bits field
 */
static inline void bits_write(__u8 **p, __u8 d, int blen, int *bpos)
{
	int lb,rb,bd;
	lb=7-*bpos;
	rb=lb-blen+1;
	if(rb>=0){
		bd=d<<rb;
		if(*bpos)
			**p|=bd;
		else
			**p=bd;
		*bpos+=blen;
	}else{
		bd=d>>-rb;
		**p|=bd;
		*p+=1;
		**p=d<<(8+rb);
		*bpos=-rb;
	}
}

#endif /* AUDIOSTREAM_H_ */
