// XConsoleVariable.h: interface for the CXConsoleVariable class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_)
#define AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VAR_STRING_SIZE 256

#include "CryCommon/CryCore/BitFiddling.h"
//#include "VariantVar.h"							// CVariantVar

struct IScriptSystem;
class CXConsole;

inline int TextToInt( const char* s, int nCurrent, bool bBitfield )
{
	int nValue = 0;
	if (s)
	{
		char* e;
		if (bBitfield)
		{
			// Bit manipulation.
			if (*s == '^')
				// Bit number
				nValue = 1 << strtol(++s, &e, 10);
			else
				// Full number
				nValue = strtol(s, &e, 10);

			// Check letter codes.
			for (; *e >= 'a'&& *e <= 'z'; e++)
				nValue |= AlphaBit(*e);

			if (*e == '+')
				nValue = nCurrent | nValue;
			else if (*e == '-')
				nValue = nCurrent & ~nValue;
			else if (*e == '^')
				nValue = nCurrent ^ nValue;
		}
		else
			nValue = strtol(s, &e, 10);
	}
	return nValue;
}

class CXConsoleVariableBase :public ICVar
{                     
public:
	//! constructor
	//! \param pConsole must not be 0
	CXConsoleVariableBase( CXConsole *pConsole, const char *sName, int nFlags, const char *help);
	//! destructor
	virtual ~CXConsoleVariableBase();

	// interface ICVar --------------------------------------------------------------------------------------

	virtual void ClearFlags(int flags);
	virtual int GetFlags();
	virtual int SetFlags( int flags );
	virtual const char* GetName() const;
	virtual const char* GetHelp();
	virtual void Release();
	virtual void ForceSet(const char* s);
	virtual void SetOnChangeCallback( ConsoleVarFunc pChangeFunc );
	virtual void GetMemoryUsage( class ICrySizer* pSizer );
	virtual int GetRealIVal() const { return GetIVal(); }
	virtual void DebugLog(const int, const EConsoleLogMode) const override {}

protected: // ------------------------------------------------------------------------------------------

	char *						m_szName;											// if VF_COPYNAME then this data need to be deleteed, otherwise it's pointer to .dll/.exe 

	char *						m_psHelp;											// pointer to the help string, might be 0
	int								m_nFlags;											// e.g.  VF_SAVEGAME, VF_CHEAT, ...

	ConsoleVarFunc    m_pChangeFunc;                // Callback function that is called when this variable changes.
	CXConsole *				m_pConsole;										// used for the callback OnBeforeVarChange()
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableString :public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableString( CXConsole *pConsole, const char *sName, const char *szDefault, int nFlags, const char *help )
		:CXConsoleVariableBase(pConsole,sName,nFlags,help)
	{
		m_sValue[0]=0;

		_Set(szDefault);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int GetIVal() const { return atoi(m_sValue); }
	virtual float GetFVal() const { return (float)atof(m_sValue); }
	virtual const char *GetString() { return m_sValue; }
	virtual void Set( const char* s )
	{
		if (m_nFlags & VF_READONLY)
			return;

		if (!s && strncmp(s,m_sValue,VAR_STRING_SIZE) == 0)
			return;

		if(m_pConsole->OnBeforeVarChange(this,s))
		{
			m_nFlags |= VF_MODIFIED;
			if(s==0)
				_Set("");
			else
				_Set(s);

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const float f )
	{ 
		char sTemp[VAR_STRING_SIZE];
		sprintf(sTemp,"%f",f);

		if (strncmp(sTemp,m_sValue,VAR_STRING_SIZE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(sTemp);
	}
	virtual void Set( const int i )
	{ 
		char sTemp[VAR_STRING_SIZE];
		sprintf(sTemp,"%d",i);
		
		if (strncmp(sTemp,m_sValue,VAR_STRING_SIZE) == 0)
			return;

		m_nFlags |= VF_MODIFIED;
		Set(sTemp);
	}
	virtual int GetType() { return CVAR_STRING; }

private: // --------------------------------------------------------------------------------------------

	char 							m_sValue[VAR_STRING_SIZE];											//!<

	//! \param szValue must not be 0
	void _Set( const char *szValue )
	{
		assert(szValue);

		int iLen=strlen(szValue);

		if(iLen<VAR_STRING_SIZE)
			memcpy(m_sValue,szValue,iLen+1);
		else
		{
			memcpy(m_sValue,szValue,VAR_STRING_SIZE-1);
			m_sValue[VAR_STRING_SIZE-1]=0;
		}
	}
};



class CXConsoleVariableInt :public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableInt( CXConsole *pConsole, const char *sName, const int iDefault, int nFlags, const char *help )
		:CXConsoleVariableBase(pConsole,sName,nFlags,help), m_iValue(iDefault)
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int GetIVal() const { return m_iValue; }
	virtual float GetFVal() const { return (float)GetIVal(); }
	virtual const char *GetString() 
	{
		static char szReturnString[256];

		sprintf(szReturnString,"%d",GetIVal());
		return szReturnString;
	}
	virtual void Set( const char* s )
	{
		int nValue = TextToInt( s, m_iValue, (m_nFlags & VF_BITFIELD)!=0 );

		Set(nValue);
	}
	virtual void Set( const float f )
	{ 
		Set((int)f);
	}
	virtual void Set( const int i )
	{ 
		if (i == m_iValue)
			return;

		if(m_nFlags & VF_READONLY)
			return;

		char sTemp[128];
		sprintf(sTemp,"%d",i);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue=i; 

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int GetType() { return CVAR_INT; }

protected: // --------------------------------------------------------------------------------------------

	int 							m_iValue;											//!<
};

//////////////////////////////////////////////////////////////////////////
class CXConsoleVariableFloat :public CXConsoleVariableBase
{
public:
	// constructor
	CXConsoleVariableFloat( CXConsole *pConsole, const char *sName, const float fDefault, int nFlags, const char *help )
		:CXConsoleVariableBase(pConsole,sName,nFlags,help), m_fValue(fDefault)
	{
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int GetIVal() const { return (int)m_fValue; }
	virtual float GetFVal() const { return m_fValue; }
	virtual const char *GetString() 
	{
		static char szReturnString[256];

		sprintf(szReturnString,"%g",m_fValue);		// %g -> "2.01",   %f -> "2.01000"
		return szReturnString;
	}
	virtual void Set( const char* s )
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);
    if (fValue == m_fValue)
			return;

		if(m_pConsole->OnBeforeVarChange(this,s))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = fValue;

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const float f )
	{
		if (f == m_fValue)
			return;

		char sTemp[128];
		sprintf(sTemp,"%g",f);		// %g -> "2.01",   %f -> "2.01000"

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue=f; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const int i )
	{ 
		if ((float)i == m_fValue)
			return;

		char sTemp[128];
		sprintf(sTemp,"%d",i);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue=(float)i; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int GetType() { return CVAR_FLOAT; }

private: // --------------------------------------------------------------------------------------------

	float 							m_fValue;											//!<
};


class CXConsoleVariableIntRef :public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableIntRef( CXConsole *pConsole, const char *sName, int32 *pVar, int nFlags, const char *help )
		:CXConsoleVariableBase(pConsole,sName,nFlags,help), m_iValue(*pVar)
	{
		assert(pVar);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int GetIVal() const { return m_iValue; }
	virtual float GetFVal() const { return (float)m_iValue; }
	virtual const char *GetString() 
	{
		static char szReturnString[256];

		sprintf(szReturnString,"%d",m_iValue);
		return szReturnString;
	}
	virtual void Set( const char* s )
	{
		int nValue = TextToInt( s, m_iValue, (m_nFlags & VF_BITFIELD)!=0 );
		if (nValue == m_iValue)
			return;

		if(m_pConsole->OnBeforeVarChange(this,s))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue=nValue;

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const float f )
	{ 
		if ((int)f == m_iValue)
			return;

		char sTemp[128];
		sprintf(sTemp,"%f",f);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue=(int)f; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const int i )
	{ 
		if (i == m_iValue)
			return;
		char sTemp[128];
		sprintf(sTemp,"%d",i);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue=i; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int GetType() { return CVAR_INT; }

private: // --------------------------------------------------------------------------------------------

	int &							m_iValue;											//!<
};





class CXConsoleVariableFloatRef :public CXConsoleVariableBase
{
public:
	//! constructor
	//!\param pVar must not be 0
	CXConsoleVariableFloatRef( CXConsole *pConsole, const char *sName, float *pVar, int nFlags, const char *help )
		:CXConsoleVariableBase(pConsole,sName,nFlags,help), m_fValue(*pVar)
	{
		assert(pVar);
	}

	// interface ICVar --------------------------------------------------------------------------------------

	virtual int GetIVal() const { return (int)m_fValue; }
	virtual float GetFVal() const { return m_fValue; }
	virtual const char *GetString() 
	{
		static char szReturnString[256];

		sprintf(szReturnString,"%g",m_fValue);
		return szReturnString;
	}
	virtual void Set( const char* s )
	{
		float fValue = 0;
		if (s)
			fValue = (float)atof(s);
		if (fValue == m_fValue)
			return;

		if(m_pConsole->OnBeforeVarChange(this,s))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue = fValue;

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const float f )
	{ 
		if (f == m_fValue)
			return;

		char sTemp[128];
		sprintf(sTemp,"%f",f);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue=f; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual void Set( const int i )
	{ 
		if ((float)i == m_fValue)
			return;

		char sTemp[128];
		sprintf(sTemp,"%d",i);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_fValue=(float)i; 
			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}
	virtual int GetType() { return CVAR_FLOAT; }

private: // --------------------------------------------------------------------------------------------

	float &							m_fValue;											//!<
};



// works like CXConsoleVariableInt but when changing it sets other console variables
// getting the value returns the last value it was set to - if that is still what was applied
// to the cvars can be tested with GetRealIVal()
class CXConsoleVariableCVarGroup :public CXConsoleVariableInt, public ILoadConfigurationEntrySink
{
public:
	// constructor
	CXConsoleVariableCVarGroup( CXConsole *pConsole, const char *sName, const char *szFileName, int nFlags );

	// destructor
	~CXConsoleVariableCVarGroup();

	// Returns:
	//   part of the help string - useful to log out detailed description without additional help text
	string GetDetailedInfo() const;

	// interface ICVar -----------------------------------------------------------------------------------

	virtual const char* GetHelp();

	virtual int GetRealIVal() const;

	virtual void DebugLog( const int iExpectedValue, const ICVar::EConsoleLogMode mode ) const;

	virtual void Set( const int i )
	{ 
		char sTemp[128];
		sprintf(sTemp,"%d",i);

		if(m_pConsole->OnBeforeVarChange(this,sTemp))
		{
			m_nFlags |= VF_MODIFIED;
			m_iValue=i; 

			if (m_pChangeFunc)
				m_pChangeFunc(this); // Call on change callback.
			m_pConsole->OnAfterVarChange(this);
		}
	}

	// ConsoleVarFunc ------------------------------------------------------------------------------------

	static void OnCVarChangeFunc( ICVar *pVar );

	// interface ILoadConfigurationEntrySink -------------------------------------------------------------

	virtual void OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup );
	virtual void OnLoadConfigurationEntry_End();

private: // --------------------------------------------------------------------------------------------

	struct SCVarGroup
	{
		std::map<string,string>							m_KeyValuePair;					// e.g. m_KeyValuePair["r_fullscreen"]="0"
	};

	SCVarGroup														m_CVarGroupDefault;
	std::map<int,SCVarGroup *>						m_CVarGroupStates;
	string																m_sDefaultValue;				// used by OnLoadConfigurationEntry_End()

	void ApplyCVars( const SCVarGroup &rGroup, const SCVarGroup *pExclude=0 );

	// Arguments:
	//   sKey - must exist, at least in default
	//   pSpec - can be 0
	string GetValueSpec( const string &sKey, const int *pSpec=0 ) const;

	// should only be used by TestCVars()
	// Returns:
	//   true=all console variables match the state (excluding default state), false otherwise
	bool _TestCVars( const SCVarGroup &rGroup, const ICVar::EConsoleLogMode mode, const SCVarGroup *pExclude=0 ) const;

	// Arguments:
	//   pGroup - can be 0 to test if the default state is set
	// Returns:
	//   true=all console variables match the state (including default state), false otherwise
	bool TestCVars( const SCVarGroup *pGroup, const ICVar::EConsoleLogMode mode=ICVar::eCLM_Off ) const;
};

#endif // !defined(AFX_XCONSOLEVARIABLE_H__AB510BA3_4D53_4C45_A2A0_EA15BABE0C34__INCLUDED_)
