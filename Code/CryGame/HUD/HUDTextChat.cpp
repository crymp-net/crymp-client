#include <cctype>
#include <cwctype>
#include <string_view>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryGame/GameRules.h"
#include "Library/WinAPI.h"
#include "Library/StringTools.h"

#include "CryGame/GameCVars.h"

#include "HUDTextChat.h"
#include "HUD.h"
#include "HUDRadar.h"

// old chat behavior
#define CHAT_BEHAVIOR_OLD 0
// new chat behavior without server explicitly support (resort to !pm)
#define CHAT_BEHAVIOR_NEW_WITHOUT_SERVER_SUPPORT 1
// new chat behavior with partial server support (send message directly)
#define CHAT_BEHAVIOR_NEW_WITH_PARTIAL_SERVER_SUPPORT 2
// new chat behavior with full server support (server sends message also to originator)
#define CHAT_BEHAVIOR_NEW_WITH_FULL_SERVER_SUPPORT 3

void CHUDTextChat::History::Add(const std::wstring& message)
{
	if (message.empty() || this->messages[this->last] == message)
	{
		return;
	}

	this->last = (this->last + 1) % this->messages.size();
	this->messages[this->last] = message;

	this->ResetSelection();
}

void CHUDTextChat::History::ResetSelection()
{
	this->pos = 0;
}

bool CHUDTextChat::History::MoveUp(std::wstring& message)
{
	if (this->pos >= this->messages.size())
	{
		return false;
	}

	const std::wstring& next = this->messages[(this->last - this->pos) % this->messages.size()];

	if (next.empty())
	{
		return false;
	}

	message = next;

	this->pos++;

	return true;
}

bool CHUDTextChat::History::MoveDown(std::wstring& message)
{
	if (this->pos == 0)
	{
		return false;
	}

	this->pos--;

	if (this->pos == 0)
	{
		message.clear();
	}
	else
	{
		message = this->messages[(this->last - (this->pos - 1)) % this->messages.size()];
	}

	return true;
}

CHUDTextChat::CHUDTextChat(CHUD* pHUD) : m_pHUD(pHUD)
{
	m_inputText.reserve(MAX_MESSAGE_LENGTH);
	m_lastInputText.reserve(MAX_MESSAGE_LENGTH);
}

CHUDTextChat::~CHUDTextChat()
{
	if (m_isListening)
	{
		gEnv->pInput->SetExclusiveListener(nullptr);
	}
}

void CHUDTextChat::Init(CGameFlashAnimation* pFlashChat)
{
	m_flashChat = pFlashChat;

	if (m_flashChat && m_flashChat->GetFlashPlayer())
	{
		m_flashChat->GetFlashPlayer()->SetFSCommandHandler(this);
	}
}

void CHUDTextChat::Update(float deltaTime)
{
	if (!m_flashChat || !m_isListening)
	{
		return;
	}

	if (m_repeatEvent.keyId != eKI_Unknown && !gEnv->pConsole->GetStatus())
	{
		const float now = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

		const float repeatSpeed = 150.f; //CryMP: Default 40.f
		const float nextTimer = (1000.0f / repeatSpeed); // repeat speed

		if ((now - m_repeatTimer) > nextTimer)
		{
			this->ProcessInput(m_repeatEvent);
			m_repeatTimer = now + nextTimer;
		}
	}

	if (m_inputText != m_lastInputText || m_chatTarget != m_lastChatTarget || m_lastChatPMTargetId != m_chatPMTargetId || m_forceUpdate)
	{
		std::string text = StringTools::ToUtf8(m_inputText);

		if (m_chatTarget == eCT_PM && m_chatPMTargetId) {
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(m_chatPMTargetId);
			if (pTarget) {
				text = std::string{ "[To " } + pTarget->GetName() + "] " + text;
			} else {
				m_chatTarget = eCT_All;
				m_chatPMTargetId = 0;
				m_flashChat->Invoke("setShowGlobalChat");
			}
		}

		m_flashChat->Invoke("setInputText", text.c_str());
		m_lastInputText = m_inputText;
		m_lastChatTarget = m_chatTarget;
		m_lastChatPMTargetId = m_chatPMTargetId;
		m_forceUpdate = false;
	}
}

bool CHUDTextChat::OnInputEvent(const SInputEvent& event)
{
	if (!m_flashChat || gEnv->pConsole->IsOpened())
	{
		return false;
	}

	// gamepad virtual keyboard input
	if (event.deviceId == eDI_XI && event.state == eIS_Pressed)
	{
		if (event.keyId == eKI_XI_DPadUp || event.keyId == eKI_PS3_Up)
		{
			this->VirtualKeyboardInput("up");
		}
		else if (event.keyId == eKI_XI_DPadDown || event.keyId == eKI_PS3_Down)
		{
			this->VirtualKeyboardInput("down");
		}
		else if (event.keyId == eKI_XI_DPadLeft || event.keyId == eKI_PS3_Left)
		{
			this->VirtualKeyboardInput("left");
		}
		else if (event.keyId == eKI_XI_DPadRight || event.keyId == eKI_PS3_Right)
		{
			this->VirtualKeyboardInput("right");
		}
		else if (event.keyId == eKI_XI_A || event.keyId == eKI_PS3_Square)
		{
			this->VirtualKeyboardInput("press");
		}
	}

	if (event.deviceId != eDI_Keyboard)
	{
		return false;
	}

	if (event.state == eIS_Released)
	{
		m_repeatEvent.keyId = eKI_Unknown;
	}

	if (event.state != eIS_Pressed)
	{
		return true;
	}

	if (gEnv->pConsole->GetStatus())
	{
		return false;
	}

	if (!gEnv->bMultiplayer)
	{
		return false;
	}

	const float repeatDelay = 200.0f;
	const float now = gEnv->pTimer->GetAsyncTime().GetMilliSeconds();

	m_repeatEvent = event;
	m_repeatTimer = now + repeatDelay;

	if (event.keyId == eKI_Enter || event.keyId == eKI_NP_Enter)
	{
		this->Flush();
	}
	else if (event.keyId == eKI_Escape)
	{
		m_inputText.clear();
		this->Flush();
	}
	else
	{
		this->ProcessInput(event);
	}

	return true;
}

bool CHUDTextChat::OnInputEventUI(const SInputEvent& event)
{
	if (!m_flashChat || gEnv->pConsole->IsOpened())
	{
		return false;
	}

	const char c = event.keyName[0];

	std::wstring wstr = WinAPI::CharToWString(c);

	for (wchar_t wc : wstr) {
		this->Insert(wc);
	}

	return true;
}

void CHUDTextChat::HandleFSCommand(const char* command, const char* args)
{
	if (!m_flashChat)
	{
		return;
	}

	if (_stricmp(command, "sendChatText") == 0)
	{
		const std::string_view text(args);

		for (wchar_t ch : StringTools::ToWide(args) )
		{
			this->Insert(ch);
		}

		this->Flush();
	}
}

void CHUDTextChat::AddChatMessage(EntityId sourceId, EntityId targetId, const wchar_t* msg, int teamFaction, bool teamChat)
{
	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();
	IEntity* pSource = gEnv->pEntitySystem->GetEntity(sourceId);

	std::string strNick{ pSource ? pSource->GetName() : "" };
	// wrap player name in [From/To] envelope if the server at least partially supports new PM system
	if (targetId && g_pGameCVars->mp_chat >= CHAT_BEHAVIOR_NEW_WITH_PARTIAL_SERVER_SUPPORT) {
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
		if (pTarget) {
			if (sourceId == clientId) {
				strNick = std::string{ "[To " } + pTarget->GetName() + "]";
			} else if(pSource && !strcmp(pSource->GetClass()->GetName(), "Player")) {
				strNick = std::string{ "[From " } + strNick + "]";
			}
		}
	}

	if (CanSeeMessageFrom(pSource)) {
		this->AddChatMessage(strNick.c_str(), targetId, msg, teamFaction, teamChat);
	}
}


void CHUDTextChat::AddChatMessage(EntityId sourceId, EntityId targetId, const char* msg, int teamFaction, bool teamChat)
{
	EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();
	IEntity* pSource = gEnv->pEntitySystem->GetEntity(sourceId);

	std::string strNick{ pSource ? pSource->GetName() : "" };
	// wrap player name in [From/To] envelope if the server at least partially supports new PM system
	if (targetId && g_pGameCVars->mp_chat >= CHAT_BEHAVIOR_NEW_WITH_PARTIAL_SERVER_SUPPORT) {
		IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
		if (pTarget) {
			if (sourceId == clientId) {
				strNick = std::string{ "[To " } + pTarget->GetName() + "]";
			} else if (pSource && !strcmp(pSource->GetClass()->GetName(), "Player")) {
				strNick = std::string{ "[From " } + strNick + "]";
			}
		}
	}

	if (CanSeeMessageFrom(pSource)) {
		this->AddChatMessage(strNick.c_str(), targetId, msg, teamFaction, teamChat);
	}
}

void CHUDTextChat::AddChatMessage(const char* nick, EntityId targetId, const wchar_t* msg, int teamFaction, bool teamChat)
{
	if (!m_flashChat)
	{
		return;
	}

	if (teamChat)
	{
		std::wstring nameAndTarget = m_pHUD->LocalizeWithParams("@ui_chat_team", true, nick);
		SFlashVarValue args[3] = { nameAndTarget.c_str(), msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
	else
	{
		SFlashVarValue args[3] = { nick, msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
}

void CHUDTextChat::AddChatMessage(const char* nick, EntityId targetId, const char* msg, int teamFaction, bool teamChat)
{
	if (!m_flashChat)
	{
		return;
	}

	if (teamChat)
	{
		std::wstring nameAndTarget = m_pHUD->LocalizeWithParams("@ui_chat_team", true, nick);
		SFlashVarValue args[3] = { nameAndTarget.c_str(), msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
	else
	{
		SFlashVarValue args[3] = { nick, msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
}

void CHUDTextChat::OpenChat(int type)
{
	if (!m_flashChat || m_isListening)
	{
		return;
	}

	CRadio* pRadio = g_pGame->GetGameRules() ? g_pGame->GetGameRules()->GetRadio() : nullptr;
	if (pRadio && (pRadio->IsOpen() || pRadio->IsPending()))
	{
		pRadio->CloseRadioMenu();
	}

	m_isListening = true;

	gEnv->pInput->ClearKeyState();
	gEnv->pInput->SetExclusiveListener(this);

	m_flashChat->Invoke("setVisibleChatBox", 1);
	m_flashChat->Invoke("GamepadAvailable", m_showVirtualKeyboard);

	if (type == 2)
	{
		m_chatTarget = IsFFA() ? eCT_All : eCT_Team;
	} else if (g_pGameCVars->mp_chat == CHAT_BEHAVIOR_OLD || m_chatTarget == eCT_Team) {
		// we reset target only in old behavior, in new one we reuse previous one
		m_chatTarget = eCT_All;
	}

	if (m_chatTarget == eCT_All) {
		m_flashChat->Invoke("setShowGlobalChat");
	} else {
		m_flashChat->Invoke("setShowTeamChat");
	}

	m_repeatEvent = SInputEvent();
	m_inputText.clear();
	m_cursor = 0;
	m_forceUpdate = true;

	m_history.ResetSelection();
}

void CHUDTextChat::Delete()
{
	if (m_cursor < m_inputText.length())
	{
		m_inputText.erase(m_cursor, 1);
	}
}

void CHUDTextChat::Backspace()
{
	if (m_cursor > 0)
	{
		m_cursor--;
		m_inputText.erase(m_cursor, 1);
	}
}

void CHUDTextChat::Left()
{
	if (m_cursor > 0)
	{
		m_cursor--;
	}
}

void CHUDTextChat::Right()
{
	if (m_cursor < m_inputText.length())
	{
		m_cursor++;
	}
}

void CHUDTextChat::Up()
{
	if (m_history.MoveUp(m_inputText))
	{
		m_cursor = m_inputText.length();
	}
}

void CHUDTextChat::Down()
{
	if (m_history.MoveDown(m_inputText))
	{
		m_cursor = m_inputText.length();
	}
}

void CHUDTextChat::Insert(wchar_t ch)
{
	if (m_inputText.length() >= MAX_MESSAGE_LENGTH)
	{
		return;
	}

	if (!std::iswprint(ch))
	{
		// allow only printable characters
		return;
	}

	m_inputText.insert(m_cursor, 1, ch);
	m_cursor++;
}

void CHUDTextChat::Paste()
{
	for (wchar_t ch : StringTools::ToWide(WinAPI::GetClipboardText(MAX_MESSAGE_LENGTH)))
	{
		this->Insert(ch);
	}
}

void CHUDTextChat::Flush()
{
	gEnv->pInput->ClearKeyState();

	if (!m_inputText.empty())
	{
		EntityId targetId = 0;
		EChatMessageType chatType = eChatToAll;
		switch (m_chatTarget) {
		case eCT_All:
			chatType = eChatToAll;
			break;
		case eCT_Team:
			chatType = eChatToTeam;
			break;
		case eCT_PM:
			chatType = eChatToTarget;
			targetId = m_chatPMTargetId;
			break;
		}

		std::string prefix{};
		const EntityId senderID = m_pHUD->m_pClientActor->GetEntityId();

		// in case improved chat is enabled, but server doesn't support handling it or doesn't specify how to handle it
		// use !pm system that most of SSMs support
		if (chatType == eChatToTarget && g_pGameCVars->mp_chat == CHAT_BEHAVIOR_NEW_WITHOUT_SERVER_SUPPORT) {
			IEntity* pTarget = gEnv->pEntitySystem->GetEntity(targetId);
			if (pTarget) {
				targetId = 0;
				chatType = eChatToAll;
				prefix = std::string{ "!pm " } + pTarget->GetName() + " ";
			}
		}

		m_pHUD->m_pGameRules->SendChatMessage(
			chatType,
			senderID,
			targetId,
			(prefix + StringTools::ToUtf8(m_inputText)).c_str()
		);

		// delivery of messages is handled by servers and most of servers don't or won't implement
		// delivering outgoing message to target back to sender, to handle that
		// force displaying outgoing PM here manually
		if (chatType == eChatToTarget && g_pGameCVars->mp_chat == CHAT_BEHAVIOR_NEW_WITH_PARTIAL_SERVER_SUPPORT) {
			AddChatMessage(senderID, targetId, m_inputText.c_str(), 0, false);
		}

		m_history.Add(m_inputText);

		m_inputText.clear();
		m_cursor = 0;
	}

	m_isListening = false;

	gEnv->pInput->SetExclusiveListener(nullptr);

	m_flashChat->Invoke("setVisibleChatBox", 0);
	m_flashChat->Invoke("GamepadAvailable", false);
}

void CHUDTextChat::ProcessInput(const SInputEvent& event)
{
	if (event.keyId == eKI_Backspace)
	{
		this->Backspace();
	}
	else if (event.keyId == eKI_Delete)
	{
		this->Delete();
	}
	else if (event.keyId == eKI_Left)
	{
		this->Left();
	}
	else if (event.keyId == eKI_Right)
	{
		this->Right();
	}
	else if (event.keyId == eKI_Up)
	{
		this->Up();
	}
	else if (event.keyId == eKI_Down)
	{
		this->Down();
	}
	else if (event.keyId == eKI_Home)
	{
		m_cursor = 0;
	}
	else if (event.keyId == eKI_End)
	{
		m_cursor = m_inputText.length();
	}
	else if (event.keyId == eKI_V && (event.modifiers & eMM_Ctrl))
	{
		this->Paste();
	}
	else if (event.keyId == eKI_Tab) {
		this->RotateTarget();
	}
}

void CHUDTextChat::VirtualKeyboardInput(const char* direction)
{
	if (m_isListening)
	{
		m_flashChat->Invoke("moveCursor", direction);
	}
}

void CHUDTextChat::RotateTarget() {
	if (g_pGameCVars->mp_chat == CHAT_BEHAVIOR_OLD) {
		return;
	}

	bool isFFA = IsFFA();

	if (m_chatTarget == eCT_All && !isFFA) {
		// we move from ALL to TEAM only if match isn't FFA
		// if the match is FFA, we rotate directly to PMs
		m_chatTarget = eCT_Team;
		m_chatPMTargetId = 0;
		m_flashChat->Invoke("setShowTeamChat");
	}
	else if (m_chatTarget == eCT_Team || (m_chatTarget == eCT_All && isFFA)) {
		// we move from TEAM to PMs (first selected player)
		// optionally this applies also when in FFA ALL
		m_chatTarget = eCT_PM;
		std::vector<EntityId> players = GetSelectedTeamMates();
		if (players.size() > 0) {
			m_chatTarget = eCT_PM;
			m_chatPMTargetId = players.at(0);
			m_flashChat->Invoke("setShowTeamChat");
		} else {
			m_chatTarget = eCT_All;
			m_chatPMTargetId = 0;
			m_flashChat->Invoke("setShowGlobalChat");
		}
	}
	else if (m_chatTarget == eCT_PM) {
		// if already in PM and requested PM, rotate to next player, if there is no next player, rotate to ALL
		std::vector<EntityId> players = GetSelectedTeamMates();
		if (players.size() > 0) {
			size_t atNow = -1;
			for (size_t i = 0; i < players.size(); i++) {
				if (players.at(i) == m_chatPMTargetId) {
					atNow = i;
					break;
				}
			}
			if (atNow == -1) {
				// previously selected player probably left
				m_chatTarget = eCT_PM;
				m_chatPMTargetId = players.at(0);
				m_flashChat->Invoke("setShowTeamChat");
			} else if (atNow >= 0 && atNow < players.size() - 1) {
				// rotate to next selected player
				m_chatTarget = eCT_PM;
				m_chatPMTargetId = players.at(atNow + 1);
				m_flashChat->Invoke("setShowTeamChat");
			} else {
				// we rotated past last selected player and go back to ALL
				m_chatTarget = eCT_All;
				m_chatPMTargetId = 0;
				m_flashChat->Invoke("setShowGlobalChat");
			}
		} else {
			// if there is no players, we fallback to ALL
			m_chatTarget = eCT_All;
			m_chatPMTargetId = 0;
			m_flashChat->Invoke("setShowGlobalChat");
		}
	}
}

bool CHUDTextChat::CanSeeMessageFrom(const IEntity* pSource) {
	IGameFramework* pFW = g_pGame->GetIGameFramework();
	EntityId clientActorId = pFW->GetClientActorId();

	IVoiceContext* pVoiceContext = pFW->GetNetContext()->GetVoiceContext();
	if (pSource && pVoiceContext && pVoiceContext->IsMuted(clientActorId, pSource->GetId())) {
		return false;
	}

	if (g_pGameCVars->cl_hud_chat == 1) {
		
		return true;
	} else {
		// if cl_hud_chat is disabled, then player cannot see messages from other players
		if (
			pSource 
			&& !strcmp(pSource->GetClass()->GetName(), "Player") 
			&& pSource->GetId() != clientActorId
		) {
			return false;
		} else {
			return true;
		}
	}
}

bool CHUDTextChat::IsFFA() {
	return g_pGame->GetGameRules()->GetTeamCount() < 2;
}

std::vector<EntityId> CHUDTextChat::GetSelectedTeamMates() {
	std::vector<EntityId> mates;
	auto players = m_pHUD->GetRadar()->GetSelectedTeamMates();
	if (players) {
		for (EntityId id : *players) {
			if (gEnv->pEntitySystem->GetEntity(id)) {
				mates.push_back(id);
			}
		}
	}
	return mates;
}