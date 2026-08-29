////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2001-2004.
// -------------------------------------------------------------------------
//  File name:   HTTPDownloader.h
//  Version:     v1.00
//  Created:     24/9/2004.
//  Compilers:   Visual Studio.NET 2003
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __HTTPDownloader_h__
#define __HTTPDownloader_h__
#pragma once

#pragma once


#if defined(WIN32)

#include <wininet.h>
#include "CryCommon/CryScriptSystem/IScriptSystem.h"
#include <dbghelp.h>


#define HTTP_BUFFER_SIZE		(16384)


enum
{
	HTTP_STATE_WORKING = 0,
	HTTP_STATE_COMPLETE,
	HTTP_STATE_CANCELED,
	HTTP_STATE_ERROR,
	HTTP_STATE_NONE,
};


class CDownloadManager;


class CHTTPDownloader // : public _ScriptableEx<CHTTPDownloader>
{
public:
	CHTTPDownloader();
	virtual ~CHTTPDownloader();

	int Create(ISystem *pISystem, CDownloadManager *pParent);
	int	Download(const char *szURL, const char *szDestination);
	void Cancel();
	int GetState() { return m_iState; };
	int	GetFileSize() const { return m_iFileSize; };
	const string& GetURL() const { return m_szURL; };
	const string& GetDstFileName() const { return m_szDstFile; };
	void	Release();

	int Download(IFunctionHandler *pH);
	int Cancel(IFunctionHandler *pH);
	int Release(IFunctionHandler *pH);

	int GetURL(IFunctionHandler *pH);
	int GetFileSize(IFunctionHandler *pH);
	int GetFileName(IFunctionHandler *pH);

	void OnError();
	void OnComplete();
	void OnCancel();

private:

	static	DWORD DownloadProc(CHTTPDownloader *_this);
	void	CreateThread();
	DWORD DoDownload();
	void	PrepareBuffer();
	IScriptTable* GetScriptObject() { return 0; };

	string						m_szURL;
	string						m_szDstFile;
	HANDLE						m_hThread;
	HINTERNET					m_hINET;
	HINTERNET					m_hUrl;

	unsigned char			*m_pBuffer;
	int								m_iFileSize;

	volatile int			m_iState;
	volatile bool			m_bContinue;

	ISystem						*m_pSystem;
	CDownloadManager	*m_pParent;

	IScriptSystem *m_pScriptSystem;
};

#endif //LINUX

#endif // __HTTPDownloader_h__