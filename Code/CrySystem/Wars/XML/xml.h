////////////////////////////////////////////////////////////////////////////
//
//  CryEngine Source File.
//  Copyright (C), Crytek Studios, 2001-2006.
// -------------------------------------------------------------------------
//  File name:   xml.h
//  Created:     21/04/2006 by Timur.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __XML_NODE_HEADER__
#define __XML_NODE_HEADER__

#include <algorithm>
#include "CryCommon/CryCore/PoolAllocator.h"

#include "CryCommon/CrySystem/IXml.h"

// track some XML stats. only to find persistent XML nodes in the system
// slow, so disable by default
//#define CRY_COLLECT_XML_NODE_STATS 
//#undef CRY_COLLECT_XML_NODE_STATS

struct IXmlStringPool
{
public:
	IXmlStringPool() { m_refCount = 0; }
	virtual ~IXmlStringPool() {};
	void AddRef() { m_refCount++; };
	void Release() {
		if (--m_refCount <= 0) 
			delete this; 
	};
	virtual char* AddString( const char *str ) = 0;
private:
	int m_refCount;
};

//524288
//////////////////////////////////////////////////////////////////////////
// CryMP C++20: the legacy CryMemoryAllocator/node_alloc implementation is not
// part of the current CryCommon. XML only requires alloc/dealloc here, so keep
// the ownership local and route it through CryMP's module allocator.
struct xml_node_allocator
{
	void* alloc(size_t size) { return ModuleAlloc(nullptr, size); }
	void dealloc(void* ptr, size_t) { ModuleAlloc(ptr, 0); }
};

//////////////////////////////////////////////////////////////////////////
struct XmlDynArrayAlloc
{
	static xml_node_allocator allocator;
	static size_t m_iAllocated;

	static void* Alloc( void* ptr, size_t osize,size_t nsize )
	{
		if (NULL == ptr)
		{
			if (nsize)
			{
				m_iAllocated += nsize;
				return allocator.alloc(nsize);
			}

			return 0;
		}
		else
		{
			void *nptr = 0;
			if (nsize)
			{
				nptr = (char*)allocator.alloc(nsize);
				memcpy(nptr, ptr, nsize>osize ? osize : nsize);
				m_iAllocated += nsize;
			}
			allocator.dealloc(ptr, osize);
			m_iAllocated -= osize;
			return nptr;
		}
	}
};
//////////////////////////////////////////////////////////////////////////

/************************************************************************/
/* XmlParser class, Parse xml and return root xml node if success.      */
/************************************************************************/
class XmlParser : public IXmlParser
{
public:
	XmlParser();
	~XmlParser();

	void AddRef()
	{
		++m_nRefCount;
	}

	void Release()
	{
		if (--m_nRefCount <= 0)
			delete this;
	}

	virtual XmlNodeRef ParseFile( const char *filename,bool bCleanPools );
	virtual XmlNodeRef ParseBuffer( const char *buffer,int nBufLen,bool bCleanPools );

	//! Parse xml file.
	XmlNodeRef parse( const char *fileName );

	//! Parse xml from memory buffer.
	XmlNodeRef parseBuffer( const char *buffer );

	const char* getErrorString() const { return m_errorString.c_str(); }

private:
	int m_nRefCount;
	XmlString m_errorString;
	class XmlParserImp *m_pImpl;
};

// Compare function for string comparasion, can be strcmp or stricmp
typedef int (__cdecl *XmlStrCmpFunc)( const char *str1,const char *str2 );
extern XmlStrCmpFunc g_pXmlStrCmp;

//////////////////////////////////////////////////////////////////////////
// XmlAttribute class
//////////////////////////////////////////////////////////////////////////
struct XmlAttribute
{
	const char* key;
	const char* value;

	bool operator<( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) < 0; }
	bool operator>( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) > 0; }
	bool operator==( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) == 0; }
	bool operator!=( const XmlAttribute &attr ) const { return g_pXmlStrCmp( key,attr.key ) != 0; }
};

//! Xml node attributes class.
//typedef DynArray<XmlAttribute,XmlDynArrayAlloc>	XmlAttributes;
typedef std::vector<XmlAttribute>	XmlAttributes;
typedef XmlAttributes::iterator XmlAttrIter;
typedef XmlAttributes::const_iterator XmlAttrConstIter;

/**
******************************************************************************
* CXmlNode class
* Never use CXmlNode directly instead use reference counted XmlNodeRef.
******************************************************************************
*/

class CXmlNode : public IXmlNode
{
public:
	//! Constructor.
	CXmlNode();
	CXmlNode( const char *tag );
	//! Destructor.
	~CXmlNode();

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete with pool allocator.
	//////////////////////////////////////////////////////////////////////////
	//void* operator new( size_t nSize );
	//void operator delete( void *ptr );

	virtual void DeleteThis() { delete this; };

	//! Create new XML node.
	XmlNodeRef createNode( const char *tag );

	//! Get XML node tag.
	const char *getTag() const { return m_tag; };
	void	setTag( const char *tag );

	//! Return true if givven tag equal to node tag.
	bool isTag( const char *tag ) const;

	//! Get XML Node attributes.
	virtual int getNumAttributes() const { return (int)m_attributes.size(); };
	//! Return attribute key and value by attribute index.
	virtual bool getAttributeByIndex( int index,const char **key,const char **value );

	virtual void copyAttributes( XmlNodeRef fromNode );

	//! Get XML Node attribute for specified key.
	const char* getAttr( const char *key ) const;
	//! Check if attributes with specified key exist.
	bool haveAttr( const char *key ) const;

	//! Creates new xml node and add it to childs list.
	XmlNodeRef newChild( const char *tagName );

	//! Adds new child node.
	void addChild( const XmlNodeRef &node );
	//! Remove child node.
	void removeChild( const XmlNodeRef &node );

	//! Remove all child nodes.
	void removeAllChilds();

	//! Get number of child XML nodes.
	int	getChildCount() const { return (int)m_childs.size(); };

	//! Get XML Node child nodes.
	XmlNodeRef getChild( int i ) const;

	//! Find node with specified tag.
	XmlNodeRef findChild( const char *tag ) const;
	void deleteChild( const char *tag );
	void deleteChildAt( int nIndex );

	//! Get parent XML node.
	XmlNodeRef	getParent() const { return m_parent; }

	//! Returns content of this node.
	const char* getContent() const { return m_content ? m_content : ""; };
	void setContent( const char *str );

	XmlNodeRef	clone();

	//! Returns line number for XML tag.
	int getLine() const { return m_line; };
	//! Set line number in xml.
	void setLine( int line ) { m_line = line; };

	//! Returns XML of this node and sub nodes.
	virtual IXmlStringData* getXMLData( int nReserveMem=0 ) const;
	XmlString getXML( int level=0 ) const;
	bool saveToFile( const char *fileName ); // saves in one huge chunk 
	bool saveToFile( const char *fileName, size_t chunkSizeBytes ); // save in small memory chunks

	//! Set new XML Node attribute (or override attribute with same key).
	void setAttr( const char* key,const char* value );
	void setAttr( const char* key,int value );
	void setAttr( const char* key,unsigned int value );
	void setAttr( const char* key,int64 value );
	void setAttr( const char* key,uint64 value );
	void setAttr( const char* key,float value );
	void setAttr( const char* key,const Vec2& value );
	void setAttr( const char* key,const Ang3& value );
	void setAttr( const char* key,const Vec3& value );
	void setAttr( const char* key,const Quat &value );

	//! Delete attrbute.
	void delAttr( const char* key );
	//! Remove all node attributes.
	void removeAllAttributes();

	//! Get attribute value of node.
	bool getAttr( const char *key,int &value ) const;
	bool getAttr( const char *key,unsigned int &value ) const;
	bool getAttr( const char *key,int64 &value ) const;
	bool getAttr( const char *key,uint64 &value ) const;
	bool getAttr( const char *key,float &value ) const;
	bool getAttr( const char *key,bool &value ) const;
	bool getAttr( const char *key,XmlString &value ) const { XmlString v; if (v=getAttr(key)) { value = v; return true; } else return false; }
	bool getAttr( const char *key,Vec2& value ) const;
	bool getAttr( const char *key,Ang3& value ) const;
	bool getAttr( const char *key,Vec3& value ) const;
	bool getAttr( const char *key,Quat &value ) const;
	//	bool getAttr( const char *key,CString &value ) const { XmlString v; if (getAttr(key,v)) { value = (const char*)v; return true; } else return false; }

private:
	void AddToXmlString( XmlString &xml,int level,FILE* pFile=0,size_t chunkSizeBytes=0 ) const;
	XmlString MakeValidXmlString( const XmlString &xml ) const;
	bool IsValidXmlString( const char *str ) const;
	XmlAttrConstIter GetAttrConstIterator( const char *key ) const
	{
		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::const_iterator it = std::find( m_attributes.begin(),m_attributes.end(),tempAttr );
		return it;

		/*
		XmlAttributes::const_iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		return it;
		return m_attributes.end();
		*/
	}
	XmlAttrIter GetAttrIterator( const char *key )
	{
		XmlAttribute tempAttr;
		tempAttr.key = key;

		XmlAttributes::iterator it = std::find( m_attributes.begin(),m_attributes.end(),tempAttr );
		return it;

		//		XmlAttributes::iterator it = std::lower_bound( m_attributes.begin(),m_attributes.end(),tempAttr );
		//if (it != m_attributes.end() && _stricmp(it->key,key) == 0)
		//return it;
		//return m_attributes.end();
	}
	const char* GetValue( const char *key ) const
	{
		XmlAttrConstIter it = GetAttrConstIterator(key);
		if (it != m_attributes.end())
			return it->value;
		return 0;
	}

private:
	// String pool used by this node.
	IXmlStringPool *m_pStringPool;

	//! Tag of XML node.
	const char *m_tag;
	//! Content of XML node.
	const char *m_content;
	//! Parent XML node.
	CXmlNode *m_parent;

	//typedef DynArray<CXmlNode*,XmlDynArrayAlloc>	XmlNodes;
	typedef std::vector<CXmlNode*>	XmlNodes;
	XmlNodes m_childs;
	//! Xml node attributes.
	XmlAttributes m_attributes;

	//! Line in XML file where this node firstly appeared (usefull for debugging).
	int m_line;

	friend class XmlParserImp;
};

typedef stl::PoolAllocatorNoMT<sizeof(CXmlNode)> CXmlNode_PoolAlloc;
extern CXmlNode_PoolAlloc *g_pCXmlNode_PoolAlloc;

#ifdef CRY_COLLECT_XML_NODE_STATS
typedef std::set<CXmlNode*> TXmlNodeSet; // yes, slow, but really only for one-shot debugging
struct SXmlNodeStats
{
	SXmlNodeStats() : nAllocs(0), nFrees(0) {}
	TXmlNodeSet nodeSet;
	uint32 nAllocs;
	uint32 nFrees;
};
extern SXmlNodeStats* g_pCXmlNode_Stats;
#endif

/*
//////////////////////////////////////////////////////////////////////////
inline void* CXmlNode::operator new( size_t nSize )
{
	void *ptr = g_pCXmlNode_PoolAlloc->Allocate();
	if (ptr)
	{
		memset( ptr,0,nSize ); // Clear objects memory.
#ifdef CRY_COLLECT_XML_NODE_STATS
		g_pCXmlNode_Stats->nodeSet.insert(reinterpret_cast<CXmlNode*> (ptr));
		++g_pCXmlNode_Stats->nAllocs;
#endif
	}
	return ptr;
}

//////////////////////////////////////////////////////////////////////////
inline void CXmlNode::operator delete( void *ptr )
{
	if (ptr)
	{
		g_pCXmlNode_PoolAlloc->Deallocate(ptr);
#ifdef CRY_COLLECT_XML_NODE_STATS
		g_pCXmlNode_Stats->nodeSet.erase(reinterpret_cast<CXmlNode*> (ptr));
		++g_pCXmlNode_Stats->nFrees;
#endif
	}
}
*/

#endif // __XML_NODE_HEADER__