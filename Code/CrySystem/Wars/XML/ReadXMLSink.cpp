#include "StdAfx.h"
#include "ReadWriteXMLSink.h"

#include "CryCommon/CrySystem/ISystem.h"
#include <stack>

typedef std::map<string, XmlNodeRef> IdTable;

static bool IsOptionalReadXML( XmlNodeRef& definition );
static bool CheckEnum( const char * name, XmlNodeRef& definition, XmlNodeRef& data );

static bool LoadTableInner( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadArray( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadProperty( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadTable( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadReferencedId( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadSomething( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );
static bool LoadArraySetValueTable( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink, int elem );

typedef bool (*LoadArraySetValue)( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink, int elem );
typedef bool (*LoadDefinitionFunction)( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink );


template <class T> struct ReadPropertyTyped;

template <class T>
struct ReadPropertyTyped
{
  static bool Load( const char * name, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
  {
    T value;
    memset(&value, 0, sizeof(T));

    if (!pSink->IsCreationMode())
    {
      if (!data->haveAttr(name))
        return false;
      if (!data->getAttr(name, value))
        return false;
      if (!CheckEnum(name, definition, data))
        return false;    
    }

    IReadXMLSink::TValue vvalue(value);
    pSink->SetValue(name, vvalue, definition);
    return true;    
  }
  static bool LoadArray( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink, int elem )
  {
    T value;
    memset(&value, 0, sizeof(T));

    if (!pSink->IsCreationMode())
    {
      if (!data->haveAttr("value"))
        return false;
      if (!data->getAttr("value", value))
        return false;
    }

    IReadXMLSink::TValue vvalue(value);
    pSink->SetAt(elem, vvalue, definition);
    return true;
  }
};

template <>
struct ReadPropertyTyped<string>
{
  static bool Load( const char * name, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
  {
    const char* value = 0;

    if (!pSink->IsCreationMode())
    {
      if (!data->haveAttr(name))
        return false;      
      if (!CheckEnum(name, definition, data))
        return false;

      value = data->getAttr(name);
    }

    IReadXMLSink::TValue vvalue( value );
    pSink->SetValue( name, vvalue, definition );
    return true;
  }
  static bool LoadArray( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink, int elem )
  {
    const char* value = 0;

    if (!pSink->IsCreationMode())
    {
      if (!data->haveAttr("value"))
        return false;

      value = data->getAttr("value");
    }

    IReadXMLSink::TValue vvalue( value );
    pSink->SetAt( elem, vvalue, definition );
    return true;
  }
};

bool IsOptionalReadXML( XmlNodeRef& definition )
{
  bool optional = false;
  definition->getAttr("optional", optional);
  return optional;
}

bool CheckEnum( const char * name, XmlNodeRef& definition, XmlNodeRef& data )
{
  if (XmlNodeRef enumNode = definition->findChild("Enum"))
  { 
    // if restrictive attribute set to false, check always succeeds 
    if (enumNode->haveAttr("restrictive"))
    {      
      bool res = true;
      enumNode->getAttr("restrictive", res);
      if (!res)
        return true;
    }

    // else check enum values
    const char* val = data->getAttr(name);
    for (int i=0; i<enumNode->getChildCount(); ++i)
    { 
      if (0 == strcmp(enumNode->getChild(i)->getContent(), val))
        return true;
    }
    return false;   
  }  
  return true;
}

bool LoadProperty( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  const char * name = definition->getAttr( "name" );
  if (0 == strlen(name))
  {
    CryLog("Property has no name");
    return false;
  }

  const char * type = definition->getAttr( "type" );
  if (0 == strlen(type))
  {
    CryLog("Property '%s' has no type", type);
    return false;
  }

  if (!pSink->IsCreationMode())
  {
    if (XmlNodeRef childRef = data->findChild(name))
    {
      if (data->haveAttr(name))
      {
        CryLog("Duplicate definition (attribute and element) for %s", name);
        return false;
      }
      if (childRef->getChildCount())
      {
        CryLog("Property-style elements can not have children (property was %s)", name);
        return false;
      }
      string content = childRef->getContent();
      data->setAttr( name, content.Trim().c_str() );
      data->removeChild( childRef );
    }

    if (!data->haveAttr(name))
    {
      if (!IsOptionalReadXML(definition))
      {
        CryLog("Failed to load property %s", name);
        return false;
      }
      return true;
    }
  }	
  else 
  {
    if (definition->haveAttr("allowAlways"))
      return true;
  }
  
  bool ok = false;
#define LOAD_PROPERTY(whichType) else if (0 == strcmp(type, #whichType)) ok = ReadPropertyTyped<whichType>::Load(name, definition, data, pSink)
  XML_SET_PROPERTY_HELPER(LOAD_PROPERTY);
#undef LOAD_PROPERTY

  if (!ok)
    CryLog("Failed loading attribute %s of type %s", name, type);

  return ok;
}

bool LoadArraySetValueTable( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink, int elem )
{
  IReadXMLSinkPtr pChildSink = pSink->BeginTableAt(elem, definition);

  if (pSink->IsCreationMode() && definition->haveAttr("type"))
  {
    if (!LoadSomething( idTable, definition, data, &*pChildSink ))
      return false;
  }
  else
  {
    if (!LoadTableInner( idTable, definition, data, &*pChildSink ))
      return false;
  }

  if (!pChildSink->EndTableAt( elem ))
  {
    CryLog( "Failed to finish table at element %d", elem );
    return false;
  }

  return true;
}

bool LoadArray( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  const char * name = definition->getAttr( "name" );
  if (0 == strlen(name))
  {
    CryLog("Array has no name");
    return false;
  }

  const char * elementName = definition->getAttr( "elementName" );
  if (0 == strlen(elementName))
    elementName = "element";

  bool validateArray = true;
  if (definition->haveAttr(elementName))
    definition->getAttr("validate", validateArray);


  XmlNodeRef childData;

  if (!pSink->IsCreationMode())
  {
    childData = data->findChild( name );
    if (!childData)
    {
      bool ok = IsOptionalReadXML(definition);
      if (!ok)
        CryLog( "Failed to load child table %s", name );
      return ok;
    }
  }

  IReadXMLSinkPtr childSink = pSink->BeginArray( name, definition );
  if (!childSink)
  {
    CryLog("Failed to begin array named %s", name);
    return false;
  }

  LoadArraySetValue setter = NULL;
  if (definition->haveAttr("type"))
  {
    setter = NULL;
    const char * type = definition->getAttr("type");
#define SETTER_PROPERTY(whichType) else if (0 == strcmp(type, #whichType)) setter = ReadPropertyTyped<whichType>::LoadArray
    XML_SET_PROPERTY_HELPER(SETTER_PROPERTY);
#undef SETTER_PROPERTY
    if (!setter)
    {
      CryLog("Unknown type %s in array %s", type, name);
      return false;
    }
  }
  else
  {
    setter = LoadArraySetValueTable;
  }

  if (!pSink->IsCreationMode())
  {
    int numElems = childData->getChildCount();
    int elem = 1;
    for (int i=0; i<numElems; i++)
    {
      XmlNodeRef elemData = childData->getChild(i);
      if (0 == strcmp(elemData->getTag(), elementName))
      {
        int increment = 1;
        if (elemData->haveAttr("_index"))
        {
          if (!elemData->getAttr("_index", elem))
          {
            CryLog( "_index is not an integer in array %s (pos hint=%d)", name, elem );
            return false;
          }         
        }
        if (!setter( idTable, definition, elemData, &*childSink, elem ))
        {
          CryLog( "Failed loading element %d of array %s", elem, name );
          return false;
        }
        elem += increment;
      }
      else if (validateArray)
      {
        CryLog( "Invalid node %s in array %s", elemData->getTag(), name );
        return false;
      }
    }
  }
  else
  {    
    // only process array content for the array being created
    if (0 == strcmp(name, pSink->GetCreationNode()->getAttr("name")))
    {
      if (!setter( idTable, definition, data, &*childSink, 1 ))
      {
        CryLog( "[ReadXML CreationMode]: Failed loading element %d of array %s", 1, name );
        return false;
      }
    }
  }

  if (!pSink->EndArray(name))
  {
    CryLog("Failed to finish array named %s", name);
    return false;
  }

  return true;
}

bool LoadTable( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  const char * name = definition->getAttr( "name" );
  if (0 == strlen(name))
  {
    CryLog("Child-table has no name");
    return false;
  }

  XmlNodeRef childData;

  if (!pSink->IsCreationMode())
  {
    childData = data->findChild( name );
    if (!childData)
    {
      bool ok = IsOptionalReadXML(definition);
      if (!ok)
        CryLog( "Failed to load child table %s", name );
      return ok;
    }
  }	

  IReadXMLSinkPtr childSink = pSink->BeginTable( name, definition );
  if (!childSink)
  {
    CryLog( "Sink creation failed for table %s", name );
    return false;
  }

  
  if (!LoadTableInner(idTable, definition, childData, childSink))
  {
    CryLog( "Failed to load data for child table %s", name );
    return false;
  }

  if (!pSink->EndTable( name ))
  {
    CryLog( "Table %s failed to complete in sink", name );
    return false;
  }

  return true;
}

bool LoadSomething( const IdTable& idTable, XmlNodeRef& nodeDefinition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  static struct 
  {
    const char * name;
    LoadDefinitionFunction loader;
  } loaderTypes[] = {
    {"Property", &LoadProperty},
    {"Array", &LoadArray},
    {"Table", &LoadTable},
    {"Use", &LoadReferencedId},    
  };
  static const int numLoaderTypes = sizeof(loaderTypes) / sizeof(*loaderTypes);

  const char * nodeDefinitionTag = nodeDefinition->getTag();
  bool ok = false;
  int i;
  
  for (i=0; i<numLoaderTypes; i++)
  {
    if (0 == strcmp(loaderTypes[i].name, nodeDefinitionTag))
    {
      ok = loaderTypes[i].loader( idTable, nodeDefinition, data, pSink );
      break;
    }
  }
  if (!ok)
  {
    if (i == numLoaderTypes)
      CryLog( "Invalid definition node type %s", nodeDefinitionTag );
  }
  return ok;
}

bool LoadReferencedId( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  IdTable::const_iterator iter = idTable.find(definition->getAttr("id"));
  if (iter == idTable.end())
  {
    CryLog("No definition with id '%s'", definition->getAttr("id"));
    return false;
  }
  XmlNodeRef useDefinition = iter->second;
  useDefinition = useDefinition->clone();
  int numAttrs = definition->getNumAttributes();
  for (int i=0; i<numAttrs; i++)
  {
    const char * key, * value;
    definition->getAttributeByIndex(i, &key, &value);
    useDefinition->setAttr(key, value);
  }
  return LoadSomething( idTable, useDefinition, data, pSink );
}

bool LoadTableInner( const IdTable& idTable, XmlNodeRef& definition, XmlNodeRef& data, IReadXMLSink * pSink )
{
  const int nChildrenDefinition = definition->getChildCount();
  
  for (int nChildDefinition = 0; nChildDefinition < nChildrenDefinition; nChildDefinition ++)
  {
    XmlNodeRef nodeDefinition = definition->getChild(nChildDefinition);

    if (!LoadSomething(idTable, nodeDefinition, data, pSink))
      return false;
  }
  
  return true;
}

bool CReadWriteXMLSink::ReadXML( XmlNodeRef rootDefinition, XmlNodeRef rootData, IReadXMLSink * pSink)
{
  if (!pSink->IsCreationMode())
  {
    if (0 == rootData)
      return false;

    if (0 != strcmp(rootDefinition->getTag(), "Definition"))
    {
      CryLog("Root tag of definition file was %s; expected Definition", rootDefinition->getTag());
      return false;
    }
    if (rootDefinition->haveAttr("root"))
    {
      if (0 != strcmp(rootDefinition->getAttr("root"), rootData->getTag()))
      {
        CryLog("Root data has wrong tag; was %s expected %s", rootData->getTag(), rootDefinition->getAttr("root"));
        return false;
      }
    }
  }

  XmlNodeRef useAlways = rootDefinition->findChild("AllowAlways");
  if (useAlways != 0)
  {
    rootDefinition->removeChild(useAlways);

    for (int i=0; i<useAlways->getChildCount(); ++i)
      useAlways->getChild(i)->setAttr("allowAlways", 1);
  }

  // scan for id's in the structure (for the Use member)
  IdTable idTable;
  std::stack<XmlNodeRef> scanStack;
  scanStack.push( rootDefinition );
  while (!scanStack.empty())
  {
    XmlNodeRef refNode = scanStack.top();
    scanStack.pop();

    int numChildren = refNode->getChildCount();
    const char* tag = refNode->getTag();

    for (int i=0; i<numChildren; i++)    
      scanStack.push(refNode->getChild(i));    

    if (refNode->haveAttr("id") && 0 != strcmp("Use", tag))
      idTable[refNode->getAttr("id")] = refNode;
    
    if (useAlways != 0 && (!strcmp("Table", tag) || (!strcmp("Array", tag) && !refNode->haveAttr("type"))))
      for (int i=0; i<useAlways->getChildCount(); ++i)
        refNode->addChild(useAlways->getChild(i)->clone());    
  }

  if (pSink->IsCreationMode() && rootDefinition->haveAttr("type"))
  {
    // if creating from a 0-child definiton node, load itself
    if (!LoadSomething(idTable, rootDefinition, rootData, pSink))
      return false;
  }
  else 
  {
    // load content
    if (!LoadTableInner( idTable, rootDefinition, rootData, pSink ))    
      return false;    
  }

  bool ok = pSink->Complete();
  if (!ok)
    CryLog( "Warning: sink failed to complete reading" );

  return ok;
}

bool CReadWriteXMLSink::ReadXML( XmlNodeRef definition, const char* dataFile, IReadXMLSink * pSink )
{
  XmlNodeRef rootData = GetISystem()->LoadXmlFile( dataFile );
  if (!rootData)
  {
    CryLog("Unable to load XML-Lua data file: %s", dataFile);
    return false;
  }
  return ReadXML(definition, rootData, pSink);
}

bool CReadWriteXMLSink::ReadXML( const char * definitionFile, XmlNodeRef rootData, IReadXMLSink * pSink )
{
  XmlNodeRef rootDefinition = GetISystem()->LoadXmlFile( definitionFile );
  if (!rootDefinition)
  {
    CryLog("Unable to load XML-Lua definition file: %s", definitionFile);
    return false;
  }  
  return ReadXML(rootDefinition, rootData, pSink);
}

bool CReadWriteXMLSink::ReadXML( const char * definitionFile, const char * dataFile, IReadXMLSink * pSink )
{
  XmlNodeRef rootData = GetISystem()->LoadXmlFile( dataFile );
  if (!rootData)
  {
    CryLog("Unable to load XML-Lua data file: %s", dataFile);
    return false;
  }
  if (!ReadXML( definitionFile, rootData, pSink ))
  {
    CryLog("Unable to load file %s", dataFile);
    return false;
  }
  return true;
}
