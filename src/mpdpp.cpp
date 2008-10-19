/***************************************************************************
 *   Copyright (C) 2008 by Andrzej Rybczak   *
 *   electricityispower@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "mpdpp.h"

const string playlist_max_message = "playlist is at the max size";

MPDConnection::MPDConnection() : isConnected(0), itsErrorCode(0), itsMaxPlaylistLength(-1), MPD_HOST("localhost"), MPD_PORT(6600), MPD_TIMEOUT(15), itsUpdater(0), itsErrorHandler(0)
{
	itsConnection = 0;
	itsCurrentStats = 0;
	itsOldStats = 0;
	itsCurrentStatus = 0;
	itsOldStatus = 0;
}

MPDConnection::~MPDConnection()
{
	if (itsConnection)
		mpd_closeConnection(itsConnection);
	if (itsOldStats)
		mpd_freeStats(itsOldStats);
	if (itsCurrentStats)
		mpd_freeStats(itsCurrentStats);
	if (itsOldStatus)
		mpd_freeStatus(itsOldStatus);
	if (itsCurrentStatus)
		mpd_freeStatus(itsCurrentStatus);
	ClearQueue();
}

bool MPDConnection::Connect()
{
	if (!isConnected && !itsConnection)
	{
		itsConnection = mpd_newConnection(MPD_HOST.c_str(), MPD_PORT, MPD_TIMEOUT);
		isConnected = 1;
		if (CheckForErrors())
			return false;
		if (!MPD_PASSWORD.empty())
			SendPassword();
		return !CheckForErrors();
	}
	else
		return true;
}

bool MPDConnection::Connected() const
{
	return isConnected;
}

void MPDConnection::Disconnect()
{
	if (itsConnection)
		mpd_closeConnection(itsConnection);
	if (itsOldStats)
		mpd_freeStats(itsOldStats);
	if (itsCurrentStats)
		mpd_freeStats(itsCurrentStats);
	if (itsOldStatus)
		mpd_freeStatus(itsOldStatus);
	if (itsCurrentStatus)
		mpd_freeStatus(itsCurrentStatus);
	itsConnection = 0;
	itsCurrentStats = 0;
	itsOldStats = 0;
	itsCurrentStatus = 0;
	itsOldStatus = 0;
	isConnected = 0;
	itsMaxPlaylistLength = -1;
	ClearQueue();
}

void MPDConnection::SetHostname(const string &host)
{
	int at = host.find("@");
	if (at != string::npos)
	{
		MPD_PASSWORD = host.substr(0, at);
		MPD_HOST = host.substr(at+1);
	}
	else
		MPD_HOST = host;
}

void MPDConnection::SendPassword() const
{
	mpd_sendPasswordCommand(itsConnection, MPD_PASSWORD.c_str());
	mpd_finishCommand(itsConnection);
}

void MPDConnection::SetStatusUpdater(StatusUpdater updater, void *data)
{
	itsUpdater = updater;
	itsStatusUpdaterUserdata = data;
}

void MPDConnection::SetErrorHandler(ErrorHandler handler, void *data)
{
	itsErrorHandler = handler;
	itsErrorHandlerUserdata = data;
}

void MPDConnection::UpdateStatus()
{
	CheckForErrors();
	
	if (itsOldStatus)
		mpd_freeStatus(itsOldStatus);
	itsOldStatus = itsCurrentStatus;
	mpd_sendStatusCommand(itsConnection);
	itsCurrentStatus = mpd_getStatus(itsConnection);
	
	if (!itsMaxPlaylistLength)
		itsMaxPlaylistLength = GetPlaylistLength();
	
	if (CheckForErrors())
		return;
	
	if (itsOldStats)
		mpd_freeStats(itsOldStats);
	itsOldStats = itsCurrentStats;
	mpd_sendStatsCommand(itsConnection);
	itsCurrentStats = mpd_getStats(itsConnection);
	
	if (itsCurrentStatus && itsCurrentStats && itsUpdater)
	{
		MPDStatusChanges changes;
		
		if (itsOldStatus == NULL || itsOldStats == NULL)
		{
			changes.Playlist = 1;
			changes.SongID = 1;
			changes.Database = 1;
			changes.DBUpdating = 1;
			changes.Volume = 1;
			changes.ElapsedTime = 1;
			changes.Crossfade = 1;
			changes.Random = 1;
			changes.Repeat = 1;
			changes.PlayerState = 1;
		}
		else
		{
			changes.Playlist = itsOldStatus->playlist != itsCurrentStatus->playlist;
			changes.SongID = itsOldStatus->songid != itsCurrentStatus->songid;
			changes.Database = itsOldStats->dbUpdateTime != itsCurrentStats->dbUpdateTime;
			changes.DBUpdating = itsOldStatus->updatingDb != itsCurrentStatus->updatingDb;
			changes.Volume = itsOldStatus->volume != itsCurrentStatus->volume;
			changes.ElapsedTime = itsOldStatus->elapsedTime != itsCurrentStatus->elapsedTime;
			changes.Crossfade = itsOldStatus->crossfade != itsCurrentStatus->crossfade;
			changes.Random = itsOldStatus->random != itsCurrentStatus->random;
			changes.Repeat = itsOldStatus->repeat != itsCurrentStatus->repeat;
			changes.PlayerState = itsOldStatus->state != itsCurrentStatus->state;
		}
		itsUpdater(this, changes, itsErrorHandlerUserdata);
	}
}

void MPDConnection::UpdateDirectory(const string &path) const
{
	if (isConnected)
	{
		mpd_sendUpdateCommand(itsConnection, (char *) path.c_str());
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Execute(const string &command) const
{
	if (isConnected)
	{
		mpd_executeCommand(itsConnection, command.c_str());
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Play() const
{
	if (isConnected)
		PlayID(-1);
}

void MPDConnection::Play(int pos) const
{
	if (isConnected)
	{
		mpd_sendPlayCommand(itsConnection, pos);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::PlayID(int id) const
{
	if (isConnected)
	{
		mpd_sendPlayIdCommand(itsConnection, id);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Pause() const
{
	if (isConnected)
	{
		mpd_sendPauseCommand(itsConnection, itsCurrentStatus->state != psPause);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Stop() const
{
	if (isConnected)
	{
		mpd_sendStopCommand(itsConnection);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Next() const
{
	if (isConnected)
	{
		mpd_sendNextCommand(itsConnection);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Prev() const
{
	if (isConnected)
	{
		mpd_sendPrevCommand(itsConnection);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Move(int from, int to) const
{
	if (isConnected)
	{
		mpd_sendMoveCommand(itsConnection, from, to);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Seek(int where) const
{
	if (isConnected)
	{
		mpd_sendSeekCommand(itsConnection, itsCurrentStatus->song, where);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Shuffle() const
{
	if (isConnected)
	{
		mpd_sendShuffleCommand(itsConnection);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::ClearPlaylist() const
{
	if (isConnected)
	{
		mpd_sendClearCommand(itsConnection);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::AddToPlaylist(const string &path, const Song &s) const
{
	if (!s.Empty())
		AddToPlaylist(path, s.GetFile());
}

void MPDConnection::AddToPlaylist(const string &path, const string &file) const
{
	if (isConnected)
	{
		mpd_sendPlaylistAddCommand(itsConnection, (char *) path.c_str(), (char *) file.c_str());
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Move(const string &path, int from, int to) const
{
	if (isConnected)
	{
		mpd_sendPlaylistMoveCommand(itsConnection, (char *) path.c_str(), from, to);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::Rename(const string &from, const string &to) const
{
	if (isConnected)
	{
		mpd_sendRenameCommand(itsConnection, from.c_str(), to.c_str());
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetPlaylistChanges(long long id, SongList &v) const
{
	if (isConnected)
	{
		if (id < 0)
		{
			id = 0;
			v.reserve(GetPlaylistLength());
		}
		mpd_sendPlChangesCommand(itsConnection, id);
		mpd_InfoEntity *item = NULL;
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_SONG)
			{
				Song *s = new Song(item->info.song, 1);
				item->info.song = 0;
				v.push_back(s);
			}
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

Song MPDConnection::GetSong(const string &path) const
{
	if (isConnected)
	{
		mpd_sendListallInfoCommand(itsConnection, path.c_str());
		mpd_InfoEntity *item = NULL;
		item = mpd_getNextInfoEntity(itsConnection);
		Song result = item->info.song;
		item->info.song = 0;
		mpd_freeInfoEntity(item);
		mpd_finishCommand(itsConnection);
		return result;
	}
	else
		return Song();
}

int MPDConnection::GetCurrentSongPos() const
{
	return isConnected && itsCurrentStatus ? (itsCurrentStatus->state == psPlay || itsCurrentStatus->state == psPause ? itsCurrentStatus->song : -1) : -1;
}

Song MPDConnection::GetCurrentSong() const
{
	if (isConnected && (GetState() == psPlay || GetState() == psPause))
	{
		mpd_sendCurrentSongCommand(itsConnection);
		mpd_InfoEntity *item = NULL;
		item = mpd_getNextInfoEntity(itsConnection);
		if (item)
		{
			Song result = item->info.song;
			item->info.song = 0;
			mpd_freeInfoEntity(item);
			return result;
		}
		else
			return Song();
		mpd_finishCommand(itsConnection);
	}
	else
		return Song();
}

void MPDConnection::GetPlaylistContent(const string &path, SongList &v) const
{
	if (isConnected)
	{
		mpd_sendListPlaylistInfoCommand(itsConnection, (char *) path.c_str());
		mpd_InfoEntity *item = NULL;
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_SONG)
			{
				Song *s = new Song(item->info.song);
				item->info.song = 0;
				v.push_back(s);
			}
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::SetRepeat(bool mode) const
{
	if (isConnected)
	{
		mpd_sendRepeatCommand(itsConnection, mode);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::SetRandom(bool mode) const
{
	if (isConnected)
	{
		mpd_sendRandomCommand(itsConnection, mode);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::SetVolume(int vol) const
{
	if (isConnected)
	{
		mpd_sendSetvolCommand(itsConnection, vol);
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::SetCrossfade(int crossfade) const
{
	if (isConnected)
	{
		mpd_sendCrossfadeCommand(itsConnection, crossfade);
		mpd_finishCommand(itsConnection);
	}
}

int MPDConnection::AddSong(const string &path)
{
	int id = -1;
	if (isConnected)
	{
		if (GetPlaylistLength() < itsMaxPlaylistLength)
		{
			id = mpd_sendAddIdCommand(itsConnection, path.c_str());
			mpd_finishCommand(itsConnection);
			UpdateStatus();
		}
		else
			if (itsErrorHandler)
				itsErrorHandler(this, MPD_ACK_ERROR_PLAYLIST_MAX, playlist_max_message, NULL);
	}
	return id;
}

int MPDConnection::AddSong(const Song &s)
{
	return !s.Empty() ? AddSong(s.GetFile()) : -1;
}

void MPDConnection::QueueAddSong(const string &path)
{
	if (isConnected && GetPlaylistLength() < itsMaxPlaylistLength)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctAdd;
		q->item_path = path;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueAddSong(const Song &s)
{
	if (!s.Empty())
		QueueAddSong(s.GetFile());
}

void MPDConnection::QueueAddToPlaylist(const string &playlist, const string &path)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctAddToPlaylist;
		q->playlist_path = playlist;
		q->item_path = path;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueAddToPlaylist(const string &playlist, const Song &s)
{
	if (!s.Empty())
		QueueAddToPlaylist(playlist, s.GetFile());
}

void MPDConnection::QueueDeleteSong(int id)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctDelete;
		q->id = id;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueDeleteSongId(int id)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctDeleteID;
		q->id = id;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueMove(int from, int to)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctMove;
		q->id = from;
		q->id2 = to;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueMove(const string &playlist, int from, int to)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctPlaylistMove;
		q->playlist_path = playlist;
		q->id = from;
		q->id2 = to;
		itsQueue.push_back(q);
	}
}

void MPDConnection::QueueDeleteFromPlaylist(const string &playlist, int pos)
{
	if (isConnected)
	{
		QueueCommand *q = new QueueCommand;
		q->type = qctDeleteFromPlaylist;
		q->playlist_path = playlist;
		q->id = pos;
		itsQueue.push_back(q);
	}
}

bool MPDConnection::CommitQueue()
{
	bool retval = false;
	if (isConnected)
	{
		mpd_sendCommandListBegin(itsConnection);
		for (std::vector<QueueCommand *>::const_iterator it = itsQueue.begin(); it != itsQueue.end(); it++)
		{
			switch ((*it)->type)
			{
				case qctAdd:
					mpd_sendAddCommand(itsConnection, (*it)->item_path.c_str());
					break;
				case qctAddToPlaylist:
					mpd_sendPlaylistAddCommand(itsConnection, (char *) (*it)->playlist_path.c_str(), (char *) (*it)->item_path.c_str());
					break;
				case qctDelete:
					mpd_sendDeleteCommand(itsConnection, (*it)->id);
					break;
				case qctDeleteID:
					mpd_sendDeleteIdCommand(itsConnection, (*it)->id);
					break;
				case qctMove:
					mpd_sendMoveCommand(itsConnection, (*it)->id, (*it)->id2);
					break;
				case qctPlaylistMove:
					mpd_sendPlaylistMoveCommand(itsConnection, (char *) (*it)->playlist_path.c_str(), (*it)->id, (*it)->id2);
					break;
				case qctDeleteFromPlaylist:
					mpd_sendPlaylistDeleteCommand(itsConnection, (char *) (*it)->playlist_path.c_str(), (*it)->id);
					break;
			}
		}
		mpd_sendCommandListEnd(itsConnection);
		mpd_finishCommand(itsConnection);
		UpdateStatus();
		if (GetPlaylistLength() == itsMaxPlaylistLength && itsErrorHandler)
			itsErrorHandler(this, MPD_ACK_ERROR_PLAYLIST_MAX, playlist_max_message, NULL);
		retval = !itsQueue.empty();
	}
	ClearQueue();
	return retval;
}

void MPDConnection::DeletePlaylist(const string &name) const
{
	if (isConnected)
	{
		mpd_sendRmCommand(itsConnection, name.c_str());
		mpd_finishCommand(itsConnection);
	}
}

bool MPDConnection::SavePlaylist(const string &name) const
{
	if (isConnected)
	{
		mpd_sendSaveCommand(itsConnection, name.c_str());
		mpd_finishCommand(itsConnection);
		return !(itsConnection->error == MPD_ERROR_ACK && itsConnection->errorCode == MPD_ACK_ERROR_EXIST);
	}
	else
		return false;
}

void MPDConnection::GetPlaylists(TagList &v) const
{
	if (isConnected)
	{
		ItemList list;
		GetDirectory("/", list);
		for (ItemList::const_iterator it = list.begin(); it != list.end(); it++)
		{
			if (it->type == itPlaylist)
				v.push_back(it->name);
		}
		FreeItemList(list);
	}
}

void MPDConnection::GetList(TagList &v, mpd_TagItems type) const
{
	if (isConnected)
	{
		mpd_sendListCommand(itsConnection, type, NULL);
		char *item;
		while ((item = mpd_getNextTag(itsConnection, type)) != NULL)
		{
			v.push_back(item);
			delete [] item;
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetArtists(TagList &v) const
{
	if (isConnected)
	{
		mpd_sendListCommand(itsConnection, MPD_TABLE_ARTIST, NULL);
		char *item;
		while ((item = mpd_getNextArtist(itsConnection)) != NULL)
		{
			v.push_back(item);
			delete [] item;
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetAlbums(string artist, TagList &v) const
{
	if (isConnected)
	{
		mpd_sendListCommand(itsConnection, MPD_TABLE_ALBUM, artist.empty() ? NULL : artist.c_str());
		char *item;
		while ((item = mpd_getNextAlbum(itsConnection)) != NULL)
		{
			v.push_back(item);
			delete [] item;
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::StartSearch(bool exact_match) const
{
	if (isConnected)
		mpd_startSearch(itsConnection, exact_match);
}

void MPDConnection::StartFieldSearch(mpd_TagItems item)
{
	if (isConnected)
	{
		itsSearchedField = item;
		mpd_startFieldSearch(itsConnection, item);
	}
}

void MPDConnection::AddSearch(mpd_TagItems item, const string &str) const
{
	if (isConnected)
		mpd_addConstraintSearch(itsConnection, item, str.c_str());
}

void MPDConnection::CommitSearch(SongList &v) const
{
	if (isConnected)
	{
		mpd_commitSearch(itsConnection);
		mpd_InfoEntity *item = NULL;
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_SONG)
			{
				Song *s = new Song(item->info.song);
				item->info.song = 0;
				v.push_back(s);
			}
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::CommitSearch(TagList &v) const
{
	if (isConnected)
	{
		mpd_commitSearch(itsConnection);
		char *tag = NULL;
		while ((tag = mpd_getNextTag(itsConnection, itsSearchedField)) != NULL)
		{
			string s_tag = tag;
			if (v.empty() || v.back() != s_tag)
				v.push_back(s_tag);
			delete [] tag;
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetDirectory(const string &path, ItemList &v) const
{
	if (isConnected)
	{
		mpd_InfoEntity *item = NULL;
		mpd_sendLsInfoCommand(itsConnection, path.c_str());
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			Item i;
			switch (item->type)
			{
				case MPD_INFO_ENTITY_TYPE_DIRECTORY:
					i.name = item->info.directory->path;
					i.type = itDirectory;
					break;
				case MPD_INFO_ENTITY_TYPE_SONG:
					i.song = new Song(item->info.song);
					item->info.song = 0;
					i.type = itSong;
					break;
				case MPD_INFO_ENTITY_TYPE_PLAYLISTFILE:
					i.name = item->info.playlistFile->path;
					i.type = itPlaylist;
					break;
			}
			v.push_back(i);
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetDirectoryRecursive(const string &path, SongList &v) const
{
	if (isConnected)
	{
		mpd_InfoEntity *item = NULL;
		mpd_sendListallInfoCommand(itsConnection, path.c_str());
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_SONG)
			{
				Song *s = new Song(item->info.song);
				item->info.song = 0;
				v.push_back(s);
			}
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetSongs(const string &path, SongList &v) const
{
	if (isConnected)
	{
		mpd_InfoEntity *item = NULL;
		mpd_sendLsInfoCommand(itsConnection, path.c_str());
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_SONG)
			{
				Song *s = new Song(item->info.song);
				item->info.song = 0;
				v.push_back(s);
			}
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

void MPDConnection::GetDirectories(const string &path, TagList &v) const
{
	if (isConnected)
	{
		mpd_InfoEntity *item = NULL;
		mpd_sendLsInfoCommand(itsConnection, path.c_str());
		while ((item = mpd_getNextInfoEntity(itsConnection)) != NULL)
		{
			if (item->type == MPD_INFO_ENTITY_TYPE_DIRECTORY)
				v.push_back(item->info.directory->path);
			mpd_freeInfoEntity(item);
		}
		mpd_finishCommand(itsConnection);
	}
}

int MPDConnection::CheckForErrors()
{
	itsErrorCode = 0;
	if (itsConnection->error)
	{
		if (itsConnection->error == MPD_ERROR_ACK)
		{
			// this is to avoid setting too small max size as we check it before fetching current status
			// setting real max playlist length is in UpdateStatus()
			if (itsConnection->errorCode == MPD_ACK_ERROR_PLAYLIST_MAX && itsMaxPlaylistLength == -1)
				itsMaxPlaylistLength = 0;
			
			if (itsErrorHandler)
				itsErrorHandler(this, itsConnection->errorCode, itsConnection->errorStr, itsErrorHandlerUserdata);
			itsErrorCode = itsConnection->errorCode;
		}
		else
		{
			isConnected = 0; // the rest of errors are fatal to connection
			if (itsErrorHandler)
				itsErrorHandler(this, itsConnection->error, itsConnection->errorStr, itsErrorHandlerUserdata);
			itsErrorCode = itsConnection->error;
		}
		itsErrorMessage = itsConnection->errorStr;
		mpd_clearError(itsConnection);
	}
	return itsErrorCode;
}

void MPDConnection::ClearQueue()
{
	for (std::vector<QueueCommand *>::iterator it = itsQueue.begin(); it != itsQueue.end(); it++)
		delete *it;
	itsQueue.clear();
}

void FreeSongList(SongList &l)
{
	for (SongList::iterator i = l.begin(); i != l.end(); i++)
		delete *i;
	l.clear();
}

void FreeItemList(ItemList &l)
{
	for (ItemList::iterator i = l.begin(); i != l.end(); i++)
		delete i->song;
	l.clear();
}

