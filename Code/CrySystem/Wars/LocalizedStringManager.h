////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2005.
// -------------------------------------------------------------------------
//  File name:   LocalizedStringManager.h
//  Version:     v1.00
//  Created:     22/9/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __LocalizedStringManager_h__
#define __LocalizedStringManager_h__
#pragma once

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryCore/StlUtils.h"

//////////////////////////////////////////////////////////////////////////
/*
	Manage Localization Data
*/
class CLocalizedStringsManager : public ILocalizationManager
{
public:
	CLocalizedStringsManager( ISystem* pSystem );
	virtual ~CLocalizedStringsManager();
	
	// ILocalizationManager
	virtual const char* GetLanguage();
	virtual bool SetLanguage( const char* sLanguage );
	virtual bool LoadExcelXmlSpreadsheet( const char* sFileName, bool bReload=false );
	virtual	void FreeData();

	virtual bool LocalizeString( const string& sString, wstring& outLocalizedString, bool bEnglish=false ) ; // currently faster
	virtual bool LocalizeString( const char* sString, wstring& outLocalizedString, bool bEnglish=false ) ;
	virtual bool LocalizeLabel( const char* sLabel, wstring& outLocalizedString, bool bEnglish=false );
	virtual bool GetLocalizedInfo( const char* sKey, SLocalizedInfo& outInfo );
	virtual int  GetLocalizedStringCount();
	virtual bool GetLocalizedInfoByIndex( int nIndex, SLocalizedInfo& outInfo );
	virtual bool GetLocalizedSoundInfo( const char* sKey, SLocalizedSoundInfo& outSoundInfo );
	virtual bool GetEnglishString( const char *sKey, string &sLocalizedString );
	virtual bool GetSubtitle( const char* sKeyOrLabel, wstring& outSubtitle, bool bForceSubtitle = false);

	virtual void FormatStringMessage( string& outString, const string& sString, const char** sParams, int nParams );
	virtual void FormatStringMessage( string& outString, const string& sString, const char* param1, const char* param2=0, const char* param3=0, const char* param4=0 );
	virtual void FormatStringMessage( wstring& outString, const wstring& sString, const wchar_t** sParams, int nParams );
	virtual void FormatStringMessage( wstring& outString, const wstring& sString, const wchar_t* param1, const wchar_t* param2=0, const wchar_t* param3=0, const wchar_t* param4=0 );

	virtual wchar_t ToUpperCase(wchar_t c);
	virtual wchar_t ToLowerCase(wchar_t c);
	virtual void LocalizeTime(time_t t, bool bMakeLocalTime, bool bShowSeconds, wstring& outTimeString);
	virtual void LocalizeDate(time_t t, bool bMakeLocalTime, bool bShort, bool bIncludeWeekday, wstring& outDateString);
	virtual void LocalizeDuration(int seconds, wstring& outDurationString);
	virtual void ChangeLanguage(const char* language) override { SetLanguage(language); }
	virtual bool LanguageExists(const char* language) const override;
	// ~ILocalizationManager

	int GetMemoryUsage( ICrySizer *pSizer );

private:
	typedef std::map<string, int, stl::less__stricmp<string> >	StringsKeyMap;
	struct SLocalizedStringEntry
	{
		string	sKey;					// key of this entry (without @)
		string	sEnglish;			// english text
		wstring	sLocalized;		// localized text
		string  sEnglishSubtitle; // subtitle. if empty, uses English text
		wstring sLocalizedSubtitle; // localized subtitle. if empty uses sLocalized
		wstring sWho;         // person speaking via XML asset
		// audio specific part
		string	sPrototypeSoundEvent;			// associated sound event prototype (radio, ...)
		float		fVolume;					
		float		fDucking;					
		float		fRadioRatio;
		float		fRadioBackground;
		float		fRadioSquelch;
		// ~audio specific part

		// subtitle
		bool    bUseSubtitle; // should a subtitle displayed for this key?

		// bool    bDependentTranslation; // if the english/localized text contains other localization labels
	};

	struct SLanguage
	{
		string sLanguage;
		StringsKeyMap m_keysMap;
		std::vector<SLocalizedStringEntry*> m_vLocalizedStrings;
	};

	// uses class-local buffer m_tempWString 
	void AppendToUnicodeString( wstring &sDest,const string& sSource );
	void AddLocalizedString( SLanguage *pLanguage,SLocalizedStringEntry *pEntry );
	void AddControl(int nKey);
	void ReplaceEndOfLine( wchar_t *str );
	void ReplaceEndOfLine( char *str );
	//////////////////////////////////////////////////////////////////////////
	void FillCellIndexMapping( XmlNodeRef &rowNode,char *nCellIndexToType );
	void InternalSetCurrentLanguage(SLanguage* pLanguage);
	ISystem* m_pSystem;
	// Pointer to the current language.
	SLanguage* m_pLanguage;

	std::set<string> m_loadedTables;
	
	// Array of loaded languages.
	std::vector<SLanguage*> m_languages;

	typedef std::set<string> PrototypeSoundEvents;
	PrototypeSoundEvents m_prototypeEvents;  // this set is purely used for clever string/string assigning to save memory

	struct less_wcscmp 
	{
		bool operator()( const wstring& left,const wstring& right ) const
		{
			return wcscmp(left.c_str(), right.c_str()) < 0;
		}
	};

	typedef std::set<wstring, less_wcscmp> CharacterNameSet;
	CharacterNameSet m_characterNameSet; // this set is purely used for clever string/string assigning to save memory

	DynArray<wchar_t> m_tempWString;
};


#endif // __LocalizedStringManager_h__
