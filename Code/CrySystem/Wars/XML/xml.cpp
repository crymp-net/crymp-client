////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   xml.cpp
//  Created:     21/04/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

//#define _CRT_SECURE_NO_DEPRECATE 1
//#define _CRT_NONSTDC_NO_DEPRECATE
#include <stdlib.h>

#define XMLPARSEAPI(type) type
#include "Expat/expat.h"
#include "xml.h"
#include <algorithm>
#include <stdio.h>
#include "CryCommon/CrySystem/ICryPak.h"

#define FLOAT_FMT	"%.8g"

#include "../SimpleStringPool.h"

//////////////////////////////////////////////////////////////////////////
static int __cdecl ascii_stricmp( const char * dst, const char * src )
{
	int f, l;
	do
	{
		if ( ((f = (unsigned char)(*(dst++))) >= 'A') && (f <= 'Z') )
			f -= 'A' - 'a';
		if ( ((l = (unsigned char)(*(src++))) >= 'A') && (l <= 'Z') )
			l -= 'A' - 'a';
	}
	while ( f && (f == l) );
	return(f - l);
}

//////////////////////////////////////////////////////////////////////////
XmlStrCmpFunc g_pXmlStrCmp = &ascii_stricmp;

// Global counter for memory allocated in XML string pools.
size_t CSimpleStringPool::g_nTotalAllocInXmlStringPools = 0;

//////////////////////////////////////////////////////////////////////////
class CXmlStringData : public IXmlStringData
{
public:
	int m_nRefCount;
	XmlString m_string;

	CXmlStringData() { m_nRefCount = 0; }
	virtual void AddRef() { ++m_nRefCount; }
	virtual void Release() { if (--m_nRefCount <= 0) delete this; }

	virtual const char* GetString() { return m_string.c_str(); };
	virtual size_t GetStringLength() { return m_string.size(); };
};

//////////////////////////////////////////////////////////////////////////
class CXmlStringPool : public IXmlStringPool
{
public:
	char* AddString( const char *str ) { return m_stringPool.Append( str,(int)strlen(str) ); }
private:
	CSimpleStringPool m_stringPool;
};

//xml_node_allocator XmlDynArrayAlloc::allocator;
//size_t XmlDynArrayAlloc::m_iAllocated = 0;
/**
******************************************************************************
* CXmlNode implementation.
******************************************************************************
*/

CXmlNode::~CXmlNode()
{
	// Clear parent pointer from childs.
	for (int i = 0,num = m_childs.size(); i < num; ++i)
	{
		m_childs[i]->m_parent = 0;
		m_childs[i]->Release();
	}
	m_pStringPool->Release();
}

CXmlNode::CXmlNode()
{
	m_tag = "";
	m_content = "";
	m_parent = 0;
	m_nRefCount = 0;
	m_pStringPool = 0; // must be changed later.
}

CXmlNode::CXmlNode( const char *tag )
{
	m_content = "";
	m_parent = 0;
	m_nRefCount = 0;
	m_pStringPool = new CXmlStringPool;
	m_pStringPool->AddRef();
	m_tag = m_pStringPool->AddString(tag);
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CXmlNode::createNode( const char *tag )
{
	CXmlNode *pNewNode = new CXmlNode;
	pNewNode->m_pStringPool = m_pStringPool;
	m_pStringPool->AddRef();
	pNewNode->m_tag = m_pStringPool->AddString(tag);
	return XmlNodeRef( pNewNode );
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::setTag( const char *tag )
{
	m_tag = m_pStringPool->AddString(tag);
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::setContent( const char *str )
{
	m_content = m_pStringPool->AddString(str);
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::isTag( const char *tag ) const
{
	return g_pXmlStrCmp( tag,m_tag ) == 0;
}

const char* CXmlNode::getAttr( const char *key ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
		return svalue;
	return "";
}

bool CXmlNode::haveAttr( const char *key ) const
{
	XmlAttrConstIter it = GetAttrConstIterator(key);
	if (it != m_attributes.end()) {
		return true;
	}
	return false;
}

void CXmlNode::delAttr( const char *key )
{
	XmlAttrIter it = GetAttrIterator(key);
	if (it != m_attributes.end())
	{
		m_attributes.erase( it );
	}
}

void CXmlNode::removeAllAttributes()
{
	m_attributes.clear();
}

void CXmlNode::setAttr( const char *key,const char *value )
{
	XmlAttrIter it = GetAttrIterator(key);
	if (it == m_attributes.end())
	{
		XmlAttribute tempAttr;
		tempAttr.key = m_pStringPool->AddString(key);
		tempAttr.value = m_pStringPool->AddString(value);
		m_attributes.push_back( tempAttr );
		// Sort attributes.
		//std::sort( m_attributes.begin(),m_attributes.end() );
	}
	else
	{
		// If already exist, ovveride this member.
		it->value = m_pStringPool->AddString(value);
	}
}

void CXmlNode::setAttr( const char *key,int value )
{
	char str[128];
	itoa( value,str,10 );
	setAttr( key,str );
}

void CXmlNode::setAttr( const char *key,unsigned int value )
{
	char str[128];
	itoa( value,str,10 );
	setAttr( key,str );
}

void CXmlNode::setAttr( const char *key,float value )
{
	char str[128];
	sprintf( str,FLOAT_FMT,value );
	setAttr( key,str );
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::setAttr( const char *key,int64 value )
{
	char str[32];
#if defined(__GNUC__)
	sprintf( str,"%lld",(unsigned long long)value );
#else
	sprintf( str,"%I64d",value );
#endif
	setAttr( key,str );
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::setAttr( const char *key,uint64 value )
{
	char str[32];
#if defined(__GNUC__)
	sprintf( str,"%llX",(unsigned long long)value );
#else
	sprintf( str,"%I64X",value );
#endif
	setAttr( key,str );
}

void CXmlNode::setAttr( const char *key,const Ang3& value )
{
	char str[128];
	sprintf(str, FLOAT_FMT "," FLOAT_FMT "," FLOAT_FMT, value.x, value.y, value.z);
	setAttr( key,str );
}
void CXmlNode::setAttr( const char *key,const Vec3& value )
{
	char str[128];
	sprintf(str, FLOAT_FMT "," FLOAT_FMT "," FLOAT_FMT, value.x, value.y, value.z);
	setAttr( key,str );
}
void CXmlNode::setAttr( const char *key,const Vec2& value )
{
	char str[128];
	sprintf(str, FLOAT_FMT "," FLOAT_FMT, value.x, value.y);
	setAttr( key,str );
}

void CXmlNode::setAttr( const char *key,const Quat &value )
{
	char str[128];
	sprintf(str, FLOAT_FMT "," FLOAT_FMT "," FLOAT_FMT "," FLOAT_FMT, value.w, value.v.x, value.v.y, value.v.z);
	setAttr( key,str );
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,int &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		value = atoi(svalue);
		return true;
	}
	return false;
}

bool CXmlNode::getAttr( const char *key,unsigned int &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		value = atoi(svalue);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,int64 &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
#if defined(_MSC_VER)
		sscanf( svalue,"%I64d",&value );
#elif defined(__GNUC__)
		unsigned long long valueULL;
		if (sscanf ( svalue,"%lld",&valueULL ) == 1)
			value = (uint64)valueULL;
#else
#error
#endif
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,uint64 &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
#if defined(_MSC_VER)
		sscanf( svalue,"%I64X",&value );
#elif defined(__GNUC__)
		unsigned long long valueULL;
		if (sscanf ( svalue,"%llX",&valueULL ) == 1)
			value = (uint64)valueULL;
#else
#error
#endif
		return true;
	}
	return false;
}

bool CXmlNode::getAttr( const char *key,bool &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		value = atoi(svalue)!=0;
		return true;
	}
	return false;
}

bool CXmlNode::getAttr( const char *key,float &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		value = (float)atof(svalue);
		return true;
	}
	return false;
}

bool CXmlNode::getAttr( const char *key,Ang3& value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		float x,y,z;
		if (sscanf( svalue,"%f,%f,%f",&x,&y,&z ) == 3)
		{
			value(x,y,z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,Vec3& value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		float x,y,z;
		if (sscanf( svalue,"%f,%f,%f",&x,&y,&z ) == 3)
		{
			value(x,y,z);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,Vec2& value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		float x,y;
		if (sscanf( svalue,"%f,%f",&x,&y ) == 3)
		{
			value = Vec2(x,y);
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttr( const char *key,Quat &value ) const
{
	const char *svalue = GetValue(key);
	if (svalue)
	{
		float w,x,y,z;
		if (sscanf( svalue,"%f,%f,%f,%f",&w,&x,&y,&z ) == 4)
		{
			value = Quat(w,x,y,z);
			return true;
		}
	}
	return false;
}


XmlNodeRef CXmlNode::findChild( const char *tag ) const
{
	for (int i = 0,num = m_childs.size(); i < num; ++i)
	{
		if (m_childs[i]->isTag(tag))
			return m_childs[i];
	}
	return 0;
}

void CXmlNode::removeChild( const XmlNodeRef &node )
{
	XmlNodes::iterator it = std::find(m_childs.begin(),m_childs.end(),(IXmlNode*)node );
	if (it != m_childs.end())
	{
		(*it)->m_parent = 0;
		(*it)->Release();
		m_childs.erase(it);
	}
}

void CXmlNode::removeAllChilds()
{
	// Clear parent pointer from childs.
	for (int i = 0,num = m_childs.size(); i < num; ++i)
	{
		m_childs[i]->m_parent = 0;
		m_childs[i]->Release();
	}
	m_childs.clear();
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::deleteChild( const char *tag )
{
	for (int i = 0,num = m_childs.size(); i < num; ++i)
	{
		if (m_childs[i]->isTag(tag))
		{
			m_childs[i]->m_parent = 0;
			m_childs[i]->Release();
			m_childs.erase( m_childs.begin()+i );
			return;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::deleteChildAt( int nIndex )
{
	if (nIndex >= 0 && nIndex < (int)m_childs.size())
	{
		m_childs[nIndex]->m_parent = 0;
		m_childs[nIndex]->Release();
		m_childs.erase( m_childs.begin()+nIndex );
	}
}

//! Adds new child node.
void CXmlNode::addChild( const XmlNodeRef &node )
{
	assert( node != 0 );
	CXmlNode *pNode = (CXmlNode*)((IXmlNode*)node);
	pNode->AddRef();
	m_childs.push_back(pNode);
	pNode->m_parent = this;
};

XmlNodeRef CXmlNode::newChild( const char *tagName )
{
	XmlNodeRef node = createNode(tagName);
	addChild(node);
	return node;
}

//! Get XML Node child nodes.
XmlNodeRef CXmlNode::getChild( int i ) const
{
	assert( i >= 0 && i < (int)m_childs.size() );
	return m_childs[i];
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::copyAttributes( XmlNodeRef fromNode )
{
	IXmlNode* inode = fromNode;
	CXmlNode *n = (CXmlNode*)inode;
	if (n->m_pStringPool == m_pStringPool)
		m_attributes = n->m_attributes;
	else
	{
		m_attributes.resize( n->m_attributes.size() );
		for (int i = 0; i < (int)n->m_attributes.size(); i++)
		{
			m_attributes[i].key = m_pStringPool->AddString( n->m_attributes[i].key );
			m_attributes[i].value = m_pStringPool->AddString( n->m_attributes[i].value );
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::getAttributeByIndex( int index,const char **key,const char **value )
{
	XmlAttributes::iterator it = m_attributes.begin();
	if (it != m_attributes.end())
	{
		std::advance( it,index );
		if (it != m_attributes.end())
		{
			*key = it->key;
			*value = it->value;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CXmlNode::clone()
{
	CXmlNode *node = new CXmlNode;
	node->m_pStringPool = m_pStringPool;
	m_pStringPool->AddRef();
	node->m_tag = m_tag;
	node->m_content = m_content;
	// Clone attributes.
	CXmlNode *n = (CXmlNode*)(IXmlNode*)node;
	n->copyAttributes( this );
	// Clone sub nodes.
	node->m_childs.reserve( m_childs.size() );
	for (int i = 0,num = m_childs.size(); i < num; ++i)
	{
		node->addChild( m_childs[i]->clone() );
	}
	
	return node;
}

//////////////////////////////////////////////////////////////////////////
static void AddTabsToString( XmlString &xml,int level )
{
	static const char *tabs[] = {
		"",
		" ",
		"  ",
		"   ",
		"    ",
		"     ",
		"      ",
		"       ",
		"        ",
		"         ",
		"          ",
		"           ",
	};
	// Add tabs.
	if (level < sizeof(tabs)/sizeof(tabs[0]))
	{
		xml += tabs[level];
	}
	else
	{
		for (int i = 0; i < level; i++)
			xml += "  ";
	}
}

//////////////////////////////////////////////////////////////////////////
bool CXmlNode::IsValidXmlString( const char *str ) const
{
	int len = strlen(str);

	{
		// Prevents invalid characters not from standard ASCII set to propagate to xml.
		// A bit of hack for efficiency, fixes input string in place.
		char *s = const_cast<char*>(str);
		for (int i = 0; i < len; i++)
		{
			if ((unsigned char)s[i] > 0x7F)
				s[i] = ' ';
		}
	}
	
	if (strcspn(str,"\"\'&><") == len)
	{
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
XmlString CXmlNode::MakeValidXmlString( const XmlString &instr ) const
{
	XmlString str = instr;

	// check if str contains any invalid characters
	str.replace("&","&amp;");
	str.replace("\"","&quot;");
	str.replace("\'","&apos;");
	str.replace("<","&lt;");
	str.replace(">","&gt;");
	str.replace("...","&gt;");

	return str;
}

//////////////////////////////////////////////////////////////////////////
void CXmlNode::AddToXmlString( XmlString &xml, int level, FILE* pFile, size_t chunkSize ) const
{
	if (pFile && chunkSize > 0)
	{
		size_t len = xml.length();
		if (len >= chunkSize)
		{
			gEnv->pCryPak->FWrite(xml.c_str(), len, 1, pFile);
			xml.assign (""); // should not free memory and does not!
		}
	}

	AddTabsToString( xml,level );

	// Begin Tag
	if (m_attributes.empty()) {
		xml += "<";
		xml += m_tag;
		if (*m_content == 0 && m_childs.empty())
		{
			// Compact tag form.
			xml += " />\n";
			return;
		}
		xml += ">";
	} else {
		xml += "<";
		xml += m_tag;
		xml += " ";

		// Put attributes.
		for (XmlAttributes::const_iterator it = m_attributes.begin(); it != m_attributes.end(); )
		{
			xml += it->key;
			xml += "=\"";
			if (IsValidXmlString(it->value))
				xml += it->value;
			else
				xml += MakeValidXmlString(it->value);
			it++;
			if (it != m_attributes.end())
			xml += "\" ";
			else
				xml += "\"";
		}
		if (*m_content == 0 && m_childs.empty())
		{
			// Compact tag form.
			xml += "/>\n";
			return;
		}
		xml += ">";
	}

	// Put node content.
	if (IsValidXmlString(m_content))
		xml += m_content;
	else
		xml += MakeValidXmlString(m_content);

	if (m_childs.empty())
	{
		xml += "</";
		xml += m_tag;
		xml += ">\n";
		return;
	}

		xml += "\n";

	// Add sub nodes.
	for (XmlNodes::const_iterator it = m_childs.begin(); it != m_childs.end(); ++it)
	{
		IXmlNode *node = *it;
		((CXmlNode*)node)->AddToXmlString( xml,level+1,pFile,chunkSize);
	}

	// Add tabs.
	AddTabsToString( xml,level );
	xml += "</";
	xml += m_tag;
	xml += ">\n";
}

//////////////////////////////////////////////////////////////////////////
IXmlStringData* CXmlNode::getXMLData( int nReserveMem ) const
{
	CXmlStringData *pStrData = new CXmlStringData;
	pStrData->m_string.reserve(nReserveMem);
	AddToXmlString( pStrData->m_string,0 );
	return pStrData;
}

//////////////////////////////////////////////////////////////////////////
XmlString CXmlNode::getXML( int level ) const
{
	XmlString xml;
	xml = "";
	xml.reserve( 1024 );

	AddToXmlString( xml,level );
	return xml;
}

bool CXmlNode::saveToFile( const char *fileName )
{
#ifdef WIN32
	CrySetFileAttributes( fileName,0x00000080 ); // FILE_ATTRIBUTE_NORMAL
#endif //WIN32
	XmlString xml = getXML();
	FILE *file = gEnv->pCryPak->FOpen( fileName,"wt" );
	if (file)
	{
		const char *sxml = (const char*)xml;
		gEnv->pCryPak->FWrite( sxml,xml.length(),file );
		gEnv->pCryPak->FClose(file);
		return true;
	}
	return false;
}

bool CXmlNode::saveToFile( const char* fileName, size_t chunkSize)
{
#ifdef WIN32
	CrySetFileAttributes( fileName,0x00000080 ); // FILE_ATTRIBUTE_NORMAL
#endif //WIN32

	if (chunkSize < 256*1024)   // make at least 256k
		chunkSize = 256*1024;

	static XmlString xml;
	xml.assign ("");
	xml.reserve( chunkSize*2 ); // we reserve double memory, as writing in chunks is not really writing in fixed blocks but a bit fuzzy

	ICryPak* pCryPak = gEnv->pCryPak;
	FILE* pFile = pCryPak->FOpen(fileName, "wt");
	if (!pFile)
		return false;
	AddToXmlString(xml, 0, pFile, chunkSize);
	size_t len = xml.length();
	if (len > 0)
		pCryPak->FWrite(xml.c_str(), len, 1, pFile);
	pCryPak->FClose(pFile);
	xml.clear(); // xml.resize(0) would not reclaim memory
	return true;
}


/**
******************************************************************************
* XmlParserImp class.
******************************************************************************
*/
class XmlParserImp : public IXmlStringPool
{
public:
	XmlParserImp();
	~XmlParserImp();

	void ParseBegin( bool bCleanPools );
	XmlNodeRef ParseFile( const char *filename,XmlString &errorString,bool bCleanPools );
	XmlNodeRef ParseBuffer( const char *buffer,size_t bufLen,XmlString &errorString,bool bCleanPools );
	void ParseEnd();

	// Add new string to pool.
	char* AddString( const char *str ) { return m_stringPool.Append( str,(int)strlen(str) ); }
	//char* AddString( const char *str ) { return (char*)str; }

protected:
	void	onStartElement( const char *tagName,const char **atts );
	void	onEndElement( const char *tagName );
	void	onRawData( const char *data );

	static void startElement(void *userData, const char *name, const char **atts) {
		((XmlParserImp*)userData)->onStartElement( name,atts );
	}
	static void endElement(void *userData, const char *name ) {
		((XmlParserImp*)userData)->onEndElement( name );
	}
	static void characterData( void *userData, const char *s, int len ) {
		char str[32768];
		assert( len < 32768 );
		strncpy( str,s,len );
		str[len] = 0;
		((XmlParserImp*)userData)->onRawData( str );
	}

	void CleanStack();

	struct SStackEntity
	{
		XmlNodeRef node;
		std::vector<CXmlNode*> childs;
	};

	// First node will become root node.
	std::vector<SStackEntity> m_nodeStack;
	int m_nNodeStackTop;

	XmlNodeRef m_root;

	XML_Parser m_parser;
	CSimpleStringPool m_stringPool;
};

//////////////////////////////////////////////////////////////////////////
void XmlParserImp::CleanStack()
{
	m_nNodeStackTop = 0;
	for (int i = 0, num = m_nodeStack.size(); i < num; i++)
	{
		m_nodeStack[i].node = 0;
		m_nodeStack[i].childs.resize(0);
	}
}

/**
******************************************************************************
* XmlParserImp
******************************************************************************
*/
void	XmlParserImp::onStartElement( const char *tagName,const char **atts )
{
	CXmlNode *pCNode = new CXmlNode;
	pCNode->m_pStringPool = this;
	pCNode->m_pStringPool->AddRef();
	pCNode->m_tag = AddString(tagName);

	XmlNodeRef node = pCNode;

	m_nNodeStackTop++;
	if (m_nNodeStackTop >= m_nodeStack.size())
		m_nodeStack.resize(m_nodeStack.size()*2);

	m_nodeStack[m_nNodeStackTop].node = pCNode;
	m_nodeStack[m_nNodeStackTop-1].childs.push_back( pCNode );

	if (!m_root)
	{
		m_root = node;
	}
	else
	{
		pCNode->m_parent = (CXmlNode*)(IXmlNode*)m_nodeStack[m_nNodeStackTop-1].node;
		node->AddRef(); // Childs need to be add refed.
	}

	node->setLine( XML_GetCurrentLineNumber( (XML_Parser)m_parser ) );
	
	// Call start element callback.
	int i = 0;
	int numAttrs = 0;
	while (atts[i] != 0)
	{
		numAttrs++;
		i += 2;
	}
	if (numAttrs > 0)
	{
		i = 0;
		pCNode->m_attributes.resize(numAttrs);
		int nAttr = 0;
		while (atts[i] != 0)
		{
			pCNode->m_attributes[nAttr].key = AddString(atts[i]);
			pCNode->m_attributes[nAttr].value = AddString(atts[i+1]);
			nAttr++;
		i += 2;
	}
		// Sort attributes.
		//std::sort( pCNode->m_attributes.begin(),pCNode->m_attributes.end() );
	}
}

void	XmlParserImp::onEndElement( const char *tagName )
{
	assert( m_nNodeStackTop > 0 );

	if (m_nNodeStackTop > 0)
	{
		// Copy current childs to the parent.
		IXmlNode* currNode = m_nodeStack[m_nNodeStackTop].node;
		((CXmlNode*)currNode)->m_childs = m_nodeStack[m_nNodeStackTop].childs;
		m_nodeStack[m_nNodeStackTop].childs.resize(0);
		m_nodeStack[m_nNodeStackTop].node = 0;
	}
	m_nNodeStackTop--;
}

void	XmlParserImp::onRawData( const char* data )
{
	assert( m_nNodeStackTop >= 0 );
	if (m_nNodeStackTop >= 0)
	{
		size_t len = strlen(data);
		if (len > 0)
		{
			if (strspn(data,"\r\n\t ") != len)
			{
				CXmlNode *node = (CXmlNode*)(IXmlNode*)m_nodeStack[m_nNodeStackTop].node;
				if (*node->m_content != '\0')
				{
					node->m_content = m_stringPool.ReplaceString( node->m_content,data );
				}
				else
					node->m_content = AddString(data);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlParserImp::XmlParserImp()
{
	m_nNodeStackTop = 0;
	m_parser = 0;
	m_nodeStack.resize(32);
	CleanStack();
}

XmlParserImp::~XmlParserImp()
{
	ParseEnd();
}

//////////////////////////////////////////////////////////////////////////
void XmlParserImp::ParseBegin( bool bCleanPools )
{
	m_root = 0;
	CleanStack();

	if (bCleanPools)
		m_stringPool.Clear();

	XML_Memory_Handling_Suite memHandler;
	memHandler.malloc_fcn = malloc;
	memHandler.realloc_fcn = realloc;
	memHandler.free_fcn = free;
	m_parser = XML_ParserCreate_MM(NULL,&memHandler,NULL);

	XML_SetUserData( m_parser, this );
	XML_SetElementHandler( m_parser, startElement,endElement );
	XML_SetCharacterDataHandler( m_parser,characterData );
	XML_SetEncoding( m_parser,"utf-8" );
}

//////////////////////////////////////////////////////////////////////////
void XmlParserImp::ParseEnd()
{
	if (m_parser)
		XML_ParserFree( m_parser );
	m_parser = 0;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef XmlParserImp::ParseBuffer( const char *buffer,size_t bufLen,XmlString &errorString,bool bCleanPools )
{
	ParseBegin(bCleanPools);
	m_stringPool.SetBlockSize( (unsigned int)bufLen / 16 );

	XmlNodeRef root = 0;

	if (XML_Parse( m_parser,buffer,(int)bufLen,1 ))
	{
		root = m_root;
	} else {
		char *str = new char [1024];
		sprintf( str,"XML Error: %s at line %d",XML_ErrorString(XML_GetErrorCode(m_parser)),XML_GetCurrentLineNumber(m_parser) );
		errorString = str;
		CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"%s",str );
		delete [] str;
	}
	m_root = 0;
	ParseEnd();

	return root;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef XmlParserImp::ParseFile( const char *filename,XmlString &errorString,bool bCleanPools )
{
	XmlNodeRef root = 0;

	int fileSize = 0;
	{
		CCryFile file;
		if (file.Open(filename,"rb"))
		{
			fileSize = file.GetLength();
			if (fileSize > 0)
			{
				ParseBegin(bCleanPools);
				m_stringPool.SetBlockSize( (unsigned int)fileSize / 16 );
				char *buffer = (char*)XML_GetBuffer(m_parser,fileSize);
				file.ReadRaw( buffer,fileSize );
			}
		}
	}

	if (fileSize > 0)
	{
		if (XML_ParseBuffer( m_parser,fileSize,1 ))
		{
			root = m_root;
		} else {
			char *str = new char [1024];
			sprintf( str,"XML Error: %s at line %d in file %s",XML_ErrorString(XML_GetErrorCode(m_parser)),XML_GetCurrentLineNumber(m_parser),filename );
			errorString = str;
			CryWarning( VALIDATOR_MODULE_SYSTEM,VALIDATOR_WARNING,"%s",str );
			delete [] str;
		}
	}
	m_root = 0;
	if (m_parser)
		ParseEnd();

	return root;
}

XmlParser::XmlParser()
{
	m_nRefCount = 0;
	m_pImpl = new XmlParserImp;
	m_pImpl->AddRef();
}

XmlParser::~XmlParser()
{
	m_pImpl->Release();
}

//! Parse xml file.
XmlNodeRef XmlParser::parse( const char *fileName )
{
	m_errorString = "";
	return m_pImpl->ParseFile( fileName,m_errorString,true );
}

//! Parse xml from memory buffer.
XmlNodeRef XmlParser::parseBuffer( const char *buffer )
{
	m_errorString = "";
	return m_pImpl->ParseBuffer( buffer,strlen(buffer),m_errorString,true );
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef XmlParser::ParseBuffer( const char *buffer,int nBufLen,bool bCleanPools )
{
	m_errorString = "";
	return m_pImpl->ParseBuffer( buffer,nBufLen,m_errorString,bCleanPools );
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef XmlParser::ParseFile( const char *filename,bool bCleanPools )
{
	m_errorString = "";
	return m_pImpl->ParseFile( filename,m_errorString,bCleanPools );
}
