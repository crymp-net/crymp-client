#include <windows.h>
#include <windowsx.h>

#include <tracy/Tracy.hpp>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/IHardwareMouse.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CryInput/IInput.h"
#include "CryGame/Game.h"
#include "CryGame/Menus/OptionsManager.h"
#include "Launcher/Resources.h"
#include "Library/StringTools.h"

#include "GameWindow.h"

GameWindow GameWindow::s_globalInstance;

static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool cursorCounterIncremented = false;

	switch (msg)
	{
		case WM_MOVE:  // 0x3
		{
			int x = GET_X_LPARAM(lParam);
			int y = GET_Y_LPARAM(lParam);

			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_MOVE, x, y);

			break;
		}
		case WM_SIZE:  // 0x5
		{
			int w = GET_X_LPARAM(lParam);
			int h = GET_Y_LPARAM(lParam);

			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_RESIZE, w, h);

			break;
		}
		case WM_CLOSE:  // 0x10
		{
			gEnv->pSystem->Quit();

			break;
		}
		case WM_INPUTLANGCHANGE:  // 0x51
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LANGUAGE_CHANGE, wParam, lParam);

			break;
		}
		case WM_CHAR:  // 0x102
		{
			if (gEnv->pInput)
			{
				SInputEvent event;
				event.modifiers = gEnv->pInput->GetModifiers();
				event.deviceId = eDI_Keyboard;
				event.state = eIS_UI;
				event.value = 1.0;

				char keyCode = static_cast<char>(wParam);
				wchar_t keyCodeWide[2] = {};

				MultiByteToWideChar(CP_ACP, 0, &keyCode, 1, keyCodeWide, 2);

				// this looks like a mistake
				event.timestamp = keyCodeWide[0];

				// MB_CUR_MAX should be here, but it's slow
				char keyName[5] = {};
				if (wctomb(keyName, static_cast<wchar_t>(wParam)) >= 0)
				{
					if (keyName[1] == 0 && static_cast<unsigned char>(keyName[0]) >= 32)
					{
						event.keyName = keyName;

						gEnv->pInput->PostInputEvent(event);
					}
				}
			}

			break;
		}
		case WM_SYSKEYDOWN:  // 0x104
		{
			switch (wParam)
			{
				case VK_RETURN:  // Alt + Enter
				{
					if (gEnv->pRenderer && gEnv->pRenderer->GetRenderType() != eRT_DX10)
					{
						if (ICVar *pFullscreenCVar = gEnv->pConsole->GetCVar("r_Fullscreen"))
						{
							int fullscreen = pFullscreenCVar->GetIVal();
							pFullscreenCVar->Set(static_cast<int>(fullscreen == 0));
						}
					}

					break;
				}
				case VK_F4:  // Alt + F4
				{
					break;
				}
				default:
				{
					return 0;
				}
			}

			break;
		}
		case WM_SYSCHAR:  // 0x106
		case WM_HOTKEY:  // 0x312
		{
			return 0;
		}
		case WM_MOUSEMOVE:  // 0x200
		{
			if (gEnv->pHardwareMouse)
			{
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);

				gEnv->pHardwareMouse->Event(x, y, HARDWAREMOUSEEVENT_MOVE);
			}

			return 0;
		}
		case WM_LBUTTONDOWN:  // 0x201
		{
			if (gEnv->pHardwareMouse)
			{
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);

				gEnv->pHardwareMouse->Event(x, y, HARDWAREMOUSEEVENT_LBUTTONDOWN);
			}

			return 0;
		}
		case WM_LBUTTONUP:  // 0x202
		{
			if (gEnv->pHardwareMouse)
			{
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);

				gEnv->pHardwareMouse->Event(x, y, HARDWAREMOUSEEVENT_LBUTTONUP);
			}

			return 0;
		}
		case WM_LBUTTONDBLCLK:  // 0x203
		{
			if (gEnv->pHardwareMouse)
			{
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);

				gEnv->pHardwareMouse->Event(x, y, HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK);
			}

			return 0;
		}
		case WM_ENTERMENULOOP:  // 0x211
		case WM_ENTERSIZEMOVE:  // 0x231
		{
			if (gEnv->pHardwareMouse && !cursorCounterIncremented)
			{
				CryLogComment("%s: Changing cursor visibility", __FUNCTION__);
				gEnv->pHardwareMouse->IncrementCounter();
				cursorCounterIncremented = true;
			}

			return 0;
		}
		case WM_EXITMENULOOP:  // 0x212
		case WM_EXITSIZEMOVE:  // 0x232
		case WM_CAPTURECHANGED:  // workaround for missing WM_EXITSIZEMOVE
		{
			if (gEnv->pHardwareMouse && cursorCounterIncremented)
			{
				CryLogComment("%s: Changing cursor visibility", __FUNCTION__);
				gEnv->pHardwareMouse->DecrementCounter();
				cursorCounterIncremented = false;
			}

			if (msg == WM_CAPTURECHANGED)
			{
				break;
			}

			return 0;
		}
		case WM_MOUSEACTIVATE:
		{
			return MA_ACTIVATEANDEAT;
		}
		case WM_ACTIVATE:
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS,
				LOWORD(wParam) != WA_INACTIVE, 0);

			return 0;
		}
		case WM_SETFOCUS:
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 1, 0);

			break;
		}
		case WM_KILLFOCUS:
		{
			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_CHANGE_FOCUS, 0, 0);

			break;
		}
		default:
		{
			break;
		}
	}

	return DefWindowProcA(window, msg, wParam, lParam);
}

GameWindow::GameWindow()
{
}

GameWindow::~GameWindow()
{
	if (m_classID)
	{
		UnregisterClassA("CryENGINE", GetModuleHandleA(nullptr));
	}
}

void GameWindow::Init()
{
	HMODULE exe = GetModuleHandleA(nullptr);

	WNDCLASSA windowClass;
	windowClass.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = exe;
	windowClass.hIcon = LoadIconA(exe, MAKEINTRESOURCEA(RESOURCE_ICON));
	windowClass.hCursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_GREEN));
	windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	windowClass.lpszMenuName = nullptr;
	windowClass.lpszClassName = "CryENGINE";

	m_classID = RegisterClassA(&windowClass);
	if (!m_classID)
	{
		throw StringTools::SysErrorFormat("Failed to register CryENGINE window class");
	}
}

bool GameWindow::OnUpdate()
{
	ZoneScoped;

	MSG msg;

	if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return false;
		}

		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}

	return true;
}

void GameWindow::OnQuit()
{
	MSG msg;

	// the engine contains multiple window message loops
	// but this is the only unfiltered one accepting messages from all windows
	while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

void GameWindow::UpdateCursorColor()
{
	HMODULE exe = GetModuleHandleA(nullptr);
	HCURSOR cursor = nullptr;

	switch (g_pGame->GetOptions()->GetCrysisProfileColor())
	{
		case CrysisProfileColor_Amber:
		{
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_AMBER));
			break;
		}
		case CrysisProfileColor_Blue:
		{
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_BLUE));
			break;
		}
		case CrysisProfileColor_Green:
		{
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_GREEN));
			break;
		}
		case CrysisProfileColor_Red:
		{
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_RED));
			break;
		}
		case CrysisProfileColor_White:
		{
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_WHITE));
			break;
		}
		default:
		{
			// default green cursor
			cursor = LoadCursorA(exe, MAKEINTRESOURCEA(RESOURCE_CURSOR_GREEN));
			break;
		}
	}

	if (cursor)
	{
		HWND window = static_cast<HWND>(gEnv->pRenderer->GetHWND());

		SetClassLongPtrA(window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
		SetCursor(cursor);
	}
}
