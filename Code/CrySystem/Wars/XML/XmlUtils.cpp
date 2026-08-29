////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   XmlUtils.h
//  Created:     21/04/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>
#include "CryCommon/CrySystem/IXml.h"
#include "xml.h"
#include "XmlUtils.h"
#include "ReadWriteXMLSink.h"
#include "../DataProbe.h"

#include "../SimpleStringPool.h"
#include "SerializeXMLReader.h"
#include "SerializeXMLWriter.h"

//////////////////////////////////////////////////////////////////////////
CXmlNode_PoolAlloc *g_pCXmlNode_PoolAlloc = 0;
#ifdef CRY_COLLECT_XML_NODE_STATS
SXmlNodeStats* g_pCXmlNode_Stats = 0;
#endif

//////////////////////////////////////////////////////////////////////////
CXmlUtils::CXmlUtils( ISystem *pSystem )
{
	m_pSystem = pSystem;
	m_pSystem->GetISystemEventDispatcher()->RegisterListener(this);

	// create IReadWriteXMLSink object
	m_pReadWriteXMLSink = new CReadWriteXMLSink();
	g_pCXmlNode_PoolAlloc = new CXmlNode_PoolAlloc;
#ifdef CRY_COLLECT_XML_NODE_STATS
	g_pCXmlNode_Stats = new SXmlNodeStats();
#endif
}

//////////////////////////////////////////////////////////////////////////
CXmlUtils::~CXmlUtils()
{
	m_pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	delete g_pCXmlNode_PoolAlloc;
#ifdef CRY_COLLECT_XML_NODE_STATS
	delete g_pCXmlNode_Stats;
#endif
}

//////////////////////////////////////////////////////////////////////////
IXmlParser* CXmlUtils::CreateXmlParser()
{
	return new XmlParser;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CXmlUtils::LoadXmlFile( const char *sFilename )
{
	XmlParser parser;
	XmlNodeRef node = parser.parse( sFilename );

	const char* pErrorString = parser.getErrorString();
	if (strcmp("", pErrorString) != 0)
	{
		CryLog("Error parsing <%s>: %s", sFilename, parser.getErrorString());
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CXmlUtils::LoadXmlFromString( const char *sXmlString )
{
	XmlParser parser;
	XmlNodeRef node = parser.parseBuffer( sXmlString );
	return node;
}

//////////////////////////////////////////////////////////////////////////
const char* CXmlUtils::HashXml( XmlNodeRef node )
{
	static char signature[16*2 + 1];
	static char temp[16];
	static const char * hex = "0123456789abcdef";
	XmlString str = node->getXML();
	CDataProbe dp;
	dp.GetMD5( str.data(), str.length(), temp );
	for (int i=0; i<16; i++)
	{
		signature[2*i+0] = hex[((uint8)temp[i])>>4];
		signature[2*i+1] = hex[((uint8)temp[i])&0xf];
	}
	signature[16*2] = 0;
	return signature;
}

//////////////////////////////////////////////////////////////////////////
IReadWriteXMLSink* CXmlUtils::GetIReadWriteXMLSink()
{
	return m_pReadWriteXMLSink;
}

//////////////////////////////////////////////////////////////////////////
class CXmlSerializer : public IXmlSerializer
{
public:
	CXmlSerializer()
	{ 
		m_nRefCount = 0;
		m_pReaderImpl = 0;
		m_pReaderSer = 0;
		m_pWriterSer = 0;
		m_pWriterImpl = 0;
	}
	~CXmlSerializer()
	{
		ClearAll();
	}
	void ClearAll()
	{
		SAFE_DELETE(m_pReaderSer);
		SAFE_DELETE(m_pReaderImpl);
		SAFE_DELETE(m_pWriterSer);
		SAFE_DELETE(m_pWriterImpl);
	}
	
	//////////////////////////////////////////////////////////////////////////
	virtual void AddRef() { ++m_nRefCount; }
	virtual void Release() { if (--m_nRefCount <= 0) delete this; }

	virtual ISerialize* GetWriter( XmlNodeRef &node )
	{
		ClearAll();
		m_pWriterImpl = new CSerializeXMLWriterImpl(node);
		m_pWriterSer = new CSimpleSerializeWithDefaults<CSerializeXMLWriterImpl>( *m_pWriterImpl );
		return m_pWriterSer;
	}
	virtual ISerialize* GetReader( XmlNodeRef &node )
	{
		ClearAll();
		m_pReaderImpl = new CSerializeXMLReaderImpl(node);
		m_pReaderSer = new CSimpleSerializeWithDefaults<CSerializeXMLReaderImpl>( *m_pReaderImpl );
		return m_pReaderSer;
	}
	//////////////////////////////////////////////////////////////////////////
private:
	int m_nRefCount;
	CSerializeXMLReaderImpl *m_pReaderImpl;
	CSimpleSerializeWithDefaults<CSerializeXMLReaderImpl> *m_pReaderSer;

	CSerializeXMLWriterImpl *m_pWriterImpl;
	CSimpleSerializeWithDefaults<CSerializeXMLWriterImpl> *m_pWriterSer;
};

//////////////////////////////////////////////////////////////////////////
IXmlSerializer* CXmlUtils::CreateXmlSerializer()
{
	return new CXmlSerializer;
}

//////////////////////////////////////////////////////////////////////////
void CXmlUtils::GetMemoryUsage( ICrySizer *pSizer )
{
	{
		SIZER_COMPONENT_NAME(pSizer, "Nodes");
		pSizer->AddObject( g_pCXmlNode_PoolAlloc,g_pCXmlNode_PoolAlloc->GetTotalAllocatedMemory() );
	}
	{
		SIZER_COMPONENT_NAME(pSizer, "StringPool");
		pSizer->AddObject( &CSimpleStringPool::g_nTotalAllocInXmlStringPools,CSimpleStringPool::g_nTotalAllocInXmlStringPools );
	}

#ifdef CRY_COLLECT_XML_NODE_STATS
	// yes, slow
	std::vector<const CXmlNode*> rootNodes;
	{
		TXmlNodeSet::const_iterator iter = g_pCXmlNode_Stats->nodeSet.begin();
		TXmlNodeSet::const_iterator iterEnd = g_pCXmlNode_Stats->nodeSet.end();
		while (iter != iterEnd)
		{
			const CXmlNode* pNode = *iter;
			if (pNode->getParent() == 0)
			{
				rootNodes.push_back(pNode);
			}
			++iter;
		}
	}

	// use the following to log to console
#if 0
	CryLogAlways("NumXMLRootNodes=%d NumXMLNodes=%d TotalAllocs=%d TotalFrees=%d", 
		rootNodes.size(), g_pCXmlNode_Stats->nodeSet.size(),
		g_pCXmlNode_Stats->nAllocs, g_pCXmlNode_Stats->nFrees);
#endif


	// use the following to debug the nodes in the system 
#if 0
	{
		std::vector<const CXmlNode*>::const_iterator iter = rootNodes.begin();
		std::vector<const CXmlNode*>::const_iterator iterEnd = rootNodes.end();
		while (iter != iterEnd)
		{
			const CXmlNode* pNode = *iter;
			CryLogAlways("Node 0x%p Tag='%s'", pNode, pNode->getTag());
			++iter;
		}
	}
#endif

	// only for debugging, add it as pseudo numbers to the CrySizer.
	// shift it by 10, so we get the actual number
	{
		SIZER_COMPONENT_NAME(pSizer, "#NumTotalNodes");
		pSizer->Add("#NumTotalNodes", g_pCXmlNode_Stats->nodeSet.size()<<10);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "#NumRootNodes");
		pSizer->Add("#NumRootNodes", rootNodes.size()<<10);
	}
#endif

}

//////////////////////////////////////////////////////////////////////////
void CXmlUtils::OnSystemEvent( ESystemEvent event,UINT_PTR wparam,UINT_PTR lparam )
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_RELOAD:
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		//
		if (g_pCXmlNode_PoolAlloc->GetTotalAllocatedNodeSize() == 0)
			g_pCXmlNode_PoolAlloc->FreeMemory();
		// STLALLOCATOR_CLEANUP was a no-op in the Wars CryCommon memory manager.
		break;
	}
}
