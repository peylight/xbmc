/*
 *      Copyright (C) 2016 Team Kodi
 *      http://kodi.tv
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

#include "ContextMenuItem.h"
#include "cores/AudioEngine/Engines/ActiveAE/AudioDSPAddons/ActiveAEDSP.h"
#include "epg/EpgInfoTag.h"
#include "pvr/addons/PVRClients.h"
#include "pvr/channels/PVRChannel.h"
#include "pvr/PVRGUIActions.h"
#include "pvr/PVRManager.h"
#include "pvr/recordings/PVRRecording.h"
#include "pvr/timers/PVRTimers.h"
#include "utils/URIUtils.h"

#include "PVRContextMenus.h"

using namespace EPG;

namespace PVR
{
  namespace CONTEXTMENUITEM
  {
    #define DECL_CONTEXTMENUITEM(i) \
    struct i : IContextMenuItem \
    { \
      std::string GetLabel(const CFileItem &item) const override; \
      bool IsVisible(const CFileItem &item) const override; \
      bool Execute(const CFileItemPtr &item) const override; \
    }

    DECL_CONTEXTMENUITEM(ProgrammeInformation);
    DECL_CONTEXTMENUITEM(FindSimilar);
    DECL_CONTEXTMENUITEM(StartRecording);
    DECL_CONTEXTMENUITEM(StopRecording);
    DECL_CONTEXTMENUITEM(AddTimerRule);
    DECL_CONTEXTMENUITEM(EditTimerRule);
    DECL_CONTEXTMENUITEM(DeleteTimerRule);
    DECL_CONTEXTMENUITEM(EditTimer);
    DECL_CONTEXTMENUITEM(DeleteTimer);
    DECL_CONTEXTMENUITEM(ShowAudioDSPSettings);

    ///////////////////////////////////////////////////////////////////////////////
    // Programme information

    std::string ProgrammeInformation::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19047); /* Programme information */
    }

    bool ProgrammeInformation::IsVisible(const CFileItem &item) const
    {
      const CPVRChannelPtr channel(item.GetPVRChannelInfoTag());
      if (channel)
        return channel->GetEPGNow().get() != nullptr;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        return true;

      const CPVRTimerInfoTagPtr timer(item.GetPVRTimerInfoTag());
      if (timer && !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER))
        return timer->GetEpgInfoTag().get() != nullptr;

      return false;
    }

    bool ProgrammeInformation::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().ShowEPGInfo(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Find similar

    std::string FindSimilar::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19003); /* Find similar */
    }

    bool FindSimilar::IsVisible(const CFileItem &item) const
    {
      const CPVRChannelPtr channel(item.GetPVRChannelInfoTag());
      if (channel)
        return channel->GetEPGNow().get() != nullptr;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        return true;

      const CPVRRecordingPtr recording(item.GetPVRRecordingInfoTag());
      if (recording)
        return !recording->IsDeleted();

      return false;
    }

    bool FindSimilar::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().FindSimilar(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Record

    std::string StartRecording::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(264); /* Record */
    }

    bool StartRecording::IsVisible(const CFileItem &item) const
    {
      const CPVRChannelPtr channel(item.GetPVRChannelInfoTag());
      if (channel)
        return g_PVRClients->SupportsTimers(channel->ClientID()) && !channel->IsRecording();

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        return g_PVRClients->SupportsTimers() && !epg->Timer() && epg->EndAsLocalTime() > CDateTime::GetCurrentDateTime();

      return false;
    }

    bool StartRecording::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().AddTimer(item, false);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Stop recording

    std::string StopRecording::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19059); /* Stop recording */
    }

    bool StopRecording::IsVisible(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CPVRChannelPtr channel(item.GetPVRChannelInfoTag());
      if (channel)
        return channel->IsRecording();

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer && !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER))
        return timer->IsRecording();

      return false;
    }

    bool StopRecording::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().StopRecording(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Add timer rule

    std::string AddTimerRule::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19061); /* Add timer */
    }

    bool AddTimerRule::IsVisible(const CFileItem &item) const
    {
      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        return g_PVRClients->SupportsTimers() && !epg->Timer();

      return false;
    }

    bool AddTimerRule::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().AddTimerRule(item, true);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Edit timer rule

    std::string EditTimerRule::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19243); /* Edit timer rule */
    }

    bool EditTimerRule::IsVisible(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer && !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER))
        return timer->GetTimerRuleId() != PVR_TIMER_NO_PARENT;

      return false;
    }

    bool EditTimerRule::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().EditTimerRule(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Delete timer rule

    std::string DeleteTimerRule::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(19295); /* Delete timer rule */
    }

    bool DeleteTimerRule::IsVisible(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer && !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER))
        return timer->GetTimerRuleId() != PVR_TIMER_NO_PARENT;

      return false;
    }

    bool DeleteTimerRule::Execute(const CFileItemPtr &item) const
    {
      CFileItemPtr parentTimer(g_PVRTimers->GetTimerRule(item.get()));
      if (parentTimer)
        return CPVRGUIActions::GetInstance().DeleteTimerRule(parentTimer);

      return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Edit / View timer

    std::string EditTimer::GetLabel(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer)
      {
        const CPVRTimerTypePtr timerType(timer->GetTimerType());
        if (timerType && !timerType->IsReadOnly() && timer->GetTimerRuleId() == PVR_TIMER_NO_PARENT)
        {
          if (epg)
            return g_localizeStrings.Get(19242); /* Edit timer */
          else
            return g_localizeStrings.Get(21450); /* Edit */
        }
      }

      return g_localizeStrings.Get(19241); /* View timer information */
    }

    bool EditTimer::IsVisible(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer && (!epg || !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER)))
      {
        const CPVRTimerTypePtr timerType(timer->GetTimerType());
        return timerType && !timerType->IsReadOnly() && timer->GetTimerRuleId() == PVR_TIMER_NO_PARENT;
      }

      return false;
    }

    bool EditTimer::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().EditTimer(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Delete timer

    std::string DeleteTimer::GetLabel(const CFileItem &item) const
    {
      const CPVRTimerInfoTagPtr timer(item.GetPVRTimerInfoTag());
      if (timer)
        return g_localizeStrings.Get(117); /* Delete */

      return g_localizeStrings.Get(19060); /* Delete timer */
    }

    bool DeleteTimer::IsVisible(const CFileItem &item) const
    {
      CPVRTimerInfoTagPtr timer;

      const CEpgInfoTagPtr epg(item.GetEPGInfoTag());
      if (epg)
        timer = epg->Timer();

      if (!timer)
        timer = item.GetPVRTimerInfoTag();

      if (timer && (!epg || !URIUtils::PathEquals(item.GetPath(), CPVRTimersPath::PATH_ADDTIMER)) && !timer->IsRecording())
      {
        const CPVRTimerTypePtr timerType(timer->GetTimerType());
        return  timerType && !timerType->IsReadOnly();
      }

      return false;
    }

    bool DeleteTimer::Execute(const CFileItemPtr &item) const
    {
      return CPVRGUIActions::GetInstance().DeleteTimer(item);
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Show Audio DSP settings

    std::string ShowAudioDSPSettings::GetLabel(const CFileItem &item) const
    {
      return g_localizeStrings.Get(15047); /* Audio DSP settings */
    }

    bool ShowAudioDSPSettings::IsVisible(const CFileItem &item) const
    {
      if (item.GetPVRChannelInfoTag() || item.GetPVRRecordingInfoTag())
        return CServiceBroker::GetADSP().IsProcessing();

      return false;
    }

    bool ShowAudioDSPSettings::Execute(const CFileItemPtr &item) const
    {
      g_windowManager.ActivateWindow(WINDOW_DIALOG_AUDIO_DSP_OSD_SETTINGS);
      return true;
    }
  } // namespace CONEXTMENUITEM

  CPVRContextMenuManager& CPVRContextMenuManager::GetInstance()
  {
    static CPVRContextMenuManager instance;
    return instance;
  }

  CPVRContextMenuManager::CPVRContextMenuManager()
  {
    m_items =
    {
      std::make_shared<CONTEXTMENUITEM::ProgrammeInformation>(),
      std::make_shared<CONTEXTMENUITEM::FindSimilar>(),
      std::make_shared<CONTEXTMENUITEM::AddTimerRule>(),
      std::make_shared<CONTEXTMENUITEM::EditTimerRule>(),
      std::make_shared<CONTEXTMENUITEM::DeleteTimerRule>(),
      std::make_shared<CONTEXTMENUITEM::EditTimer>(),
      std::make_shared<CONTEXTMENUITEM::DeleteTimer>(),
      std::make_shared<CONTEXTMENUITEM::StartRecording>(),
      std::make_shared<CONTEXTMENUITEM::StopRecording>(),
      std::make_shared<CONTEXTMENUITEM::ShowAudioDSPSettings>(),
    };
  }

} // namespace PVR
