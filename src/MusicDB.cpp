/*
 * MusicDB.cpp
 *
 *  Created on: Mar 29, 2010
 *      Author: jdellaria
 */

#include "MusicDB.h"

MusicDB::MusicDB() {
	// TODO Auto-generated constructor stub

}

MusicDB::~MusicDB() {
	// TODO Auto-generated destructor stub
}

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <cstring>

#include <time.h>
#include <DLog.h>

unsigned char	*getval(unsigned char *);

//  Global Variables
	MYSQL	dbaseConnection;
	long rowOffset;
	MYSQL_RES *activeAutomaticSongQueryResult;
	int playAutomatic = 0; // 0 will not pick an automatic song. 1 will find a new song without anything in the play Q

	extern DLog myLog;

int asciiToUtf8(char *string, size_t maxLen)
{
	iconv_t it;
	size_t inLength=16;
	size_t outLength=maxLen;
	char *toCode="UTF8";
	char *fromCode="ISO-8859-1";
	int returnValue;
	char *outString;
	size_t c;
	char *source=string;
	char *result;

	outString = (char *)calloc(outLength,sizeof (char));
	if (outString == NULL)
	{
		returnValue = -2; //out of Memory
		return(returnValue);
	}
	result=outString;
	it=iconv_open(toCode, fromCode);
	if(it!=(iconv_t)-1)
	{

		inLength=strlen(string);

		if(iconv(it, (char **)&string, &inLength, &outString, &outLength)!=-1)
		{
			strcpy (source, result);
			returnValue = 1; //Success
		}
		else
		{
			returnValue = 0; //Failure
		}
		iconv_close(it);
	}
	else
	{
		returnValue = -1; //iconv Open Failure
	}
	free (result);
	return(returnValue);
}

//Music Database Connection
MYSQL OpenDBConnection()
{
	int	err = 0;
	string message;
	/* now to connect to the database */

	if(mysql_init(&dbaseConnection) == NULL)
	{
		err = 1;
	}
	else
	{
		if(mysql_real_connect(&dbaseConnection,"localhost","root","dlirius","Music",0,NULL,0) == NULL)
			err = 1;
	}
	if(err)
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Failed to connect to database: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
//		return(0); Jon
//		exit(0);
	}
	return (dbaseConnection);
}

void CloseDBConnection()
{
	mysql_close(&dbaseConnection);
}

// Songlibrary Table
int incrementSongPlayCount(long songID)
{
	char SQLStmt[200];
	sprintf(SQLStmt,"update songlibrary set PlayCount=Playcount+1, LastPlayed=NOW() where songIndex = %u",songID);
	string message;

	if (mysql_query(&dbaseConnection, SQLStmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Database error: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
		return(0);
	}
	return(1);
}

char * getActiveAutoSongQuerySQLStatement()
{
	char queryStmt[2000];
	string message;

	MYSQL_ROW row;


	sprintf(queryStmt,"SELECT SQLStatement, rowOffset FROM AutoSongQuery where Status = 'Active'");

	if(mysql_real_query(&dbaseConnection,queryStmt,strlen(queryStmt)))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": SQL error");
		myLog.print(logError, message);
//		exit(1);
		return (0);
	}

	activeAutomaticSongQueryResult = mysql_store_result(&dbaseConnection);
	row = mysql_fetch_row(activeAutomaticSongQueryResult);
	rowOffset = atol(row[1]); // get last song that was played from the query
        return(row[0]);
}


MYSQL_RES * queryAutomaticSong()
{
	char queryStmt[2000];
	string message;
	MYSQL_RES *queryAutomaticSongResult;

	sprintf(queryStmt,getActiveAutoSongQuerySQLStatement());
	if (queryStmt == 0)
	{
		message = __func__;
		message.append(": getActiveAutoSongQuerySQLStatement returned 0"); //Jon
		myLog.print(logError, message);
		return (0);
	}

	message = __func__;
	message.append(": before mysql_real_query"); //Jon
	myLog.print(logDebug, message);

	if(mysql_real_query(&dbaseConnection,queryStmt,strlen(queryStmt)))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": SQL error");
		myLog.print(logError, message);
		return(0);
//		exit(1);
	}

	message = __func__;
	message.append(": before mysql_store_result"); //Jon
	myLog.print(logDebug, message);

	queryAutomaticSongResult = mysql_store_result(&dbaseConnection);

	message = __func__;
	message.append(": before mysql_data_seek"); //Jon
	myLog.print(logDebug, message);

	mysql_data_seek(queryAutomaticSongResult, rowOffset);

        return(queryAutomaticSongResult);
}

struct playQRecord getNextAutomaticSongRecord()
{

	int	i;
	MYSQL_ROW row;
	struct playQRecord pQR;
	MYSQL_RES *queryAutomaticSongResult;
	string message;

	message = __func__;
	message.append(": before queryAutomaticSong"); //Jon
	myLog.print(logDebug, message);

	queryAutomaticSongResult = queryAutomaticSong();
	if (queryAutomaticSongResult == 0)
	{
		message = __func__;
		message.append(": queryAutomaticSong returned 0 setting autoplay to manual"); //Jon
		myLog.print(logError, message);
		playAutomatic = 0;
		pQR.id = 0;//MusicPlayQResultSet.getLong(1);
		pQR.songID = 0;//MusicPlayQResultSet.getLong(2);
		return (pQR);
	}
	row = mysql_fetch_row(queryAutomaticSongResult);
	if (row == NULL) // if a null is returned, assume that we are at the end of the result set
	{
		message = __func__;
		message.append(": (row == NULL)"); //Jon
		myLog.print(logDebug, message);

		mysql_free_result(queryAutomaticSongResult);
		zeroAutomaticSongRowOffset();
		queryAutomaticSongResult = queryAutomaticSong();
		row = mysql_fetch_row(queryAutomaticSongResult);
	}
	message = __func__;
	message.append(": before incrementAutomaticSongRowOffset"); //Jon
	myLog.print(logDebug, message);

	incrementAutomaticSongRowOffset(); // Save current song result offset back to the record

	pQR.id = 0;//MusicPlayQResultSet.getLong(1);
	pQR.songID = atol(row[0]);//MusicPlayQResultSet.getLong(2);
	sprintf(pQR.name,row[1]);//MusicPlayQResultSet.getString(3);
	sprintf(pQR.artist,row[2]);//MusicPlayQResultSet.getString(4);
	sprintf(pQR.location,row[3]);//MusicPlayQResultSet.getString(5);
//	sprintf(pQR.status,row[5]);//MusicPlayQResultSet.getString(6);
	sprintf(pQR.album,row[4]);//MusicPlayQResultSet.getString(7);
	sprintf(pQR.genre,row[5]);//MusicPlayQResultSet.getString(8);
	pQR.tracknumber =  atol(row[6]);//MusicPlayQResultSet.getInt(9);
	pQR.songyear =  atol(row[7]);//MusicPlayQResultSet.getInt(10);
	mysql_free_result(queryAutomaticSongResult);
	message = __func__;
	message.append(": before reurn"); //Jon
	myLog.print(logDebug, message);

	return (pQR);
}

void incrementAutomaticSongRowOffset()
{
	char SQLStmt[200];
	string message;

	sprintf(SQLStmt, "Update AutoSongQuery set rowOffset = rowOffset+1 where Status = 'Active'");
	if (mysql_query(&dbaseConnection, SQLStmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": SQL error");
		myLog.print(logError, message);
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Database error: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
		return;
//		exit(0);
	}
}

void zeroAutomaticSongRowOffset()
{
	char SQLStmt[200];
	string message;

	sprintf(SQLStmt, "Update AutoSongQuery set rowOffset = 0 where Status = 'Active'");
	if (mysql_query(&dbaseConnection, SQLStmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Database error: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
//		exit(0);
	}
}

struct playQRecord getNextSongRecord()
{

	int	i;
	int	nrows;
	unsigned long playQID;
	struct playQRecord pQR;
	MYSQL_RES *queryPlayQResult;
	string message;

	message = "MusicDB.cpp "; // Jon
	message.append(__func__);
	message.append(": before getCurrentSongInPlayQ");
	myLog.print(logDebug, message);

	pQR = getCurrentSongInPlayQ();

	if (pQR.id > 0)
	{
		updatePlayQStatus(pQR.id, "Completed");
		updatePlayQEndPlayTime(pQR.id);
		incrementSongPlayCount(pQR.songID);
	}

	message = "MusicDB.cpp "; // Jon
	message.append(__func__);
	message.append(": before queryPlayQ() ");
	myLog.print(logDebug, message);

	queryPlayQResult = queryPlayQ();

	message = "MusicDB.cpp "; // Jon
	message.append(__func__);
	message.append(": before mysql_num_rows() ");
	myLog.print(logDebug, message);

	nrows = mysql_num_rows(queryPlayQResult);



	if (nrows > 0)
	{
		message = "MusicDB.cpp "; // Jon
		message.append(__func__);
		message.append(": (nrows > 0) ");
		myLog.print(logDebug, message);

		pQR = getNextPlayQRecord();
	}
	else
	{
		if (playAutomatic == 1)
		{
			message = "MusicDB.cpp "; // Jon
			message.append(__func__);
			message.append(": (playAutomatic == 1) ");
			myLog.print(logDebug, message);

			pQR = getNextAutomaticSongRecord();
			addSongToPlayQ(pQR.songID, "Automatic");
			pQR = getNextPlayQRecord();
		}
		else
		{
			message = "MusicDB.cpp "; // Jon
			message.append(__func__);
			message.append(": pQR.id = 0 ");
			myLog.print(logDebug, message);

			pQR.id = 0;
		}
	}
	if (pQR.id != 0)
	{
		updatePlayQBeginPlayTime(pQR.id);
		updatePlayQStatus(pQR.id, "Currently Playing");
	}
	mysql_free_result(queryPlayQResult);
	return (pQR);
}

bool isSongAvailableInPlayQ()
{
	int	nrows;
	MYSQL_RES *queryPlayQResult;

	queryPlayQResult = queryPlayQ();
	nrows = mysql_num_rows(queryPlayQResult);
	mysql_free_result(queryPlayQResult);
	if ((nrows > 0) || (playAutomatic == 1))
	{
		return (true);
	}
	else
	{
		return (false);
	}
}

struct playQRecord skipToNextAlbum()
{

	int	i = 0;
	int	nrows;
	unsigned long playQID;
	struct playQRecord pQR;
	struct playQRecord nextQR;
	MYSQL_RES *queryPlayQResult;

	pQR = getCurrentSongInPlayQ();
	if (pQR.id > 0)
	{
		updatePlayQStatus(pQR.id, "Canceled");
		updatePlayQEndPlayTime(pQR.id);
	}

	do
	{
		queryPlayQResult = queryPlayQ();
		nrows = mysql_num_rows(queryPlayQResult);
		if (nrows > 0)  //items in the Play Q
		{
			nextQR = getNextPlayQRecord();
			if (strcmp(pQR.album,nextQR.album)==0)
			{
				updatePlayQStatus(nextQR.id, "Canceled");
				updatePlayQEndPlayTime(nextQR.id);
			}
			else
			{
				i++;
			}
		}
		else if(playAutomatic == 1) //items in Automatic Q
		{
			nextQR = getNextAutomaticSongRecord();
			if (strcmp(pQR.album,nextQR.album)!=0)
			{
				addSongToPlayQ(nextQR.songID, "Automatic");
				i++;
			}
		}
		mysql_free_result(queryPlayQResult);
	}while(i == 0);
	return (nextQR);
}

// PlayQ Table
void addSongToPlayQ(long songID, char * requestType)
{
	char SQLStmt[200];
	string message;

	sprintf(SQLStmt, "INSERT INTO playq (songID,RequestType) VALUES(%u, '%s')",songID,requestType);
	if (mysql_query(&dbaseConnection, SQLStmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Database error: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
//		exit(0);
	}
	return;
}

void updatePlayQStatus(long id, char * Status)
{
	char SQLStmt[200];
	string message;

	sprintf(SQLStmt, "Update playq set Status = '%s' where id = %u",Status, id);
	if (mysql_query(&dbaseConnection, SQLStmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": Database error: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
//		exit(0);
	}
}

void updatePlayQRequestTime(long id)
{
	char * SQLStmt = "Update playq set RequestTime = ? where id = ?";
	updatePlayQTimeField( id, SQLStmt);
}

void  updatePlayQEndPlayTime(long id)
{

	char * SQLStmt = "Update playq set EndPlayTime = ? where id = ?";
	updatePlayQTimeField( id, SQLStmt);
}

void  updatePlayQBeginPlayTime(long id)
{

	char * SQLStmt = "Update playq set BeginPlayTime = ? where id = ?";

	updatePlayQTimeField( id, SQLStmt);

}

void  updatePlayQTimeField(long id, char * SQLStmt)
{
	MYSQL_TIME  ts;
	MYSQL_BIND  bind[3];
	MYSQL_STMT  *stmt;
	string message;

	stmt = mysql_stmt_init(&dbaseConnection);
	if (!stmt)
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": mysql_stmt_init(), out of memory: ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
//		exit(0);
	}
	if (mysql_stmt_prepare( stmt, SQLStmt, strlen(SQLStmt)))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": ");
		message.append(mysql_stmt_error(stmt));
		myLog.print(logError, message);
//		exit(0);
	}

	memset(bind, 0, sizeof(bind));
	/* set up input buffers for all 3 parameters */
	bind[0].buffer_type= MYSQL_TYPE_TIMESTAMP;
	bind[0].buffer= (char *)&ts;
	bind[0].is_null= 0;
	bind[0].length= 0;

	/* INTEGER PARAM */
	/* This is a number type, so there is no need to specify buffer_length */
	bind[1].buffer_type= MYSQL_TYPE_LONG;
	bind[1].buffer= (char *)&id;
	bind[1].is_null= 0;
	bind[1].length= 0;


	mysql_stmt_bind_param(stmt, bind);

	//Get the current time
	struct tm *ptr;
	time_t lt;
	lt = time(NULL);
	ptr = localtime(&lt);


	/* supply the data to be sent in the ts structure */
	ts.year= 1900 + ptr->tm_year;
	ts.month= 1 + ptr->tm_mon;
	ts.day= ptr->tm_mday;

	ts.hour= ptr->tm_hour;
	ts.minute= ptr->tm_min;
	ts.second= ptr->tm_sec;
	ts.second_part = 0;
	ts.neg = 0;
	/* Execute the INSERT statement - 1*/
	if (mysql_stmt_execute(stmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": ");
		message.append(mysql_stmt_error(stmt));
		myLog.print(logError, message);
//		exit(0);
	}
	/* Close the statement */
	if (mysql_stmt_close(stmt))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": failed while closing the statement: ");
		message.append(mysql_stmt_error(stmt));
		myLog.print(logError, message);
	//	exit(0);
	}
}

MYSQL_RES * queryPlayQ()
{
	char queryStmt[5000];
	string message;
	MYSQL_RES *queryPlayQResult;

	sprintf(queryStmt,"SELECT playq.id, playq.songID, Name, Artist, Location, playq.Status, Album, Genre, TrackNumber, SongYear  FROM songlibrary inner join playq on songlibrary.SongIndex = playq.songID where (playq.Status = 'In Queue') order by playq.Arrangement asc, playq.Status, playq.RequestType desc, playq.id asc");
	if(mysql_real_query(&dbaseConnection,queryStmt,strlen(queryStmt)))
	{
		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": SQL error");
		myLog.print(logError, message);
		return(0);
//		exit(1);
	}
	queryPlayQResult = mysql_store_result(&dbaseConnection);
	return(queryPlayQResult);
}

struct playQRecord getNextPlayQRecord()
{
	int	i;
	MYSQL_ROW row;
	struct playQRecord pQR;
	MYSQL_RES *queryPlayQResult;

	queryPlayQResult = queryPlayQ();
	row = mysql_fetch_row(queryPlayQResult);
	pQR.id = atol(row[0]);
	pQR.songID = atol(row[1]);
	sprintf(pQR.name,row[2]);
	sprintf(pQR.artist,row[3]);
	sprintf(pQR.location,row[4]);
	sprintf(pQR.status,row[5]);
	sprintf(pQR.album,row[6]);
	sprintf(pQR.genre,row[7]);
	pQR.tracknumber =  atoi(row[8]);
	pQR.songyear = atoi(row[9]);
	mysql_free_result(queryPlayQResult);
	return (pQR);
}

struct playQRecord getCurrentSongInPlayQ()
{
//	char * SQLStmt = "Select id from playq where Status = 'Currently Playing'";
	char * SQLStmt = "SELECT playq.id, playq.songID, Name, Artist, Location, playq.Status, Album, Genre, TrackNumber, SongYear  FROM songlibrary inner join playq on songlibrary.SongIndex = playq.songID where Status = 'Currently Playing'";

	MYSQL_ROW row;
	MYSQL_RES *queryResult;
	int	nrows;
	long returnValue;
	struct playQRecord pQR = {0};
	string message;

	if (mysql_query(&dbaseConnection, SQLStmt))
	{

		message = "MusicDB.cpp ";
		message.append(__func__);
		message.append(": ");
		message.append(mysql_error(&dbaseConnection));
		myLog.print(logError, message);
		return(pQR);
//		exit(0);
	}
	queryResult = mysql_store_result(&dbaseConnection);
	nrows = mysql_num_rows(queryResult);
	if (nrows == 0)
	{
		pQR.id = 0; //if id = 0 then no rows were returned
		message = "MusicDB.cpp "; // Jon
		message.append(__func__);
		message.append(": pQR.id = 0");
		myLog.print(logDebug, message);
	}
	else
	{
		row = mysql_fetch_row(queryResult);
		pQR.id = atol(row[0]);
		pQR.songID = atol(row[1]);
		sprintf(pQR.name,row[2]);
		sprintf(pQR.artist,row[3]);
		sprintf(pQR.location,row[4]);
		sprintf(pQR.status,row[5]);
		sprintf(pQR.album,row[6]);
		sprintf(pQR.genre,row[7]);
		pQR.tracknumber =  atoi(row[8]);
		pQR.songyear = atoi(row[9]);

		message = "MusicDB.cpp "; // Jon
		message.append(__func__);
		message.append(": song returned");
		myLog.print(logDebug, message);

	}
	mysql_free_result(queryResult);
	return (pQR);
}

void setCurrentPlayQSongComplete()
{
	struct playQRecord pQR;
	pQR = getCurrentSongInPlayQ();
	updatePlayQStatus(pQR.id, "Completed");
}
