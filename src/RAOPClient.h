/*
 * RAOPClient.h
 *
 *  Created on: Feb 14, 2010
 *      Author: jdellaria
 */

#ifndef RAOPCLIENT_H_
#define RAOPCLIENT_H_
#include <asm/types.h>

#include "RTSPClient.h"
#include "AudioStream.h"
#include "TCPSocket.h"

//aes.h
#ifndef uint8
#define uint8  unsigned char
#endif

#ifndef uint32
#define uint32 unsigned long int
#endif


#define	RAOP_FD_READ (1<<0)
#define RAOP_FD_WRITE (1<<1)

typedef struct
{
    uint32 erk[64];     /* encryption round keys */
    uint32 drk[64];     /* decryption round keys */
    int nr;             /* number of rounds */
}
aes_context;

//int  aes_set_key( aes_context *ctx, uint8 *key, int nbits );
//void aes_encrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );
//void aes_decrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );
//aes.h
class RAOPClient {
public:
	RAOPClient();
	virtual ~RAOPClient();
	int Connect(const char *host,__u16 destport);
	int StreamConnect();
	int Open();
	int Close();
	int SendSample(__u8 *sample, int count );
	int SmallSilent();
	int Flush();
	int SevenSecondSilent();
	int UpdateVolume(int vol);
	int Encrypt( __u8 *data, int size);
	int ReallocMemory(void **p, int newsize, const char *func);
	int pos(char c);
	int Base64Encode(const void *data, int size, char **str);
	int TokenDecode(const char *token);
	int Base64Decode(const char *str, unsigned char *data);
	int RsaEncrypt(__u8 *text, int len, __u8 *res);
	int RemoveCharFromString(char *str, char rc);
	void AesGenTables( void );
	int AesSetKey( aes_context *ctx, uint8 *key, int nbits );
	void AesEncrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );
	void AesDecrypt( aes_context *ctx, uint8 input[16], uint8 output[16] );

	RTSPClient rtsp;
	TCPSocket raopTcpConnection;
	__u8 iv[16]; // initialization vector for aes-cbc
	__u8 nv[16]; // next vector for aes-cbc
	__u8 key[16]; // key for aes-cbc
	string addr; // target host address
	__u16 rtsp_port;
	int ajstatus;
	int ajtype;
	int volume;
	int sfd; // stream socket fd
	int wblk_wsize;
	int wblk_remsize;
	int wait_songdone;
	aes_context ctx;
	__u8 *data;
	__u8 min_sdata[MINIMUM_SAMPLE_SIZE*4+16];
	int min_sdata_size;
	time_t paused_time;
	int size_in_aex;
	struct timeval last_read_tv;
};


#define JACK_STATUS_DISCONNECTED 0
#define JACK_STATUS_CONNECTED 1

#define JACK_TYPE_ANALOG 0
#define JACK_TYPE_DIGITAL 1

#define VOLUME_DEF -30
#define VOLUME_MIN -144
#define VOLUME_MAX 0

#endif /* RAOPCLIENT_H_ */
