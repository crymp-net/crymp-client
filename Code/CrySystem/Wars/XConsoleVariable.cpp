// XConsoleVariable.cpp: implementation of the CXConsoleVariable class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "XConsole.h"
#include "XConsoleVariable.h"

#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/ISystem.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CXConsoleVariableBase::CXConsoleVariableBase(CXConsole *pConsole, const char *sName, int nFlags, const char *help)
{
	assert(pConsole);

	m_psHelp = (char *)help;
	m_pChangeFunc = NULL;
	
	m_pConsole=pConsole;

	if (nFlags & VF_NOT_NET_SYNCED)
		gEnv->pLog->LogWarning("CVAR %s is declared not synced", sName);

	m_nFlags=nFlags;

	if(nFlags&VF_COPYNAME)
	{
		m_szName = new char[strlen(sName)+1];
		strcpy(m_szName,sName);
	}
	else m_szName=(char *)sName;
}


//////////////////////////////////////////////////////////////////////////
CXConsoleVariableBase::~CXConsoleVariableBase()
{
	if(m_nFlags&VF_COPYNAME)
		delete m_szName;
}

//////////////////////////////////////////////////////////////////////////
void CXConsoleVariableBase::ForceSet( const char* s )
{	
	bool bCheat=false;
	bool bReadOnly=false;
	if (m_nFlags & VF_READONLY)
	{
		m_nFlags&=~VF_READONLY;
		bReadOnly=true;
	}

	if (m_nFlags & VF_CHEAT)
	{
		m_nFlags&=~VF_CHEAT;
		bCheat=true;
	}

	Set(s);
		
	if (bReadOnly)
		m_nFlags|=VF_READONLY;
	if (bCheat)
		m_nFlags|=VF_CHEAT;
}



//////////////////////////////////////////////////////////////////////////
void CXConsoleVariableBase::ClearFlags (int flags)
{
	m_nFlags&=~flags;
}

//////////////////////////////////////////////////////////////////////////
int CXConsoleVariableBase::GetFlags()
{
	return m_nFlags;
}

//////////////////////////////////////////////////////////////////////////
int CXConsoleVariableBase::SetFlags( int flags )
{
	m_nFlags = flags;
	return m_nFlags ;
}

//////////////////////////////////////////////////////////////////////////
const char* CXConsoleVariableBase::GetName() const
{
	return m_szName;
}

//////////////////////////////////////////////////////////////////////////
const char* CXConsoleVariableBase::GetHelp()
{
	return m_psHelp;
}

void CXConsoleVariableBase::Release()
{
	m_pConsole->UnregisterVariable(m_szName);
	delete this;
}

void CXConsoleVariableBase::GetMemoryUsage( class ICrySizer* pSizer )
{
	pSizer->Add (*this);
}

void CXConsoleVariableBase::SetOnChangeCallback( ConsoleVarFunc pChangeFunc )
{
	m_pChangeFunc = pChangeFunc;
}


void CXConsoleVariableCVarGroup::OnLoadConfigurationEntry( const char *szKey, const char *szValue, const char *szGroup )
{
	assert(szGroup);
	assert(szKey);
	assert(szValue);

	bool bCheckIfInDefault=false;

	SCVarGroup *pGrp=0;

	if(stricmp(szGroup,"default")==0)				// needs to be before the other groups
	{
		pGrp = &m_CVarGroupDefault;

//		if(stricmp(GetName(),szKey)==0)
		if(*szKey==0)
		{
			m_sDefaultValue = szValue;
			int iGrpValue = atoi(szValue);

			// if default state is not part of the mentioned states generate this state, so GetIRealVal() can return this state as well
			if(m_CVarGroupStates.find(iGrpValue)==m_CVarGroupStates.end())
				m_CVarGroupStates[iGrpValue] = new SCVarGroup;

			return;
		}
	}
	else
	{
		int iGrp;

		if(sscanf(szGroup,"%d",&iGrp)==1)
		{
			if(m_CVarGroupStates.find(iGrp)==m_CVarGroupStates.end())
				m_CVarGroupStates[iGrp] = new SCVarGroup;

			pGrp = m_CVarGroupStates[iGrp];
			bCheckIfInDefault=true;
		}
		else
		{
			gEnv->pLog->LogError("Error: ConsoleVariableGroup [%s] is no valid group",szGroup);
			assert(0);
			return;
		}

		if(*szKey==0)
		{
			assert(0);			// =%d only expected in default section
			return;
		}
	}


	if(pGrp)
	{
		if(pGrp->m_KeyValuePair.find(szKey)!=pGrp->m_KeyValuePair.end())
		{
			gEnv->pLog->LogError("Error: ConsoleVariableGroup '%s' key '%s' specified mutliple times",GetName(),szKey);
			assert(0);
		}

		pGrp->m_KeyValuePair[szKey]=szValue;

		if(bCheckIfInDefault)
		if(m_CVarGroupDefault.m_KeyValuePair.find(szKey)==m_CVarGroupDefault.m_KeyValuePair.end())
		{
			gEnv->pLog->LogError("Error: ConsoleVariableGroup '%s' key '%s' is not missing in default",GetName(),szKey);
			assert(0);
		}
	}
}


void CXConsoleVariableCVarGroup::OnLoadConfigurationEntry_End()
{
	if(!m_sDefaultValue.empty())
	{
		gEnv->pConsole->LoadConfigVar(GetName(),m_sDefaultValue.c_str());
		m_sDefaultValue.clear();
	}
}


CXConsoleVariableCVarGroup::CXConsoleVariableCVarGroup( CXConsole *pConsole, const char *sName, const char *szFileName, int nFlags )
	:CXConsoleVariableInt(pConsole,sName,0,nFlags,0)
{
	gEnv->pSystem->LoadConfiguration(szFileName,this);
}


string CXConsoleVariableCVarGroup::GetDetailedInfo() const
{
	string sRet = GetName();

	sRet +=	" [";

	{
		std::map<int,SCVarGroup *>::const_iterator it, end=m_CVarGroupStates.end();

		for(it=m_CVarGroupStates.begin();it!=end;++it)
		{
			if(it!=m_CVarGroupStates.begin())
				sRet += "/";

			char szNum[10];

			sprintf(szNum,"%d",it->first);

			sRet += szNum;
		}
	}

	sRet +=	"/x]:\n";


	std::map<string,string>::const_iterator it, end=m_CVarGroupDefault.m_KeyValuePair.end();

	for(it=m_CVarGroupDefault.m_KeyValuePair.begin();it!=end;++it)
	{
		const string &rKey = it->first;

		sRet += " ... ";
		sRet += rKey;
		sRet += " = ";
		
		std::map<int,SCVarGroup *>::const_iterator it2, end2=m_CVarGroupStates.end();

		for(it2=m_CVarGroupStates.begin();it2!=end2;++it2)
		{
			SCVarGroup *pGrp = it2->second;

			sRet += GetValueSpec(rKey,&(it2->first));
			sRet += "/";
		}
		sRet += GetValueSpec(rKey);

		sRet += "\n";
	}

	return sRet;
}



const char *CXConsoleVariableCVarGroup::GetHelp()
{
	if(m_psHelp)
		return m_psHelp;

	// create help on demand
	string sRet = "Console variable group to apply settings to multiple variables\n\n";

	sRet += GetDetailedInfo();

	m_psHelp = new char[sRet.size()+1];
	strcpy(m_psHelp,&sRet[0]);

	return m_psHelp;
}



void CXConsoleVariableCVarGroup::DebugLog( const int iExpectedValue, const ICVar::EConsoleLogMode mode ) const 
{
	std::map<int,SCVarGroup *>::const_iterator it, end = m_CVarGroupStates.end();

	SCVarGroup *pCurrentGrp = 0;
	{
		std::map<int,SCVarGroup *>::const_iterator itCurrentGrp = m_CVarGroupStates.find(iExpectedValue);

		if(itCurrentGrp!=end)
			pCurrentGrp=itCurrentGrp->second;
	}

	// try the current state
	if(TestCVars(pCurrentGrp,mode))
		return;
}


int CXConsoleVariableCVarGroup::GetRealIVal() const 
{
	std::map<int,SCVarGroup *>::const_iterator it, end = m_CVarGroupStates.end();

	int iValue = GetIVal();

	SCVarGroup *pCurrentGrp = 0;
	{
		std::map<int,SCVarGroup *>::const_iterator itCurrentGrp = m_CVarGroupStates.find(iValue);

		if(itCurrentGrp!=end)
			pCurrentGrp=itCurrentGrp->second;
	}

	// first try the current state
	if(TestCVars(pCurrentGrp))
		return iValue;

	// then all other
	for(it=m_CVarGroupStates.begin();it!=end;++it)
	{
		SCVarGroup *pLocalGrp = it->second;
		
		if(pLocalGrp==pCurrentGrp)
			continue;

		int iLocalState = it->first;

		if(TestCVars(pLocalGrp))
			return iLocalState;
	}

	return -1;		// no state found that represent the current one
}


CXConsoleVariableCVarGroup::~CXConsoleVariableCVarGroup()
{
	std::map<int,SCVarGroup *>::iterator it, end=m_CVarGroupStates.end();

	for(it=m_CVarGroupStates.begin();it==end;++it)
	{
		SCVarGroup *pGrp = it->second;

		delete pGrp;
	}

	delete m_psHelp;
}


void CXConsoleVariableCVarGroup::OnCVarChangeFunc( ICVar *pVar )
{
	CXConsoleVariableCVarGroup *pThis = (CXConsoleVariableCVarGroup *)pVar;

	int iValue = pThis->GetIVal();

	// all sys_spec_* should be clamped by the max available spec 
	if(strnicmp(pThis->GetName(),"sys_spec",8)==0)
	{
		int iMaxSpec =gEnv->pSystem->GetMaxConfigSpec();

		if(iValue>iMaxSpec)
		{
			iValue=iMaxSpec;
			pThis->m_iValue=iValue;
		}
	}

	std::map<int,SCVarGroup *>::const_iterator itGrp = pThis->m_CVarGroupStates.find(iValue);

	SCVarGroup *pGrp=0;

	if(itGrp!=pThis->m_CVarGroupStates.end())
		pGrp = itGrp->second;

	if(pGrp)
		pThis->ApplyCVars(*pGrp);

	pThis->ApplyCVars(pThis->m_CVarGroupDefault,pGrp);
}


bool CXConsoleVariableCVarGroup::TestCVars( const SCVarGroup *pGroup, const ICVar::EConsoleLogMode mode ) const
{
	if(pGroup)
	if(!_TestCVars(*pGroup,mode))
		return false;

	if(!_TestCVars(m_CVarGroupDefault,mode,pGroup))
		return false;

	return true;
}

bool CXConsoleVariableCVarGroup::_TestCVars( const SCVarGroup &rGroup, const ICVar::EConsoleLogMode mode, const SCVarGroup *pExclude ) const
{
	bool bRet=true;
	std::map<string,string>::const_iterator it, end=rGroup.m_KeyValuePair.end();

	for(it=rGroup.m_KeyValuePair.begin();it!=end;++it)
	{
		const string &rKey = it->first;
		const string &rValue = it->second;

		if(pExclude)
		if(pExclude->m_KeyValuePair.find(rKey)!=pExclude->m_KeyValuePair.end())
			continue;

		ICVar *pVar = gEnv->pConsole->GetCVar(rKey.c_str());

		if(pVar)
		{
			bool bOk=true;

			// compare exact type, 
			// simple string comparison would fail on some comparisons e.g. 2.0 == 2
			// and GetString() for int and float return pointer to shared array so this
			// can cause problems
			switch(pVar->GetType())
			{
				case CVAR_INT:
					{
						int iVal;
						if(sscanf(rValue.c_str(),"%d",&iVal)==1)
						if(pVar->GetIVal()!=atoi(rValue.c_str()))
							{ bOk=false;break; }

						if(pVar->GetIVal()!=pVar->GetRealIVal())
							{ bOk=false;break; }
					}
					break;
				case CVAR_FLOAT:
					{
						float fVal;
						if(sscanf(rValue.c_str(),"%f",&fVal)==1)
						if(pVar->GetFVal()!=fVal)
							{ bOk=false;break; }
					}
					break;
				case CVAR_STRING:
					if(rValue!=pVar->GetString())
						{ bOk=false;break; }
					break;
				default: assert(0);
			}

			if(!bOk)
			{
				if(mode==ICVar::eCLM_Off)
					return false;		// exit as early as possible

				bRet=false;				// exit with same return code but log all differences

				if(strcmp(pVar->GetString(),rValue.c_str())!=0)
				{
					switch(mode)
					{
						case ICVar::eCLM_ConsoleAndFile: 
							CryLog("     $3%s = $6%s $4expected:%s",rKey.c_str(),pVar->GetString(),rValue.c_str());break;
						
						case ICVar::eCLM_FileOnly:
						case ICVar::eCLM_FullInfo: 
							gEnv->pLog->LogToFile("     %s = %s expected:%s",rKey.c_str(),pVar->GetString(),rValue.c_str());break;
						
						default: assert(0);
					}
				}
				else if(mode==ICVar::eCLM_FullInfo)
					gEnv->pLog->LogToFile("     %s = Custom expected:%s",rKey.c_str(),rValue.c_str());

				pVar->DebugLog(pVar->GetIVal(),mode);		// recursion
			}

			if(pVar->GetFlags()&VF_CHEAT)
			{
				// either VF_CHEAT should be removed or the var should be not part of the CVarGroup
				gEnv->pLog->LogError("Error: ConsoleVariableGroup '%s' key '%s' is cheat protected",GetName(),rKey.c_str());
				assert(0);
			}

		}
		else
		{
      if(gEnv->pSystem && gEnv->pSystem->GetISoundSystem() && gEnv->pSystem->GetIGame())
			  gEnv->pLog->LogError("Error: ConsoleVariableGroup '%s' key '%s' is no registered console variable",GetName(),rKey.c_str());
			// some of these are client-only variables, so only fail assertion if not dedicated server.
			//assert(0);			// CVar in group but does not exist
			assert(gEnv->pSystem->IsDedicated());			// CVar in group but does not exist
		}
	}

	return bRet;
}



string CXConsoleVariableCVarGroup::GetValueSpec( const string &sKey, const int *pSpec ) const
{
	if(pSpec)
	{
		std::map<int,SCVarGroup *>::const_iterator itGrp = m_CVarGroupStates.find(*pSpec);

		if(itGrp!=m_CVarGroupStates.end())
		{
			const SCVarGroup *pGrp = itGrp->second;

			// check in spec
			std::map<string,string>::const_iterator it = pGrp->m_KeyValuePair.find(sKey);

			if(it!=pGrp->m_KeyValuePair.end())
				return it->second;
		}
	}

	// check in default
	std::map<string,string>::const_iterator it = m_CVarGroupDefault.m_KeyValuePair.find(sKey);

	if(it!=m_CVarGroupDefault.m_KeyValuePair.end())
		return it->second;

	assert(0);		// internal error
	return "";
}

void CXConsoleVariableCVarGroup::ApplyCVars( const SCVarGroup &rGroup, const SCVarGroup *pExclude )
{
	std::map<string,string>::const_iterator it, end=rGroup.m_KeyValuePair.end();

	for(it=rGroup.m_KeyValuePair.begin();it!=end;++it)
	{
		const string &rKey = it->first;

		if(pExclude)
		if(pExclude->m_KeyValuePair.find(rKey)!=pExclude->m_KeyValuePair.end())
			continue;

//		gEnv->pLog->LogWithType(IMiniLog::eInputResponse,"CVarGroup(%s=%d) %s=%s",GetName(),GetIVal(),rKey.c_str(),it->second.c_str());		// can be deactivated once the system proved itself

		gEnv->pConsole->LoadConfigVar(rKey.c_str(),it->second.c_str());
	}
}

