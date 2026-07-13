#include <cstring>

#include "CryCommon/CryFont/IFont.h"
#include "CryCommon/CrySystem/gEnv.h"
#include "Library/StringTools.h"
#include "Library/WinAPI.h"

struct CFFont {
	// Vtable[20]
	//! Draw a formated string
	//! \param bASCIIMultiLine true='\','n' is a valid return, false=it's not
	void DrawString1(float x, float y, const char* szMsg, const bool bASCIIMultiLine = true) {
		if (!szMsg) return;
		wchar_t buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		MultiByteToWideChar(65001, 0, szMsg, std::strlen(szMsg), buffer, 1023);
		reinterpret_cast<IFFont*>(this)->DrawStringW(x, y, buffer, bASCIIMultiLine);
	}

	// Vtable[19]
	//! Draw a formated string, taking z into account
	//! \param bASCIIMultiLine true='\','n' is a valid return, false=it's not
	void DrawString2(float x, float y, float z, const char* szMsg, const bool bASCIIMultiLine = true) {
		if (!szMsg) return;
		wchar_t buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		MultiByteToWideChar(65001, 0, szMsg, std::strlen(szMsg), buffer, 1023);
		reinterpret_cast<IFFont*>(this)->DrawStringW(x, y, z, buffer, bASCIIMultiLine);
	}

	// Vtable[21]
	//! Compute the text size
	//! \param bASCIIMultiLine true='\','n' is a valid return, false=it's not
	vector2f GetTextSize(const char* szMsg, const bool bASCIIMultiLine = true) {
		if (!szMsg) return vector2f(0.0f, 0.0f);
		wchar_t buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		MultiByteToWideChar(65001, 0, szMsg, std::strlen(szMsg), buffer, 1023);
		return reinterpret_cast<IFFont*>(this)->GetTextSizeW(buffer, bASCIIMultiLine);
	}

	int GetTextLength(const char* szMsg, const bool bASCIIMultiLine = true) {
		if (!szMsg) return 0;
		wchar_t buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		MultiByteToWideChar(65001, 0, szMsg, std::strlen(szMsg), buffer, 1023);
		return reinterpret_cast<IFFont*>(this)->GetTextLengthW(buffer, bASCIIMultiLine);
	}
};

void PatchCryFont() {
	auto font = gEnv->pCryFont->GetFont("default");
	if (!font) {
		throw StringTools::ErrorFormat("Default font not found!");
	}
	void** CryFontVTable = *reinterpret_cast<void***>(font);

	// vtable hook
	auto drawString1 = &CFFont::DrawString1;
	auto drawString2 = &CFFont::DrawString2;
	auto getTextSize = &CFFont::GetTextSize;
	auto getTextLength = &CFFont::GetTextLength;
	WinAPI::FillMem(&CryFontVTable[20], &reinterpret_cast<void*&>(drawString1), sizeof(void*), false);
	WinAPI::FillMem(&CryFontVTable[19], &reinterpret_cast<void*&>(drawString2), sizeof(void*), false);
	WinAPI::FillMem(&CryFontVTable[21], &reinterpret_cast<void*&>(getTextSize), sizeof(void*), false);
	WinAPI::FillMem(&CryFontVTable[27], &reinterpret_cast<void*&>(getTextLength), sizeof(void*), false);
}
