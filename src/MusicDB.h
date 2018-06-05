/*
 * MusicDB.h
 *
 *  Created on: Mar 29, 2010
 *      Author: jdellaria
 */

#ifndef MUSICDB_H_
#define MUSICDB_H_

class MusicDB {
public:
	MusicDB();
	virtual ~MusicDB();
};



#include <mysql/mysql.h>
#include <iconv.h>

struct playQRecord {
    /** Creates a new instance of playQRecord */
	long id;
	long songID;
	char name[255];
	char artist[150];
//    public String composer;
	char album[255];
	char grouping[100];
	char genre[100];
/*    public long size;
    public long songtime;
    public int discnumber;
    public int disccount;*/
	int tracknumber;
	int songyear;
	char datemodified[100];
	char dateadded[100];
/*    public int bitrate;
    public int samplerate;
    public String volumeadjustment;
    public String kind;
    public String equalizer;
    public String comments;
    public int playcount;
    public String lastplayed;
    public String myrating;*/
	char location[255];
//    public long songindex;
	char status[100];
};

MYSQL OpenDBConnection();
void CloseDBConnection();
MYSQL_RES *querySongArtistAlbum( );
struct playQRecord getNextSongRecord();
struct playQRecord getNextAutomaticSongRecord();
void addSong(long songID, char * requestType);
void updatePlayQStatus(long id, char* Status);
void updatePlayQRequestTime(long id);
void updatePlayQEndPlayTime(long id);
void updatePlayQBeginPlayTime(long id);
void updatePlayQTimeField(long id, char * SQLStmt);
MYSQL_RES * queryPlayQ();
struct playQRecord getNextPlayQRecord();
struct playQRecord getCurrentSongInPlayQ();
void setCurrentPlayQSongComplete();
char * getActiveAutoSongQuerySQLStatement();
int incrementSongPlayCount(long songID);
struct playQRecord skipToNextAlbum();
int asciiToUtf8(char *string, size_t maxLen);
void zeroAutomaticSongRowOffset();
void incrementAutomaticSongRowOffset();
void addSongToPlayQ(long songID, char * requestType);
MYSQL_RES * queryAutomaticSong();
bool isSongAvailableInPlayQ();
#endif /* MUSICDB_H_ */
