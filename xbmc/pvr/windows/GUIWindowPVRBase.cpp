/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "GUIWindowPVRBase.h"
#include "GUIWindowPVRRecordings.h" // TODO: wtf?! include of derived class???

#include "Application.h"
#include "addons/AddonManager.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogNumeric.h"
#include "dialogs/GUIDialogExtendedProgressBar.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogYesNo.h"
#include "epg/Epg.h"
#include "epg/GUIEPGGridContainer.h"
#include "filesystem/StackDirectory.h"
#include "GUIUserMessages.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "input/Key.h"
#include "messaging/ApplicationMessenger.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/channels/PVRChannelGroup.h"
#include "pvr/channels/PVRChannelGroupsContainer.h"
#include "pvr/dialogs/GUIDialogPVRRecordingInfo.h"
#include "pvr/PVRGUIActions.h"
#include "pvr/PVRManager.h"
#include "pvr/timers/PVRTimers.h"
#include "ServiceBroker.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#define MAX_INVALIDATION_FREQUENCY 2000 // limit to one invalidation per X milliseconds

using namespace PVR;
using namespace EPG;
using namespace KODI::MESSAGING;

CCriticalSection CGUIWindowPVRBase::m_selectedItemPathsLock;
std::string CGUIWindowPVRBase::m_selectedItemPaths[2];

CGUIWindowPVRBase::CGUIWindowPVRBase(bool bRadio, int id, const std::string &xmlFile) :
  CGUIMediaWindow(id, xmlFile.c_str()),
  m_bRadio(bRadio),
  m_progressHandle(nullptr)
{
  // prevent removable drives to appear in directory listing (base class default behavior).
  m_rootDir.AllowNonLocalSources(false);

  m_selectedItemPaths[false] = "";
  m_selectedItemPaths[true] = "";

  RegisterObservers();
}

CGUIWindowPVRBase::~CGUIWindowPVRBase(void)
{
  UnregisterObservers();
}

void CGUIWindowPVRBase::SetSelectedItemPath(bool bRadio, const std::string &path)
{
  CSingleLock lock(m_selectedItemPathsLock);
  m_selectedItemPaths[bRadio] = path;
}

std::string CGUIWindowPVRBase::GetSelectedItemPath(bool bRadio)
{
  CSingleLock lock(m_selectedItemPathsLock);
  return m_selectedItemPaths[bRadio];
}

void CGUIWindowPVRBase::UpdateSelectedItemPath()
{
  CSingleLock lock(m_selectedItemPathsLock);
  m_selectedItemPaths[m_bRadio] = m_viewControl.GetSelectedItemPath();
}

void CGUIWindowPVRBase::RegisterObservers(void)
{
  CSingleLock lock(m_critSection);
  g_PVRManager.RegisterObserver(this);
  if (m_channelGroup)
    m_channelGroup->RegisterObserver(this);
};

void CGUIWindowPVRBase::UnregisterObservers(void)
{
  CSingleLock lock(m_critSection);
  if (m_channelGroup)
    m_channelGroup->UnregisterObserver(this);
  g_PVRManager.UnregisterObserver(this);
};

void CGUIWindowPVRBase::Notify(const Observable &obs, const ObservableMessage msg)
{
  if (msg == ObservableMessageManagerStopped)
    ClearData();

  if (IsActive())
  {
    CGUIMessage m(GUI_MSG_REFRESH_LIST, GetID(), 0, msg);
    CApplicationMessenger::GetInstance().SendGUIMessage(m);
  }
}

bool CGUIWindowPVRBase::OnAction(const CAction &action)
{
  switch (action.GetID())
  {
    case ACTION_PREVIOUS_CHANNELGROUP:
    case ACTION_NEXT_CHANNELGROUP:
      // switch to next or previous group
      SetChannelGroup(action.GetID() == ACTION_NEXT_CHANNELGROUP ? m_channelGroup->GetNextGroup() : m_channelGroup->GetPreviousGroup());
      return true;
  }

  return CGUIMediaWindow::OnAction(action);
}

bool CGUIWindowPVRBase::OnBack(int actionID)
{
  if (actionID == ACTION_NAV_BACK)
  {
    // don't call CGUIMediaWindow as it will attempt to go to the parent folder which we don't want.
    if (GetPreviousWindow() != WINDOW_FULLSCREEN_LIVETV)
    {
      g_windowManager.ActivateWindow(WINDOW_HOME);
      return true;
    }
    else
      return CGUIWindow::OnBack(actionID);
  }
  return CGUIMediaWindow::OnBack(actionID);
}

void CGUIWindowPVRBase::ClearData()
{
  CSingleLock lock(m_critSection);
  m_channelGroup.reset();
}

void CGUIWindowPVRBase::OnInitWindow(void)
{
  SetProperty("IsRadio", m_bRadio ? "true" : "");

  if (InitChannelGroup())
  {
    CGUIMediaWindow::OnInitWindow();

    // mark item as selected by channel path
    m_viewControl.SetSelectedItem(GetSelectedItemPath(m_bRadio));
  }
  else
  {
    CGUIWindow::OnInitWindow(); // do not call CGUIMediaWindow as it will do a Refresh which in no case works in this state (no cahnnelgroup!)
    ShowProgressDialog(g_localizeStrings.Get(19235), 0); // PVR manager is starting up
  }
}

void CGUIWindowPVRBase::OnDeinitWindow(int nextWindowID)
{
  HideProgressDialog();
  UpdateSelectedItemPath();
  CGUIMediaWindow::OnDeinitWindow(nextWindowID);
}

bool CGUIWindowPVRBase::OnMessage(CGUIMessage& message)
{
  bool bReturn = false;
  switch (message.GetMessage())
  {
    case GUI_MSG_CLICKED:
    {
      switch (message.GetSenderId())
      {
        case CONTROL_BTNCHANNELGROUPS:
          return OpenChannelGroupSelectionDialog();
      }
    }
    break;

    case GUI_MSG_REFRESH_LIST:
    {
      switch (message.GetParam1())
      {
        case ObservableMessageChannelGroupsLoaded:
        {
          // late init
          InitChannelGroup();
          RegisterObservers();
          HideProgressDialog();
          Refresh(true);
          m_viewControl.SetFocused();
          break;
        }
      }
      if (IsActive())
      {
        // Only the active window must set the selected item path which is shared
        // between all PVR windows, not the last notified window (observer).
        UpdateSelectedItemPath();
      }
      bReturn = true;
    }
    break;

    case GUI_MSG_NOTIFY_ALL:
    {
      switch (message.GetParam1())
      {
        case GUI_MSG_UPDATE_SOURCES:
        {
          // removable drive connected/disconnected. base class triggers a window
          // content refresh, which makes no sense for pvr windows.
          bReturn = true;
          break;
        }
      }
    }
    break;
  }

  return bReturn || CGUIMediaWindow::OnMessage(message);
}

bool CGUIWindowPVRBase::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  bool bReturn = false;

  switch(button)
  {
    case CONTEXT_BUTTON_MENU_HOOKS:
      if (itemNumber >= 0 && itemNumber < m_vecItems->Size())
      {
        CFileItemPtr item = m_vecItems->Get(itemNumber);

        if (item->IsEPG() && item->GetEPGInfoTag()->HasPVRChannel())
          g_PVRClients->ProcessMenuHooks(item->GetEPGInfoTag()->ChannelTag()->ClientID(), PVR_MENUHOOK_EPG, item.get());
        else if (item->IsPVRChannel())
          g_PVRClients->ProcessMenuHooks(item->GetPVRChannelInfoTag()->ClientID(), PVR_MENUHOOK_CHANNEL, item.get());
        else if (item->IsDeletedPVRRecording())
          g_PVRClients->ProcessMenuHooks(item->GetPVRRecordingInfoTag()->m_iClientId, PVR_MENUHOOK_DELETED_RECORDING, item.get());
        else if (item->IsUsablePVRRecording())
          g_PVRClients->ProcessMenuHooks(item->GetPVRRecordingInfoTag()->m_iClientId, PVR_MENUHOOK_RECORDING, item.get());
        else if (item->IsPVRTimer())
          g_PVRClients->ProcessMenuHooks(item->GetPVRTimerInfoTag()->m_iClientId, PVR_MENUHOOK_TIMER, item.get());

        bReturn = true;
      }
      break;
    default:
      bReturn = false;
  }

  return bReturn || CGUIMediaWindow::OnContextButton(itemNumber, button);
}

void CGUIWindowPVRBase::SetInvalid()
{
  if (m_refreshTimeout.IsTimePast())
  {
    VECFILEITEMS items = m_vecItems->GetList();
    for (VECFILEITEMS::iterator it = items.begin(); it != items.end(); ++it)
      (*it)->SetInvalid();
    CGUIMediaWindow::SetInvalid();
    m_refreshTimeout.Set(MAX_INVALIDATION_FREQUENCY);
  }
}

bool CGUIWindowPVRBase::CanBeActivated() const
{
  // check if there is at least one enabled PVR add-on
  if (!ADDON::CAddonMgr::GetInstance().HasAddons(ADDON::ADDON_PVRDLL))
  {
    CGUIDialogOK::ShowAndGetInput(CVariant{19296}, CVariant{19272}); // No PVR add-on enabled, You need a tuner, backend software...
    return false;
  }

  return true;
}

bool CGUIWindowPVRBase::OpenChannelGroupSelectionDialog(void)
{
  CGUIDialogSelect *dialog = (CGUIDialogSelect*)g_windowManager.GetWindow(WINDOW_DIALOG_SELECT);
  if (!dialog)
    return false;

  CFileItemList options;
  g_PVRChannelGroups->Get(m_bRadio)->GetGroupList(&options, true);

  dialog->Reset();
  dialog->SetHeading(CVariant{g_localizeStrings.Get(19146)});
  dialog->SetItems(options);
  dialog->SetMultiSelection(false);
  dialog->SetSelected(m_channelGroup->GroupName());
  dialog->Open();

  if (!dialog->IsConfirmed())
    return false;

  const CFileItemPtr item = dialog->GetSelectedFileItem();
  if (!item)
    return false;

  SetChannelGroup(g_PVRChannelGroups->Get(m_bRadio)->GetByName(item->m_strTitle));

  return true;
}

bool CGUIWindowPVRBase::InitChannelGroup()
{
  const CPVRChannelGroupPtr group(g_PVRManager.GetPlayingGroup(m_bRadio));
  if (group)
  {
    CSingleLock lock(m_critSection);
    if (m_channelGroup != group)
    {
      m_viewControl.SetSelectedItem(0);
      m_channelGroup = group;
      m_vecItems->SetPath(GetDirectoryPath());
    }
    return true;
  }
  return false;
}

CPVRChannelGroupPtr CGUIWindowPVRBase::GetChannelGroup(void)
{
  CSingleLock lock(m_critSection);
  return m_channelGroup;
}

void CGUIWindowPVRBase::SetChannelGroup(const CPVRChannelGroupPtr &group)
{
  CSingleLock lock(m_critSection);
  if (!group)
    return;

  if (m_channelGroup != group)
  {
    if (m_channelGroup)
      m_channelGroup->UnregisterObserver(this);
    m_channelGroup = group;
    // we need to register the window to receive changes from the new group
    m_channelGroup->RegisterObserver(this);
    g_PVRManager.SetPlayingGroup(m_channelGroup);
    Update(GetDirectoryPath());
  }
}

bool CGUIWindowPVRBase::PlayFile(CFileItem *item, bool bPlayMinimized /* = false */, bool bCheckResume /* = true */)
{
  if (item->m_bIsFolder)
  {
    return false;
  }

  CPVRChannelPtr channel = item->HasPVRChannelInfoTag() ? item->GetPVRChannelInfoTag() : CPVRChannelPtr();
  if (item->GetPath() == g_application.CurrentFile() ||
      (channel && channel->HasRecording() && channel->GetRecording()->GetPath() == g_application.CurrentFile()))
  {
    CGUIMessage msg(GUI_MSG_FULLSCREEN, 0, GetID());
    g_windowManager.SendMessage(msg);
    return true;
  }

  CMediaSettings::GetInstance().SetVideoStartWindowed(bPlayMinimized);

  if (item->HasPVRRecordingInfoTag())
  {
    return PlayRecording(item, bPlayMinimized, bCheckResume);
  }
  else
  {
    bool bSwitchSuccessful(false);
    CPVRChannelPtr channel(item->GetPVRChannelInfoTag());

    if (channel && g_PVRManager.CheckParentalLock(channel))
    {
      CPVRRecordingPtr recording = channel->GetRecording();
      if (recording)
      {
        CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*) g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
        if (pDialog)
        {
          pDialog->SetHeading(CVariant{19687}); // Play recording
          pDialog->SetLine(0, CVariant{""});
          pDialog->SetLine(1, CVariant{12021}); // Start from beginning
          pDialog->SetLine(2, CVariant{recording->m_strTitle});
          pDialog->Open();

          if (pDialog->IsConfirmed())
          {
            CFileItem recordingItem(recording);
            return PlayRecording(&recordingItem, CSettings::GetInstance().GetBool(CSettings::SETTING_PVRPLAYBACK_PLAYMINIMIZED), bCheckResume);
          }
        }
      }

      /* try a fast switch */
      if ((g_PVRManager.IsPlayingTV() || g_PVRManager.IsPlayingRadio()) &&
         (channel->IsRadio() == g_PVRManager.IsPlayingRadio()))
      {
        if (channel->StreamURL().empty())
          bSwitchSuccessful = g_application.m_pPlayer->SwitchChannel(channel);
      }

      if (!bSwitchSuccessful)
      {
        CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(new CFileItem(*item)));
        return true;
      }
    }

    if (!bSwitchSuccessful)
    {
      std::string channelName = g_localizeStrings.Get(19029); // Channel
      if (channel)
        channelName = channel->ChannelName();
      std::string msg = StringUtils::Format(g_localizeStrings.Get(19035).c_str(), channelName.c_str()); // CHANNELNAME could not be played. Check the log for details.

      CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Error,
              g_localizeStrings.Get(19166), // PVR information
              msg);
      return false;
    }
  }

  return true;
}

bool CGUIWindowPVRBase::CheckResumeRecording(CFileItem *item)
{
  bool bPlayIt(true);
  std::string resumeString = CGUIWindowPVRRecordings::GetResumeString(*item);
  if (!resumeString.empty())
  {
    CContextButtons choices;
    choices.Add(CONTEXT_BUTTON_RESUME_ITEM, resumeString);
    choices.Add(CONTEXT_BUTTON_PLAY_ITEM, 12021); // Start from beginning
    int choice = CGUIDialogContextMenu::ShowAndGetChoice(choices);
    if (choice > 0)
      item->m_lStartOffset = choice == CONTEXT_BUTTON_RESUME_ITEM ? STARTOFFSET_RESUME : 0;
    else
      bPlayIt = false; // context menu cancelled
  }
  return bPlayIt;
}

bool CGUIWindowPVRBase::PlayRecording(CFileItem *item, bool bPlayMinimized /* = false */, bool bCheckResume /* = true */)
{
  if (!item->HasPVRRecordingInfoTag())
    return false;

  std::string stream = item->GetPVRRecordingInfoTag()->m_strStreamURL;
  if (stream.empty())
  {
    if (!bCheckResume || CheckResumeRecording(item))
      CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(new CFileItem(*item)));

    return true;
  }

  /* Isolate the folder from the filename */
  size_t found = stream.find_last_of("/");
  if (found == std::string::npos)
    found = stream.find_last_of("\\");

  if (found != std::string::npos)
  {
    /* Check here for asterisk at the begin of the filename */
    if (stream[found+1] == '*')
    {
      /* Create a "stack://" url with all files matching the extension */
      std::string ext = URIUtils::GetExtension(stream);
      std::string dir = stream.substr(0, found);

      CFileItemList items;
      XFILE::CDirectory::GetDirectory(dir, items);
      items.Sort(SortByFile, SortOrderAscending);

      std::vector<int> stack;
      for (int i = 0; i < items.Size(); ++i)
      {
        if (URIUtils::HasExtension(items[i]->GetPath(), ext))
          stack.push_back(i);
      }

      if (stack.empty())
      {
        /* If we have a stack change the path of the item to it */
        XFILE::CStackDirectory dir;
        std::string stackPath = dir.ConstructStackPath(items, stack);
        item->SetPath(stackPath);
      }
    }
    else
    {
      /* If no asterisk is present play only the given stream URL */
      item->SetPath(stream);
    }
  }
  else
  {
    CLog::Log(LOGERROR, "CGUIWindowPVRBase - %s - can't open recording: no valid filename", __FUNCTION__);
    CGUIDialogOK::ShowAndGetInput(CVariant{19033}, CVariant{19036});
    return false;
  }

  if (!bCheckResume || CheckResumeRecording(item))
    CApplicationMessenger::GetInstance().PostMsg(TMSG_MEDIA_PLAY, 0, 0, static_cast<void*>(new CFileItem(*item)));

  return true;
}

void CGUIWindowPVRBase::ShowRecordingInfo(CFileItem *item)
{
  CGUIDialogPVRRecordingInfo* pDlgInfo = (CGUIDialogPVRRecordingInfo*)g_windowManager.GetWindow(WINDOW_DIALOG_PVR_RECORDING_INFO);
  if (item->IsPVRRecording() && pDlgInfo)
  {
    pDlgInfo->SetRecording(item);
    pDlgInfo->Open();
  }
}

bool CGUIWindowPVRBase::ActionInputChannelNumber(int input)
{
  std::string strInput = StringUtils::Format("%i", input);
  if (CGUIDialogNumeric::ShowAndGetNumber(strInput, g_localizeStrings.Get(19103)))
  {
    int iChannelNumber = atoi(strInput.c_str());
    if (iChannelNumber >= 0)
    {
      int itemIndex = 0;
      VECFILEITEMS items = m_vecItems->GetList();
      for (VECFILEITEMS::iterator it = items.begin(); it != items.end(); ++it)
      {
        if(((*it)->HasPVRChannelInfoTag() && (*it)->GetPVRChannelInfoTag()->ChannelNumber() == iChannelNumber) ||
           ((*it)->HasEPGInfoTag() && (*it)->GetEPGInfoTag()->HasPVRChannel() && (*it)->GetEPGInfoTag()->PVRChannelNumber() == iChannelNumber))
        {
          // different handling for guide grid
          if (GetID() == WINDOW_TV_GUIDE || GetID() == WINDOW_RADIO_GUIDE)
          {
            CGUIEPGGridContainer* epgGridContainer = (CGUIEPGGridContainer*) GetControl(m_viewControl.GetCurrentControl());
            if ((*it)->HasEPGInfoTag() && (*it)->GetEPGInfoTag()->HasPVRChannel())
              epgGridContainer->SetChannel((*it)->GetEPGInfoTag()->ChannelTag());
            else
              epgGridContainer->SetChannel((*it)->GetPVRChannelInfoTag());
          }
          else
            m_viewControl.SetSelectedItem(itemIndex);
          return true;
        }
        ++itemIndex;
      }
    }
  }

  return false;
}

bool CGUIWindowPVRBase::ActionPlayChannel(CFileItem *item)
{
  return PlayFile(item, CSettings::GetInstance().GetBool(CSettings::SETTING_PVRPLAYBACK_PLAYMINIMIZED));
}

bool CGUIWindowPVRBase::ActionPlayEpg(CFileItem *item, bool bPlayRecording)
{
  if (!item || !item->HasEPGInfoTag())
    return false;

  CPVRChannelPtr channel;
  CEpgInfoTagPtr epgTag(item->GetEPGInfoTag());
  if (epgTag && epgTag->HasPVRChannel())
    channel = epgTag->ChannelTag();

  if (!channel || !g_PVRManager.CheckParentalLock(channel))
    return false;

  CFileItem fileItem;
  if (bPlayRecording && epgTag->HasRecording())
    fileItem = CFileItem(epgTag->Recording());
  else
    fileItem = CFileItem(channel);

  g_application.SwitchToFullScreen();
  if (!PlayFile(&fileItem))
  {
    // CHANNELNAME could not be played. Check the log for details.
    std::string msg = StringUtils::Format(g_localizeStrings.Get(19035).c_str(), channel->ChannelName().c_str());
    CGUIDialogOK::ShowAndGetInput(CVariant{19033}, CVariant{std::move(msg)});
    return false;
  }

  return true;
}

bool CGUIWindowPVRBase::ActionDeleteChannel(CFileItem *item)
{
  CPVRChannelPtr channel(item->GetPVRChannelInfoTag());

  /* check if the channel tag is valid */
  if (!channel || channel->ChannelNumber() <= 0)
    return false;

  /* show a confirmation dialog */
  CGUIDialogYesNo* pDialog = (CGUIDialogYesNo*) g_windowManager.GetWindow(WINDOW_DIALOG_YES_NO);
  if (!pDialog)
    return false;
  pDialog->SetHeading(CVariant{19039});
  pDialog->SetLine(0, CVariant{""});
  pDialog->SetLine(1, CVariant{channel->ChannelName()});
  pDialog->SetLine(2, CVariant{""});
  pDialog->Open();

  /* prompt for the user's confirmation */
  if (!pDialog->IsConfirmed())
    return false;

  g_PVRChannelGroups->GetGroupAll(channel->IsRadio())->RemoveFromGroup(channel);
  Refresh(true);

  return true;
}

bool CGUIWindowPVRBase::UpdateEpgForChannel(CFileItem *item)
{
  CPVRChannelPtr channel(item->GetPVRChannelInfoTag());

  CEpgPtr epg = channel->GetEPG();
  if (!epg)
    return false;

  epg->ForceUpdate();
  return true;
}

bool CGUIWindowPVRBase::Update(const std::string &strDirectory, bool updateFilterPath /*= true*/)
{
  if (!GetChannelGroup())
  {
    // no updates before fully initialized
    return false;
  }

  return CGUIMediaWindow::Update(strDirectory, updateFilterPath);
}

void CGUIWindowPVRBase::UpdateButtons(void)
{
  CGUIMediaWindow::UpdateButtons();
  SET_CONTROL_LABEL(CONTROL_BTNCHANNELGROUPS, g_localizeStrings.Get(19141) + ": " + m_channelGroup->GroupName());
}

void CGUIWindowPVRBase::ShowProgressDialog(const std::string &strText, int iProgress)
{
  if (!m_progressHandle)
  {
    CGUIDialogExtendedProgressBar *loadingProgressDialog = dynamic_cast<CGUIDialogExtendedProgressBar *>(g_windowManager.GetWindow(WINDOW_DIALOG_EXT_PROGRESS));
    if (!loadingProgressDialog)
    {
      CLog::Log(LOGERROR, "CGUIWindowPVRBase - %s - unable to get WINDOW_DIALOG_EXT_PROGRESS!", __FUNCTION__);
      return;
    }
    m_progressHandle = loadingProgressDialog->GetHandle(g_localizeStrings.Get(19235)); // PVR manager is starting up
  }

  m_progressHandle->SetPercentage(static_cast<float>(iProgress));
  m_progressHandle->SetText(strText);
}

void CGUIWindowPVRBase::HideProgressDialog(void)
{
  if (m_progressHandle)
  {
    m_progressHandle->MarkFinished();
    m_progressHandle = nullptr;
  }
}
