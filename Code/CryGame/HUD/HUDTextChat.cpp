#include <cctype>
#include <cwctype>
#include <string_view>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryGame/GameRules.h"
#include "Library/WinAPI.h"
#include "Library/StringTools.h"

#include "HUDTextChat.h"
#include "HUD.h"

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

	if (m_inputText != m_lastInputText)
	{
		m_flashChat->Invoke("setInputText", StringTools::ToUtf8(m_inputText).c_str());
		m_lastInputText = m_inputText;
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

void CHUDTextChat::AddChatMessage(EntityId sourceId, const wchar_t* msg, int teamFaction, bool teamChat)
{
	IEntity* source = gEnv->pEntitySystem->GetEntity(sourceId);
	const char* nick = source ? source->GetName() : "";

	this->AddChatMessage(nick, msg, teamFaction, teamChat);
}


void CHUDTextChat::AddChatMessage(EntityId sourceId, const char* msg, int teamFaction, bool teamChat)
{
	IEntity* pSource = gEnv->pEntitySystem->GetEntity(sourceId);
	const char* nick = pSource ? pSource->GetName() : "";

	this->AddChatMessage(nick, msg, teamFaction, teamChat);
}

void CHUDTextChat::AddChatMessage(const char* nick, const wchar_t* msg, int teamFaction, bool teamChat)
{
	if (!m_flashChat)
	{
		return;
	}

	if (teamChat)
	{
		wstring nameAndTarget = m_pHUD->LocalizeWithParams("@ui_chat_team", true, nick);
		SFlashVarValue args[3] = { nameAndTarget.c_str(), msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
	else
	{
		SFlashVarValue args[3] = { nick, msg, teamFaction };
		m_flashChat->Invoke("setChatText", args, 3);
	}
}

void CHUDTextChat::AddChatMessage(const char* nick, const char* msg, int teamFaction, bool teamChat)
{
	if (!m_flashChat)
	{
		return;
	}

	if (teamChat)
	{
		wstring nameAndTarget = m_pHUD->LocalizeWithParams("@ui_chat_team", true, nick);
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
		m_teamChat = true;
		m_flashChat->Invoke("setShowTeamChat");
	}
	else
	{
		m_teamChat = false;
		m_flashChat->Invoke("setShowGlobalChat");
	}

	m_repeatEvent = SInputEvent();
	m_inputText.clear();
	m_cursor = 0;

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
		const EChatMessageType chatType = m_teamChat ? eChatToTeam : eChatToAll;
		const EntityId senderID = m_pHUD->m_pClientActor->GetEntityId();

		m_pHUD->m_pGameRules->SendChatMessage(
			chatType,
			senderID,
			0,
			StringTools::ToUtf8(m_inputText).c_str()
		);

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
}

void CHUDTextChat::VirtualKeyboardInput(const char* direction)
{
	if (m_isListening)
	{
		m_flashChat->Invoke("moveCursor", direction);
	}
}
