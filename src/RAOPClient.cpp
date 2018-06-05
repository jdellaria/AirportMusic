/*
 * RAOPClient.cpp
 *
 *  Created on: Feb 14, 2010
 *      Author: jdellaria
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <DLog.h>


#include "RTSPClient.h"
#include "RAOPClient.h"
#include "AudioStream.h"
#include "APTalk.h"


extern DLog myLog;

using namespace std;

static char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
#define SERVER_PORT 5000

#define DEFAULT_SAMPLE_RATE 44100
#define MAX_SAMPLES_IN_CHUNK 4096

extern AudioStream auds;
extern int FDflags;
/*
 * if I send very small chunk of data, AEX is going to disconnect.
 * To avoid it, apply this size as the minimum size of chunk.
 */
#define MINIMUM_SAMPLE_SIZE 32

RAOPClient::RAOPClient() {
	// TODO Auto-generated constructor stub
//	memset(min_sdata,0,sizeof(min_sdata));
}

RAOPClient::~RAOPClient() {
	free(data);
}

int RAOPClient::Open()
{
	__s16 sdata[MINIMUM_SAMPLE_SIZE*2];
	data_source_t ds;
	ds.type = MEMORY;
	__u8 *bp;
	string message;

	RAND_seed(sdata,sizeof(sdata));
	if(!RAND_bytes(iv, sizeof(iv)) || !RAND_bytes(key, sizeof(key)))
	{
//		ERRMSG("%s:RAND_bytes error code=%ld\n",__func__,ERR_get_error());
//		sprintf(message, "RAOPClient.cpp %s: RAND_bytes error code=%ld", __func__,ERR_get_error());
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": RAND_bytes error code");
		myLog.print(logError, message);
		return 0;
	}
	memcpy(nv,iv,sizeof(nv));
//	volume=VOLUME_DEF;
	volume=VOLUME_MAX;
	AesSetKey(&ctx, key, 128);
	// prepare a small silent data to send during pause period.
	ds.u.mem.size=MINIMUM_SAMPLE_SIZE*4;
	ds.u.mem.data=sdata;
	memset(sdata,0,sizeof(sdata));
	auds.WritePCM(min_sdata, &bp, &min_sdata_size, MINIMUM_SAMPLE_SIZE, &ds);
	return 1;
}

int RAOPClient::Flush()
{
	rtsp.Flush();
	return 0;
}

int RAOPClient::Close()
{
	rtsp.Teardown();
	rtsp.Close();
	close(sfd); //close RAOP stream to AirPort
	return 0;
}

int RAOPClient::Connect( const char *host,__u16 destport)
{
	__u8 buf[4+8+16];
	char sid[16];
	char sci[24];
	char *sac=NULL,*myKey=NULL,*myIv=NULL;
	char sdp[1024];
	int rval=-1;
	key_data_t setup_kd;
	char *ajChar, *token, *pc;
	string aj;
	const char delimiters[] = ";";
	__u8 rsakey[512];
	int i;
	string message;

	ajChar = NULL;
	RAND_bytes(buf, sizeof(buf));
	sprintf(sid, "%d", *((__u32*)buf));
	sprintf(sci, "%08x%08x",*((__u32*)(buf+4)),*((__u32*)(buf+8)));
	Base64Encode(buf+12,16,&sac);
	rtsp.Open();
	rtsp.SetUserAgent("iTunes/4.6 (Macintosh; U; PPC Mac OS X 10.3)");
	rtsp.AddExthds("Client-Instance", sci);
	rtsp.Connect(host, destport, sid);
	i=RsaEncrypt(key,16,rsakey);
	Base64Encode(rsakey,i,&myKey);
	RemoveCharFromString(myKey,'=');
	Base64Encode(iv,16,&myIv);
	RemoveCharFromString(myIv,'=');
	sprintf(sdp,
            "v=0\r\n"
            "o=iTunes %s 0 IN IP4 %s\r\n"
            "s=iTunes\r\n"
            "c=IN IP4 %s\r\n"
            "t=0 0\r\n"
            "m=audio 0 RTP/AVP 96\r\n"
            "a=rtpmap:96 AppleLossless\r\n"
            "a=fmtp:96 4096 0 16 40 10 14 2 255 0 0 44100\r\n"
            "a=rsaaeskey:%s\r\n"
            "a=aesiv:%s\r\n",
            sid, rtsp.LocalIP(), host, myKey, myIv);
	RemoveCharFromString(sac,'=');
	rtsp.AddExthds("Apple-Challenge", sac);
	if (rtsp.AnnouceSDP(sdp) != 0)
	{
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": rtsp.AnnouceSDP did not succeed");
		myLog.print(logError, message);
		goto erexit;
	}
	if (rtsp.MarkDeleteExthds("Apple-Challenge") != 0)
	{
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": rtsp.MarkDeleteExthds did not succeed");
		myLog.print(logError, message);
		goto erexit;
	}
	if (rtsp.Setup(&setup_kd) != 0)
	{
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": rtsp.Setup did not succeed");
		myLog.print(logError, message);
		goto erexit;
	}
	aj=rtsp.kdLookup(setup_kd,"Audio-Jack-Status");
	if((aj.size()==0))
	{
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": Audio-Jack-Status is missing");
		myLog.print(logError, message);
		goto erexit;
	}
	ajChar = (char*)malloc(aj.size()+1);
	if(ajChar == NULL)
	{
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": error allocating memory");
		myLog.print(logError, message);
		goto erexit;
	}
	strcpy(ajChar,aj.c_str());
	token=strtok(ajChar,delimiters);
	while(token){
		if((pc=strstr(token,"="))){
			*pc=0;
			if(!strcmp(token,"type") && !strcmp(pc+1,"digital")){
				ajtype=JACK_TYPE_DIGITAL;
			}
		}else{
			if(!strcmp(token,"connected")){
				ajstatus=JACK_STATUS_CONNECTED;
			}
		}
		token=strtok(NULL,delimiters);
	}
	if(rtsp.Record()) goto erexit;

	addr.clear();
	addr.append(host);
	rtsp_port=destport;

	if(StreamConnect()) goto erexit;

	rval=0;
 erexit:
	if(ajChar != NULL)
	{
		free(ajChar);
	}
	return rval;
}

int RAOPClient::StreamConnect()
{
	__u16 myport=0;

	if((sfd=raopTcpConnection.Open(NULL, &myport))==-1) return -1;
	if(raopTcpConnection.ConnectByHost(addr.c_str(), rtsp.GetServerPort()))
	{
//		close(sfd);
		raopTcpConnection.Close();
		sfd=-1;
		return -1;
	}
	return 0;
}


int RAOPClient::SendSample(__u8 *sample, int count )
{
	int rval=-1;
	int writeReturn;
	__u16 len;
        __u8 header[] = {
		0x24, 0x00, 0x00, 0x00,
		0xF0, 0xFF, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
        };
	const int header_size=sizeof(header);

	if(ReallocMemory((void**)&data, count+header_size+16, __func__))
	{
		DBGMSG("RAOPClient::SendSample error Reallocating memory!\n");
		goto erexit;
	}
	memcpy(data,header,header_size);
	len=count+header_size-4;
	data[2]=len>>8;
	data[3]=len&0xff;
	memcpy(data+header_size,sample,count);
	Encrypt(data+header_size, count);
	len=count+header_size;
	wblk_remsize=count+header_size;
	wblk_wsize=0;

	FDflags = RAOP_FD_READ|RAOP_FD_WRITE;
//	writeReturn = write(sfd,data+wblk_wsize,wblk_remsize);
//	DBGMSG("RAOPClient::SendSample writeReturn:%d\n",writeReturn);
	rval=0;
 erexit:

	return rval;
}
#ifdef JON
int set_fd_event(int fd, int flags, fd_callback_t cbf, void *p)
{
	int i;
	// check the same fd first. if it exists, update it
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd==fd){
			raopld->fds[i].dp=p;
			raopld->fds[i].cbf=cbf;
			raopld->fds[i].flags=flags;
			return 0;
		}
	}
	// then create a new one
	for(i=0;i<MAX_NUM_OF_FDS;i++){
		if(raopld->fds[i].fd<0){
			raopld->fds[i].fd=fd;
			raopld->fds[i].dp=p;
			raopld->fds[i].cbf=cbf;
			raopld->fds[i].flags=flags;
			return 0;
		}
	}
	return -1;
}
#endif
/*
 * update volume
 * minimum=0, maximum=100
 */
int RAOPClient::UpdateVolume(int vol)
{

	char a[128];

	volume=VOLUME_MIN+(VOLUME_MAX-VOLUME_MIN)*vol/100;
	sprintf(a, "volume: %d.000000\r\n", volume);
	return rtsp.SetParameter(a);
}
int RAOPClient::SmallSilent()
{
	__u8 buffer[MAX_SAMPLES_IN_CHUNK*4+16]; //This is the same as the buffer size in MP3Stream
	memset (buffer, 0 , MAX_SAMPLES_IN_CHUNK*4+16);
	SendSample(buffer,MAX_SAMPLES_IN_CHUNK*4+16);
	//DBGMSG("sent a small silent data\n");
	return 0;
}


int RAOPClient::SevenSecondSilent()
{
	int i;
	for (i=0;i <= 100; i++)
	{
		SmallSilent();
	}
	return 0;
}


int RAOPClient::Encrypt( __u8 *data, int size)
{
	__u8 *buf;

	int i=0,j;
	memcpy(nv,iv,16);
	while(i+16<=size){
		buf=data+i;
		for(j=0;j<16;j++) buf[j] ^= nv[j];
		AesEncrypt(&ctx, buf, buf);
		memcpy(nv,buf,16);
		i+=16;
	}
	if(i<size){
#if 0
		INFMSG("%s: a block less than 16 bytes(%d) is not encrypted\n",__func__, size-i);
		memset(tmp,0,16);
		memcpy(tmp,data+i,size-i);
		for(j=0;j<16;j++) tmp[j] ^= nv[j];
		aes_encrypt(ctx, tmp, tmp);
		memcpy(nv,tmp,16);
		memcpy(data+i,tmp,16);
		i+=16;
#endif
	}
	return i;
}
/*
 * if newsize < 4096, align the size to power of 2
 */
int RAOPClient::ReallocMemory(void **p, int newsize, const char *func)
{
	void *np;
	int i=0;
	int n=16;
	string message;

	for(i=0;i<8;i++){
		if(newsize<=n){
			newsize=n;
			break;
		}
		n=n<<1;
	}
//	newsize=newsize;
	np=realloc(*p,newsize);
	if(!np){
//		ERRMSG("%s: realloc failed: %s\n",func,strerror(errno));
		message = "RAOPClient.cpp ";
		message.append(__func__);
		message.append(": realloc failed: ");
		myLog.print(logError, message);
		message.append(strerror(errno));
		return -1;
	}
	*p=np;
	return 0;
}

int RAOPClient::pos(char c)
{
    char *p;
    for (p = base64_chars; *p; p++)
	if (*p == c)
	    return p - base64_chars;
    return -1;
}


int RAOPClient::Base64Encode(const void *data, int size, char **str)
{
    char *s, *p;
    int i;
    int c;
    const unsigned char *q;

    p = s = (char *) malloc(size * 4 / 3 + 4);
    if (p == NULL)
	return -1;
    q = (const unsigned char *) data;
    i = 0;
    for (i = 0; i < size;) {
	c = q[i++];
	c *= 256;
	if (i < size)
	    c += q[i];
	i++;
	c *= 256;
	if (i < size)
	    c += q[i];
	i++;
	p[0] = base64_chars[(c & 0x00fc0000) >> 18];
	p[1] = base64_chars[(c & 0x0003f000) >> 12];
	p[2] = base64_chars[(c & 0x00000fc0) >> 6];
	p[3] = base64_chars[(c & 0x0000003f) >> 0];
	if (i > size)
	    p[3] = '=';
	if (i > size + 1)
	    p[2] = '=';
	p += 4;
    }
    *p = 0;
    *str = s;
    return strlen(s);
}


#define DECODE_ERROR 0xffffffff

int RAOPClient::TokenDecode(const char *token)
{
    int i;
    unsigned int val = 0;
    int marker = 0;
    if (strlen(token) < 4)
	return DECODE_ERROR;
    for (i = 0; i < 4; i++) {
	val *= 64;
	if (token[i] == '=')
	    marker++;
	else if (marker > 0)
	    return DECODE_ERROR;
	else
	    val += pos(token[i]);
    }
    if (marker > 2)
	return DECODE_ERROR;
    return (marker << 24) | val;
}

int RAOPClient::Base64Decode(const char *str, unsigned char *data)
{
    const char *p;
    unsigned char *q;

    q = data;
    for (p = str; *p && (*p == '=' || strchr(base64_chars, *p)); p += 4) {
	unsigned int val = TokenDecode(p);
	unsigned int marker = (val >> 24) & 0xff;
	if (val == DECODE_ERROR)
	    return -1;
	*q++ = (val >> 16) & 0xff;
	if (marker < 2)
	    *q++ = (val >> 8) & 0xff;
	if (marker < 1)
	    *q++ = val & 0xff;
    }
    return q - (unsigned char *) data;
}

int RAOPClient::RsaEncrypt(__u8 *text, int len, __u8 *res)
{
	RSA *rsa;
	__u8 modules[256];
	__u8 exponent[8];
	int size;

        char n[] =
            "59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUtwC"
            "5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDR"
            "KSKv6kDqnw4UwPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuB"
            "OitnZ/bDzPHrTOZz0Dew0uowxf/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJ"
            "Q+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/UAaHqn9JdsBWLUEpVviYnh"
            "imNVvYFZeCXg/IdTQ+x4IRdiXNv5hEew==";
        char e[] = "AQAB";

	rsa=RSA_new();
	size=Base64Decode(n,modules);
	rsa->n=BN_bin2bn(modules,size,NULL);
	size=Base64Decode(e,exponent);
	rsa->e=BN_bin2bn(exponent,size,NULL);
	size=RSA_public_encrypt(len, text, res, rsa, RSA_PKCS1_OAEP_PADDING);
	RSA_free(rsa);
	return size;
}

/*
 * remove one character from a string
 * return the number of deleted characters
 */
int RAOPClient::RemoveCharFromString(char *str, char rc)
{
	int i=0,j=0,len;
	int num=0;
	len=strlen(str);
	while(i<len){
		if(str[i]==rc){
			for(j=i;j<len;j++) str[j]=str[j+1];
			len--;
			num++;
		}else{
			i++;
		}
	}
	return num;
}


// aes.c


/* uncomment the following line to run the test suite */

/* #define TEST */

/* forward S-box & tables */

uint32 FSb[256];
uint32 FT0[256];
uint32 FT1[256];
uint32 FT2[256];
uint32 FT3[256];

/* reverse S-box & tables */

uint32 RSb[256];
uint32 RT0[256];
uint32 RT1[256];
uint32 RT2[256];
uint32 RT3[256];

/* round constants */

uint32 RCON[10];

/* tables generation flag */

int do_init = 1;

/* tables generation routine */

#define ROTR8(x) ( ( ( x << 24 ) & 0xFFFFFFFF ) | \
                   ( ( x & 0xFFFFFFFF ) >>  8 ) )

#define XTIME(x) ( ( x <<  1 ) ^ ( ( x & 0x80 ) ? 0x1B : 0x00 ) )
#define MUL(x,y) ( ( x &&  y ) ? aes_gen_tables_pow[(aes_gen_tables_log[x] + aes_gen_tables_log[y]) % 255] : 0 )

void RAOPClient::AesGenTables( void )
{
    int i;
    uint8 x, y;
    uint8 aes_gen_tables_pow[256];
    uint8 aes_gen_tables_log[256];

    /* compute pow and log tables over GF(2^8) */

    for( i = 0, x = 1; i < 256; i++, x ^= XTIME( x ) )
    {
        aes_gen_tables_pow[i] = x;
        aes_gen_tables_log[x] = i;
    }

    /* calculate the round constants */

    for( i = 0, x = 1; i < 10; i++, x = XTIME( x ) )
    {
        RCON[i] = (uint32) x << 24;
    }

    /* generate the forward and reverse S-boxes */

    FSb[0x00] = 0x63;
    RSb[0x63] = 0x00;

    for( i = 1; i < 256; i++ )
    {
        x = aes_gen_tables_pow[255 - aes_gen_tables_log[i]];

        y = x;  y = ( y << 1 ) | ( y >> 7 );
        x ^= y; y = ( y << 1 ) | ( y >> 7 );
        x ^= y; y = ( y << 1 ) | ( y >> 7 );
        x ^= y; y = ( y << 1 ) | ( y >> 7 );
        x ^= y ^ 0x63;

        FSb[i] = x;
        RSb[x] = i;
    }

    /* generate the forward and reverse tables */

    for( i = 0; i < 256; i++ )
    {
        x = FSb[i]; y = XTIME( x );

        FT0[i] =   (uint32) ( x ^ y ) ^
                 ( (uint32) x <<  8 ) ^
                 ( (uint32) x << 16 ) ^
                 ( (uint32) y << 24 );

        FT0[i] &= 0xFFFFFFFF;

        FT1[i] = ROTR8( FT0[i] );
        FT2[i] = ROTR8( FT1[i] );
        FT3[i] = ROTR8( FT2[i] );

        y = RSb[i];

        RT0[i] = ( (uint32) MUL( 0x0B, y )       ) ^
                 ( (uint32) MUL( 0x0D, y ) <<  8 ) ^
                 ( (uint32) MUL( 0x09, y ) << 16 ) ^
                 ( (uint32) MUL( 0x0E, y ) << 24 );

        RT0[i] &= 0xFFFFFFFF;

        RT1[i] = ROTR8( RT0[i] );
        RT2[i] = ROTR8( RT1[i] );
        RT3[i] = ROTR8( RT2[i] );
    }
}


/* platform-independant 32-bit integer manipulation macros */

#define GET_UINT32(n,b,i)                       \
{                                               \
    (n) = ( (uint32) (b)[(i)    ] << 24 )       \
        | ( (uint32) (b)[(i) + 1] << 16 )       \
        | ( (uint32) (b)[(i) + 2] <<  8 )       \
        | ( (uint32) (b)[(i) + 3]       );      \
}

#define PUT_UINT32(n,b,i)                       \
{                                               \
    (b)[(i)    ] = (uint8) ( (n) >> 24 );       \
    (b)[(i) + 1] = (uint8) ( (n) >> 16 );       \
    (b)[(i) + 2] = (uint8) ( (n) >>  8 );       \
    (b)[(i) + 3] = (uint8) ( (n)       );       \
}

/* decryption key schedule tables */

int KT_init = 1;

uint32 KT0[256];
uint32 KT1[256];
uint32 KT2[256];
uint32 KT3[256];

/* AES key scheduling routine */

int RAOPClient::AesSetKey( aes_context *ctx, uint8 *key, int nbits )
{
    int i;
    uint32 *RK, *SK;

    if( do_init )
    {
        AesGenTables();

        do_init = 0;
    }

    switch( nbits )
    {
        case 128: ctx->nr = 10; break;
        case 192: ctx->nr = 12; break;
        case 256: ctx->nr = 14; break;
        default : return( 1 );
    }

    RK = ctx->erk;

    for( i = 0; i < (nbits >> 5); i++ )
    {
        GET_UINT32( RK[i], key, i * 4 );
    }

    /* setup encryption round keys */

    switch( nbits )
    {
    case 128:

        for( i = 0; i < 10; i++, RK += 4 )
        {
            RK[4]  = RK[0] ^ RCON[i] ^
                        ( FSb[ (uint8) ( RK[3] >> 16 ) ] << 24 ) ^
                        ( FSb[ (uint8) ( RK[3] >>  8 ) ] << 16 ) ^
                        ( FSb[ (uint8) ( RK[3]       ) ] <<  8 ) ^
                        ( FSb[ (uint8) ( RK[3] >> 24 ) ]       );

            RK[5]  = RK[1] ^ RK[4];
            RK[6]  = RK[2] ^ RK[5];
            RK[7]  = RK[3] ^ RK[6];
        }
        break;

    case 192:

        for( i = 0; i < 8; i++, RK += 6 )
        {
            RK[6]  = RK[0] ^ RCON[i] ^
                        ( FSb[ (uint8) ( RK[5] >> 16 ) ] << 24 ) ^
                        ( FSb[ (uint8) ( RK[5] >>  8 ) ] << 16 ) ^
                        ( FSb[ (uint8) ( RK[5]       ) ] <<  8 ) ^
                        ( FSb[ (uint8) ( RK[5] >> 24 ) ]       );

            RK[7]  = RK[1] ^ RK[6];
            RK[8]  = RK[2] ^ RK[7];
            RK[9]  = RK[3] ^ RK[8];
            RK[10] = RK[4] ^ RK[9];
            RK[11] = RK[5] ^ RK[10];
        }
        break;

    case 256:

        for( i = 0; i < 7; i++, RK += 8 )
        {
            RK[8]  = RK[0] ^ RCON[i] ^
                        ( FSb[ (uint8) ( RK[7] >> 16 ) ] << 24 ) ^
                        ( FSb[ (uint8) ( RK[7] >>  8 ) ] << 16 ) ^
                        ( FSb[ (uint8) ( RK[7]       ) ] <<  8 ) ^
                        ( FSb[ (uint8) ( RK[7] >> 24 ) ]       );

            RK[9]  = RK[1] ^ RK[8];
            RK[10] = RK[2] ^ RK[9];
            RK[11] = RK[3] ^ RK[10];

            RK[12] = RK[4] ^
                        ( FSb[ (uint8) ( RK[11] >> 24 ) ] << 24 ) ^
                        ( FSb[ (uint8) ( RK[11] >> 16 ) ] << 16 ) ^
                        ( FSb[ (uint8) ( RK[11] >>  8 ) ] <<  8 ) ^
                        ( FSb[ (uint8) ( RK[11]       ) ]       );

            RK[13] = RK[5] ^ RK[12];
            RK[14] = RK[6] ^ RK[13];
            RK[15] = RK[7] ^ RK[14];
        }
        break;
    }

    /* setup decryption round keys */

    if( KT_init )
    {
        for( i = 0; i < 256; i++ )
        {
            KT0[i] = RT0[ FSb[i] ];
            KT1[i] = RT1[ FSb[i] ];
            KT2[i] = RT2[ FSb[i] ];
            KT3[i] = RT3[ FSb[i] ];
        }

        KT_init = 0;
    }

    SK = ctx->drk;

    *SK++ = *RK++;
    *SK++ = *RK++;
    *SK++ = *RK++;
    *SK++ = *RK++;

    for( i = 1; i < ctx->nr; i++ )
    {
        RK -= 8;

        *SK++ = KT0[ (uint8) ( *RK >> 24 ) ] ^
                KT1[ (uint8) ( *RK >> 16 ) ] ^
                KT2[ (uint8) ( *RK >>  8 ) ] ^
                KT3[ (uint8) ( *RK       ) ]; RK++;

        *SK++ = KT0[ (uint8) ( *RK >> 24 ) ] ^
                KT1[ (uint8) ( *RK >> 16 ) ] ^
                KT2[ (uint8) ( *RK >>  8 ) ] ^
                KT3[ (uint8) ( *RK       ) ]; RK++;

        *SK++ = KT0[ (uint8) ( *RK >> 24 ) ] ^
                KT1[ (uint8) ( *RK >> 16 ) ] ^
                KT2[ (uint8) ( *RK >>  8 ) ] ^
                KT3[ (uint8) ( *RK       ) ]; RK++;

        *SK++ = KT0[ (uint8) ( *RK >> 24 ) ] ^
                KT1[ (uint8) ( *RK >> 16 ) ] ^
                KT2[ (uint8) ( *RK >>  8 ) ] ^
                KT3[ (uint8) ( *RK       ) ]; RK++;
    }

    RK -= 8;

    *SK++ = *RK++;
    *SK++ = *RK++;
    *SK++ = *RK++;
    *SK++ = *RK++;

    return( 0 );
}

/* AES 128-bit block encryption routine */

void RAOPClient::AesEncrypt( aes_context *ctx, uint8 input[16], uint8 output[16] )
{
    uint32 *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

    RK = ctx->erk;

    GET_UINT32( X0, input,  0 ); X0 ^= RK[0];
    GET_UINT32( X1, input,  4 ); X1 ^= RK[1];
    GET_UINT32( X2, input,  8 ); X2 ^= RK[2];
    GET_UINT32( X3, input, 12 ); X3 ^= RK[3];

#define AES_FROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    RK += 4;                                    \
                                                \
    X0 = RK[0] ^ FT0[ (uint8) ( Y0 >> 24 ) ] ^  \
                 FT1[ (uint8) ( Y1 >> 16 ) ] ^  \
                 FT2[ (uint8) ( Y2 >>  8 ) ] ^  \
                 FT3[ (uint8) ( Y3       ) ];   \
                                                \
    X1 = RK[1] ^ FT0[ (uint8) ( Y1 >> 24 ) ] ^  \
                 FT1[ (uint8) ( Y2 >> 16 ) ] ^  \
                 FT2[ (uint8) ( Y3 >>  8 ) ] ^  \
                 FT3[ (uint8) ( Y0       ) ];   \
                                                \
    X2 = RK[2] ^ FT0[ (uint8) ( Y2 >> 24 ) ] ^  \
                 FT1[ (uint8) ( Y3 >> 16 ) ] ^  \
                 FT2[ (uint8) ( Y0 >>  8 ) ] ^  \
                 FT3[ (uint8) ( Y1       ) ];   \
                                                \
    X3 = RK[3] ^ FT0[ (uint8) ( Y3 >> 24 ) ] ^  \
                 FT1[ (uint8) ( Y0 >> 16 ) ] ^  \
                 FT2[ (uint8) ( Y1 >>  8 ) ] ^  \
                 FT3[ (uint8) ( Y2       ) ];   \
}

    AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 1 */
    AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 2 */
    AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 3 */
    AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 4 */
    AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 5 */
    AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 6 */
    AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 7 */
    AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 8 */
    AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 9 */

    if( ctx->nr > 10 )
    {
        AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );   /* round 10 */
        AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );   /* round 11 */
    }

    if( ctx->nr > 12 )
    {
        AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );   /* round 12 */
        AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );   /* round 13 */
    }

    /* last round */

    RK += 4;

    X0 = RK[0] ^ ( FSb[ (uint8) ( Y0 >> 24 ) ] << 24 ) ^
                 ( FSb[ (uint8) ( Y1 >> 16 ) ] << 16 ) ^
                 ( FSb[ (uint8) ( Y2 >>  8 ) ] <<  8 ) ^
                 ( FSb[ (uint8) ( Y3       ) ]       );

    X1 = RK[1] ^ ( FSb[ (uint8) ( Y1 >> 24 ) ] << 24 ) ^
                 ( FSb[ (uint8) ( Y2 >> 16 ) ] << 16 ) ^
                 ( FSb[ (uint8) ( Y3 >>  8 ) ] <<  8 ) ^
                 ( FSb[ (uint8) ( Y0       ) ]       );

    X2 = RK[2] ^ ( FSb[ (uint8) ( Y2 >> 24 ) ] << 24 ) ^
                 ( FSb[ (uint8) ( Y3 >> 16 ) ] << 16 ) ^
                 ( FSb[ (uint8) ( Y0 >>  8 ) ] <<  8 ) ^
                 ( FSb[ (uint8) ( Y1       ) ]       );

    X3 = RK[3] ^ ( FSb[ (uint8) ( Y3 >> 24 ) ] << 24 ) ^
                 ( FSb[ (uint8) ( Y0 >> 16 ) ] << 16 ) ^
                 ( FSb[ (uint8) ( Y1 >>  8 ) ] <<  8 ) ^
                 ( FSb[ (uint8) ( Y2       ) ]       );

    PUT_UINT32( X0, output,  0 );
    PUT_UINT32( X1, output,  4 );
    PUT_UINT32( X2, output,  8 );
    PUT_UINT32( X3, output, 12 );
}

/* AES 128-bit block decryption routine */

void RAOPClient::AesDecrypt( aes_context *ctx, uint8 input[16], uint8 output[16] )
{
    uint32 *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

    RK = ctx->drk;

    GET_UINT32( X0, input,  0 ); X0 ^= RK[0];
    GET_UINT32( X1, input,  4 ); X1 ^= RK[1];
    GET_UINT32( X2, input,  8 ); X2 ^= RK[2];
    GET_UINT32( X3, input, 12 ); X3 ^= RK[3];

#define AES_RROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    RK += 4;                                    \
                                                \
    X0 = RK[0] ^ RT0[ (uint8) ( Y0 >> 24 ) ] ^  \
                 RT1[ (uint8) ( Y3 >> 16 ) ] ^  \
                 RT2[ (uint8) ( Y2 >>  8 ) ] ^  \
                 RT3[ (uint8) ( Y1       ) ];   \
                                                \
    X1 = RK[1] ^ RT0[ (uint8) ( Y1 >> 24 ) ] ^  \
                 RT1[ (uint8) ( Y0 >> 16 ) ] ^  \
                 RT2[ (uint8) ( Y3 >>  8 ) ] ^  \
                 RT3[ (uint8) ( Y2       ) ];   \
                                                \
    X2 = RK[2] ^ RT0[ (uint8) ( Y2 >> 24 ) ] ^  \
                 RT1[ (uint8) ( Y1 >> 16 ) ] ^  \
                 RT2[ (uint8) ( Y0 >>  8 ) ] ^  \
                 RT3[ (uint8) ( Y3       ) ];   \
                                                \
    X3 = RK[3] ^ RT0[ (uint8) ( Y3 >> 24 ) ] ^  \
                 RT1[ (uint8) ( Y2 >> 16 ) ] ^  \
                 RT2[ (uint8) ( Y1 >>  8 ) ] ^  \
                 RT3[ (uint8) ( Y0       ) ];   \
}

    AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 1 */
    AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 2 */
    AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 3 */
    AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 4 */
    AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 5 */
    AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 6 */
    AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 7 */
    AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );       /* round 8 */
    AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );       /* round 9 */

    if( ctx->nr > 10 )
    {
        AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );   /* round 10 */
        AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );   /* round 11 */
    }

    if( ctx->nr > 12 )
    {
        AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );   /* round 12 */
        AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );   /* round 13 */
    }

    /* last round */

    RK += 4;

    X0 = RK[0] ^ ( RSb[ (uint8) ( Y0 >> 24 ) ] << 24 ) ^
                 ( RSb[ (uint8) ( Y3 >> 16 ) ] << 16 ) ^
                 ( RSb[ (uint8) ( Y2 >>  8 ) ] <<  8 ) ^
                 ( RSb[ (uint8) ( Y1       ) ]       );

    X1 = RK[1] ^ ( RSb[ (uint8) ( Y1 >> 24 ) ] << 24 ) ^
                 ( RSb[ (uint8) ( Y0 >> 16 ) ] << 16 ) ^
                 ( RSb[ (uint8) ( Y3 >>  8 ) ] <<  8 ) ^
                 ( RSb[ (uint8) ( Y2       ) ]       );

    X2 = RK[2] ^ ( RSb[ (uint8) ( Y2 >> 24 ) ] << 24 ) ^
                 ( RSb[ (uint8) ( Y1 >> 16 ) ] << 16 ) ^
                 ( RSb[ (uint8) ( Y0 >>  8 ) ] <<  8 ) ^
                 ( RSb[ (uint8) ( Y3       ) ]       );

    X3 = RK[3] ^ ( RSb[ (uint8) ( Y3 >> 24 ) ] << 24 ) ^
                 ( RSb[ (uint8) ( Y2 >> 16 ) ] << 16 ) ^
                 ( RSb[ (uint8) ( Y1 >>  8 ) ] <<  8 ) ^
                 ( RSb[ (uint8) ( Y0       ) ]       );

    PUT_UINT32( X0, output,  0 );
    PUT_UINT32( X1, output,  4 );
    PUT_UINT32( X2, output,  8 );
    PUT_UINT32( X3, output, 12 );
}

#ifdef TEST

#include <string.h>
#include <stdio.h>

/*
 * Rijndael Monte Carlo Test: ECB mode
 * source: NIST - rijndael-vals.zip
 */

static unsigned char AES_enc_test[3][16] =
{
    { 0xA0, 0x43, 0x77, 0xAB, 0xE2, 0x59, 0xB0, 0xD0,
      0xB5, 0xBA, 0x2D, 0x40, 0xA5, 0x01, 0x97, 0x1B },
    { 0x4E, 0x46, 0xF8, 0xC5, 0x09, 0x2B, 0x29, 0xE2,
      0x9A, 0x97, 0x1A, 0x0C, 0xD1, 0xF6, 0x10, 0xFB },
    { 0x1F, 0x67, 0x63, 0xDF, 0x80, 0x7A, 0x7E, 0x70,
      0x96, 0x0D, 0x4C, 0xD3, 0x11, 0x8E, 0x60, 0x1A }
};

static unsigned char AES_dec_test[3][16] =
{
    { 0xF5, 0xBF, 0x8B, 0x37, 0x13, 0x6F, 0x2E, 0x1F,
      0x6B, 0xEC, 0x6F, 0x57, 0x20, 0x21, 0xE3, 0xBA },
    { 0xF1, 0xA8, 0x1B, 0x68, 0xF6, 0xE5, 0xA6, 0x27,
      0x1A, 0x8C, 0xB2, 0x4E, 0x7D, 0x94, 0x91, 0xEF },
    { 0x4D, 0xE0, 0xC6, 0xDF, 0x7C, 0xB1, 0x69, 0x72,
      0x84, 0x60, 0x4D, 0x60, 0x27, 0x1B, 0xC5, 0x9A }
};

int main( void )
{
    int m, n, i, j;
    aes_context ctx;
    unsigned char buf[16];
    unsigned char key[32];

    for( m = 0; m < 2; m++ )
    {
        printf( "\n Rijndael Monte Carlo Test (ECB mode) - " );

        if( m == 0 ) printf( "encryption\n\n" );
        if( m == 1 ) printf( "decryption\n\n" );

        for( n = 0; n < 3; n++ )
        {
            printf( " Test %d, key size = %3d bits: ",
                    n + 1, 128 + n * 64 );

            fflush( stdout );

            memset( buf, 0, 16 );
            memset( key, 0, 16 + n * 8 );

            for( i = 0; i < 400; i++ )
            {
                aes_set_key( &ctx, key, 128 + n * 64 );

                for( j = 0; j < 9999; j++ )
                {
                    if( m == 0 ) aes_encrypt( &ctx, buf, buf );
                    if( m == 1 ) aes_decrypt( &ctx, buf, buf );
                }

                if( n > 0 )
                {
                    for( j = 0; j < (n << 3); j++ )
                    {
                        key[j] ^= buf[j + 16 - (n << 3)];
                    }
                }

                if( m == 0 ) aes_encrypt( &ctx, buf, buf );
                if( m == 1 ) aes_decrypt( &ctx, buf, buf );

                for( j = 0; j < 16; j++ )
                {
                    key[j + (n << 3)] ^= buf[j];
                }
            }

            if( ( m == 0 && memcmp( buf, AES_enc_test[n], 16 ) ) ||
                ( m == 1 && memcmp( buf, AES_dec_test[n], 16 ) ) )
            {
                printf( "failed!\n" );
                return( 1 );
            }

            printf( "passed.\n" );
        }
    }

    printf( "\n" );

    return( 0 );
}
#endif
