/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlaylistOperations.h"

#include "GUIUserMessages.h"
#include "PlayListPlayer.h"
#include "guilib/GUIWindowManager.h"
#include "input/Key.h"
#include "messaging/ApplicationMessenger.h"
#include "pictures/GUIWindowSlideShow.h"
#include "pictures/PictureInfoTag.h"
#include "utils/Variant.h"

using namespace JSONRPC;
using namespace PLAYLIST;
using namespace KODI::MESSAGING;

JSONRPC_STATUS CPlaylistOperations::GetPlaylists(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  result = CVariant(CVariant::VariantTypeArray);
  CVariant playlist = CVariant(CVariant::VariantTypeObject);

  playlist["playlistid"] = PLAYLIST_MUSIC;
  playlist["type"] = "audio";
  result.append(playlist);

  playlist["playlistid"] = PLAYLIST_VIDEO;
  playlist["type"] = "video";
  result.append(playlist);

  playlist["playlistid"] = PLAYLIST_PICTURE;
  playlist["type"] = "picture";
  result.append(playlist);

  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetProperties(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  for (unsigned int index = 0; index < parameterObject["properties"].size(); index++)
  {
    std::string propertyName = parameterObject["properties"][index].asString();
    CVariant property;
    JSONRPC_STATUS ret;
    if ((ret = GetPropertyValue(playlist, propertyName, property)) != OK)
      return ret;

    result[propertyName] = property;
  }

  return OK;
}

JSONRPC_STATUS CPlaylistOperations::GetItems(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  CFileItemList list;
  int playlist = GetPlaylist(parameterObject["playlistid"]);

  CGUIWindowSlideShow *slideshow = NULL;
  switch (playlist)
  {
    case PLAYLIST_VIDEO:
    case PLAYLIST_MUSIC:
      CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_GET_ITEMS, playlist, -1, static_cast<void*>(&list));
      break;

    case PLAYLIST_PICTURE:
      slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
      if (slideshow)
        slideshow->GetSlideShowContents(list);
      break;
  }

  HandleFileItemList("id", true, "items", list, parameterObject, result);

  return OK;
}

bool CPlaylistOperations::CheckMediaParameter(int playlist, const CVariant &itemObject)
{
  if (itemObject.isMember("media") && itemObject["media"].asString().compare("files") != 0)
  {
    if (playlist == PLAYLIST_VIDEO && itemObject["media"].asString().compare("video") != 0)
      return false;
    if (playlist == PLAYLIST_MUSIC && itemObject["media"].asString().compare("music") != 0)
      return false;
    if (playlist == PLAYLIST_PICTURE && itemObject["media"].asString().compare("video") != 0 && itemObject["media"].asString().compare("pictures") != 0)
      return false;
  }
  return true;
}

JSONRPC_STATUS CPlaylistOperations::Add(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);

  CGUIWindowSlideShow *slideshow = NULL;
  if (playlist == PLAYLIST_PICTURE)
  {
    slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
    if (slideshow == NULL)
      return FailedToExecute;
  }

  CFileItemList list;
  if (!HandleItemsParameter(playlist, parameterObject["item"], list))
    return InvalidParams;

  switch (playlist)
  {
    case PLAYLIST_VIDEO:
    case PLAYLIST_MUSIC:
    {
      auto tmpList = new CFileItemList();
      tmpList->Copy(list);
      CApplicationMessenger::GetInstance().PostMsg(TMSG_PLAYLISTPLAYER_ADD, playlist, -1, static_cast<void*>(tmpList));
      break;
    }
    case PLAYLIST_PICTURE:
      for (int index = 0; index < list.Size(); index++)
      {
        CPictureInfoTag picture = CPictureInfoTag();
        if (!picture.Load(list[index]->GetPath()))
          continue;

        *list[index]->GetPictureInfoTag() = picture;
        slideshow->Add(list[index].get());
      }
      break;

    default:
      return InvalidParams;
  }

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Insert(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  if (playlist == PLAYLIST_PICTURE)
    return FailedToExecute;

  CFileItemList list;
  if (!HandleItemsParameter(playlist, parameterObject["item"], list))
    return InvalidParams;

  auto tmpList = new CFileItemList();
  tmpList->Copy(list);
  CApplicationMessenger::GetInstance().PostMsg(TMSG_PLAYLISTPLAYER_INSERT, playlist,
    static_cast<int>(parameterObject["position"].asInteger()), static_cast<void*>(tmpList));

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Remove(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  if (playlist == PLAYLIST_PICTURE)
    return FailedToExecute;

  int position = (int)parameterObject["position"].asInteger();
  if (CServiceBroker::GetPlaylistPlayer().GetCurrentPlaylist() == playlist && CServiceBroker::GetPlaylistPlayer().GetCurrentSong() == position)
    return InvalidParams;

  CApplicationMessenger::GetInstance().PostMsg(TMSG_PLAYLISTPLAYER_REMOVE, playlist, position);

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Clear(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  CGUIWindowSlideShow *slideshow = NULL;
  switch (playlist)
  {
    case PLAYLIST_MUSIC:
    case PLAYLIST_VIDEO:
      CApplicationMessenger::GetInstance().PostMsg(TMSG_PLAYLISTPLAYER_CLEAR, playlist);
      break;

    case PLAYLIST_PICTURE:
      slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
      if (!slideshow)
        return FailedToExecute;
      CApplicationMessenger::GetInstance().PostMsg(TMSG_GUI_ACTION, WINDOW_SLIDESHOW, -1, static_cast<void*>(new CAction(ACTION_STOP)));
      slideshow->Reset();
      break;
  }

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::Swap(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  if (playlist == PLAYLIST_PICTURE)
    return FailedToExecute;

  auto tmpVec = new std::vector<int>();
  tmpVec->push_back(static_cast<int>(parameterObject["position1"].asInteger()));
  tmpVec->push_back(static_cast<int>(parameterObject["position2"].asInteger()));
  CApplicationMessenger::GetInstance().PostMsg(TMSG_PLAYLISTPLAYER_SWAP, playlist, -1, static_cast<void*>(tmpVec));

  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::SetShuffle(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  bool shuffle = parameterObject["shuffle"].isBoolean() && parameterObject["shuffle"].asBoolean();
  bool unshuffle = parameterObject["shuffle"].isBoolean() && !parameterObject["shuffle"].asBoolean();
  bool toggle = parameterObject["shuffle"].isString() && parameterObject["shuffle"].asString() == "toggle";

  switch (playlist)
  {
    case PLAYLIST_MUSIC:
    case PLAYLIST_VIDEO:
    {
      if (CServiceBroker::GetPlaylistPlayer().IsShuffled(playlist))
      {
        if (unshuffle || toggle)
          CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_SHUFFLE, playlist, 0);
      }
      else
      {
        if (shuffle || toggle)
          CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_SHUFFLE, playlist, 1);
      }
      break;
    }

    case PLAYLIST_PICTURE:
    {
      CGUIWindowSlideShow *slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
      if (!slideshow)
        return FailedToExecute;
      if (slideshow->IsShuffled())
      {
        if (unshuffle || toggle)
          return FailedToExecute;
      }
      else
      {
        if (shuffle || toggle)
          slideshow->Shuffle();
      }
      break;
    }

    default:
      return FailedToExecute;
  }
  return ACK;
}

JSONRPC_STATUS CPlaylistOperations::SetRepeat(const std::string &method, ITransportLayer *transport, IClient *client, const CVariant &parameterObject, CVariant &result)
{
  int playlist = GetPlaylist(parameterObject["playlistid"]);
  if (playlist == PLAYLIST_PICTURE)
    return FailedToExecute;

  std::string repeat = parameterObject["repeat"].asString();
  REPEAT_STATE state = REPEAT_NONE;
  if (repeat == "cycle")
  {
    REPEAT_STATE statePrev = CServiceBroker::GetPlaylistPlayer().GetRepeat(playlist);
    switch (statePrev)
    {
      case REPEAT_NONE:
        state = REPEAT_ALL;
        break;

      case REPEAT_ALL:
        state = REPEAT_ONE;
        break;

      default:
        state = REPEAT_NONE;
    }
  }
  else if (repeat == "one")
    state = REPEAT_ONE;
  else if (repeat == "all")
    state = REPEAT_ALL;

  CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_REPEAT, playlist, state);

  return ACK;
}

int CPlaylistOperations::GetPlaylist(const CVariant &playlist)
{
  int playlistid = (int)playlist.asInteger();
  if (playlistid > PLAYLIST_NONE && playlistid <= PLAYLIST_PICTURE)
    return playlistid;

  return PLAYLIST_NONE;
}

JSONRPC_STATUS CPlaylistOperations::GetPropertyValue(int playlist, const std::string &property, CVariant &result)
{
  if (property == "type")
  {
    switch (playlist)
    {
      case PLAYLIST_MUSIC:
        result = "audio";
        break;

      case PLAYLIST_VIDEO:
        result = "video";
        break;

      case PLAYLIST_PICTURE:
        result = "pictures";
        break;

      default:
        result = "unknown";
        break;
    }
  }
  else if (property == "size")
  {
    CFileItemList list;
    CGUIWindowSlideShow *slideshow = NULL;
    switch (playlist)
    {
      case PLAYLIST_MUSIC:
      case PLAYLIST_VIDEO:
        CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_GET_ITEMS, playlist, -1, static_cast<void*>(&list));
        result = list.Size();
        break;

      case PLAYLIST_PICTURE:
        slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
        if (slideshow)
          result = slideshow->NumSlides();
        else
          result = 0;
        break;

      default:
        result = 0;
        break;
    }
  }
  else if (property == "shuffled")
  {
    switch (playlist)
    {
      case PLAYLIST_MUSIC:
      case PLAYLIST_VIDEO:
      {
        bool shuffled;
        CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_IS_SHUFFLED, playlist, -1, static_cast<void*>(&shuffled));
        result = shuffled;
        break;
      }

      case PLAYLIST_PICTURE:
      {
        CGUIWindowSlideShow *slideshow = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIWindowSlideShow>(WINDOW_SLIDESHOW);
        if (slideshow)
          result = slideshow->IsShuffled();
        else
          result = -1;
        break;
      }

      default:
        result = -1;
    }
  }
  else if (property == "repeat")
  {
    switch (playlist)
    {
      case PLAYLIST_MUSIC:
      case PLAYLIST_VIDEO:
      {
        REPEAT_STATE state;
        CApplicationMessenger::GetInstance().SendMsg(TMSG_PLAYLISTPLAYER_GET_REPEAT, playlist, -1, static_cast<void*>(&state));
        switch (state)
        {
          case REPEAT_ONE:
            result = "one";
            break;
          case REPEAT_ALL:
            result = "all";
            break;
          default:
            result = "off";
        }
        break;
      }

      case PLAYLIST_PICTURE:
      default:
        result = "off";
    }
  }
  else
    return InvalidParams;

  return OK;
}

bool CPlaylistOperations::HandleItemsParameter(int playlistid, const CVariant &itemParam, CFileItemList &items)
{
  std::vector<CVariant> vecItems;
  if (itemParam.isArray())
    vecItems.assign(itemParam.begin_array(), itemParam.end_array());
  else
    vecItems.push_back(itemParam);

  bool success = false;
  for (auto& itemIt : vecItems)
  {
    if (!CheckMediaParameter(playlistid, itemIt))
      continue;

    switch (playlistid)
    {
    case PLAYLIST_VIDEO:
      itemIt["media"] = "video";
      break;
    case PLAYLIST_MUSIC:
      itemIt["media"] = "music";
      break;
    case PLAYLIST_PICTURE:
      itemIt["media"] = "pictures";
      break;
    }

    success |= FillFileItemList(itemIt, items);
  }

  return success;
}
