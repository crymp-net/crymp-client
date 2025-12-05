// Copyright (C) 2001-2008 Crytek GmbH

#pragma once

#include "CryCommon/CryCore/functor.h"
#include "CryCommon/CryCore/platform.h"
#include "CryCommon/CryMath/Cry_Math.h"

class ICrySizer;
class SmartScriptTable;

struct ISystem;
struct IScriptTable;
struct IFunctionHandler;

// script function reference
struct SScriptFuncHandle;
typedef SScriptFuncHandle* HSCRIPTFUNCTION;

//////////////////////////////////////////////////////////////////////////
// ScriptHandle type used to pass pointers and handles to/from Lua script.
// As lua script do not natively support integers, full range integers used
// as handles must be stored in Lua using this class.
//////////////////////////////////////////////////////////////////////////
union ScriptHandle
{
	UINT_PTR n;
	void *ptr;

	ScriptHandle() : ptr(0) {};
	ScriptHandle(int i) : n(i) {};
	ScriptHandle(void *p) : ptr(p) {};
};

////////////////////////////////////////////////////////////////////////////
enum ScriptVarType
{
	svtNull = 0,
	svtString,
	svtNumber,
	svtBool,
	svtFunction,
	svtObject,
	svtPointer,
	svtUserData,
};

//////////////////////////////////////////////////////////////////////////
// Any Script value.
//////////////////////////////////////////////////////////////////////////
enum ScriptAnyType
{
	ANY_ANY = 0,
	ANY_TNIL,
	ANY_TBOOLEAN,
	ANY_THANDLE,
	ANY_TNUMBER,
	ANY_TSTRING,
	ANY_TTABLE,
	ANY_TFUNCTION,
	ANY_TUSERDATA,
	ANY_TVECTOR,
	ANY_COUNT,
};

struct ScriptAnyValue
{
	ScriptAnyType type;
	union
	{
		bool b;
		float number;
		const char *str;
		IScriptTable *table;
		const void *ptr;
		HSCRIPTFUNCTION function;
		struct { float x,y,z; } vec3;
		struct { void * ptr; int nRef; } ud;
	};

	~ScriptAnyValue(); // implemented at the end of header

	ScriptAnyValue() : type(ANY_ANY) { table = 0; };
	ScriptAnyValue( ScriptAnyType _type ) : type(_type) {};
	ScriptAnyValue( bool value ) : type(ANY_TBOOLEAN) { b = value; };
	ScriptAnyValue( int value ) : type(ANY_TNUMBER) { number = (float)value; };
	ScriptAnyValue( unsigned int value ) : type(ANY_TNUMBER) { number = (float)value; };
	ScriptAnyValue( float value ) : type(ANY_TNUMBER) { number = value; };
	ScriptAnyValue( const char *value ) : type(ANY_TSTRING) { str = value; };
	ScriptAnyValue( ScriptHandle value ) : type(ANY_THANDLE) { ptr = value.ptr; };
	ScriptAnyValue( HSCRIPTFUNCTION value ) : type(ANY_TFUNCTION) { function = value; };
	ScriptAnyValue( const Vec3 &value ) : type(ANY_TVECTOR) { vec3.x=value.x; vec3.y=value.y; vec3.z=value.z; };
	ScriptAnyValue( const Ang3 &value ) : type(ANY_TVECTOR) { vec3.x=value.x; vec3.y=value.y; vec3.z=value.z; };
	ScriptAnyValue( IScriptTable *value ); // implemented at the end of header
	ScriptAnyValue( const SmartScriptTable &value ); // implemented at the end of header

	ScriptAnyValue( const ScriptAnyValue& value ); // implemented at the end of header
	void Swap( ScriptAnyValue& value ); // implemented at the end of header

	ScriptAnyValue& operator=( const ScriptAnyValue& rhs )
	{
		ScriptAnyValue temp(rhs);
		Swap(temp);
		return *this;
	}
	//////////////////////////////////////////////////////////////////////////
	// Compare 2 values.
	bool operator==( const ScriptAnyValue& rhs ) const
	{
		if (memcmp(this,&rhs,sizeof(*this)) == 0)
			return true;
		return false;
	}
	bool operator!=( const ScriptAnyValue& rhs ) const { return !(*this == rhs); };

	bool CopyTo( bool &value ) { if (type==ANY_TBOOLEAN) { value = b; return true; }; return false; };
	bool CopyTo( int &value ) { if (type==ANY_TNUMBER) { value = (int)number; return true; }; return false; };
	bool CopyTo( unsigned int &value ) { if (type==ANY_TNUMBER) { value = (unsigned int)number; return true; }; return false; };
	bool CopyTo( float &value ) { if (type==ANY_TNUMBER) { value = number; return true; }; return false; };
	bool CopyTo( const char *&value ) { if (type==ANY_TSTRING) { value = str; return true; }; return false; };
	bool CopyTo( char *&value ) { if (type==ANY_TSTRING) { value = (char*)str; return true; }; return false; };
	bool CopyTo( ScriptHandle &value ) { if (type==ANY_THANDLE) { value.ptr = const_cast<void*>(ptr); return true; }; return false; };
	bool CopyTo( HSCRIPTFUNCTION &value ) { if (type==ANY_TFUNCTION) { value = function; return true; }; return false; };
	bool CopyTo( Vec3 &value ) { if (type==ANY_TVECTOR) { value.x=vec3.x;value.y=vec3.y;value.z=vec3.z; return true; }; return false; };
	bool CopyTo( Ang3 &value ) { if (type==ANY_TVECTOR) { value.x=vec3.x;value.y=vec3.y;value.z=vec3.z; return true; }; return false; };
	bool CopyTo( IScriptTable *value ); // implemented at the end of header
	bool CopyTo( SmartScriptTable &value ); // implemented at the end of header

	// Clear any variable to uninitialized state.
	void Clear(); // implemented at the end of header

	// Only initialize type.
	ScriptAnyValue( bool,int ) : type(ANY_TBOOLEAN) {};
	ScriptAnyValue( int,int ) : type(ANY_TNUMBER) {};
	ScriptAnyValue( unsigned int,int ) : type(ANY_TNUMBER) {};
	ScriptAnyValue( float&,int ) : type(ANY_TNUMBER) {};
	ScriptAnyValue( const char*,int ) : type(ANY_TSTRING) {};
	ScriptAnyValue( ScriptHandle,int ) : type(ANY_THANDLE) {};
	ScriptAnyValue( HSCRIPTFUNCTION,int ) : type(ANY_TFUNCTION) {};
	ScriptAnyValue( Vec3&,int ) : type(ANY_TVECTOR) {};
	ScriptAnyValue( Ang3&,int ) : type(ANY_TVECTOR) {};
	ScriptAnyValue(IScriptTable *value, int);
	ScriptAnyValue(const SmartScriptTable & value, int);

	ScriptVarType GetVarType() const
	{
		switch (type)
		{
			case ANY_ANY:       return svtNull;
			case ANY_TNIL:      return svtNull;
			case ANY_TBOOLEAN:  return svtBool;
			case ANY_THANDLE:   return svtPointer;
			case ANY_TNUMBER:   return svtNumber;
			case ANY_TSTRING:   return svtString;
			case ANY_TTABLE:    return svtObject;
			case ANY_TFUNCTION: return svtFunction;
			case ANY_TUSERDATA: return svtUserData;
			case ANY_TVECTOR:   return svtObject;
		}

		return svtNull;
	}
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// Summary
//   Scripting Engine interface
// Description
//	 This interface is mapped 1:1 on a script state.
//	 All scripts loaded from the same interface instance are visible with
//   each others.
struct IScriptSystem
{
	// Summary
	//   Updates the system, per frame
	virtual void Update( void ) = 0;

	// Summary
	//   Sets the rate of Garbage Collection for script system
	// Parameters
	//   fRate - the rate in seconds
	virtual void SetGCFrequency( const float fRate ) = 0;

	// Summary
	//   Loads and runs a script file
	// Parameters
	//   sFileName - Path of the script file
	//   bRaiseError - When set to true, the script engine will call CryWarning
	//                 when an error in the script file occurs
	// Returns
	//   false if the execution fails, otherwise it will be true
	// Remarks
	//   All global variables and functions declared in the executed script will
	//   persist for all the script system lifetime.
	virtual bool ExecuteFile(const char *sFileName,bool bRaiseError = true, bool bForceReload=false) = 0;

	// Summary
	//   Executes an ASCII buffer
	// Parameters
	//   sBuffer - an 8bit ASCII buffer containing the script that must be executed
	//   bRaiseError - When set to true, the script engine will call CryWarning
	//                 when an error in the script file occurs
	//   sBufferDescription - Used as a name to describe the buffer
	// Returns
	//   false if the execution fails, otherwise it will be true
	// Remarks
	//   All global variables and functions declared in the executed script will
	//   persist for all the script system lifetime.
	virtual bool ExecuteBuffer(const char *sBuffer, size_t nSize,const char *sBufferDescription="") = 0;

	// Summary
	//   Unloads a script
	// Parameters
	//   sFileName - path of the script file
	// Remarks
	//   the script engine never loads twice the same file because it internally
	//   stores a list of the loaded files. Calling this functions will remove
	//   the script file from this list.
	// See Also
	//   UnloadScripts
	virtual void UnloadScript(const char *sFileName) = 0;

	// Summary
	//   Unloads all the scripts
	// Remarks
	//   UnloadScript
	virtual void UnloadScripts() = 0;

	// Summary
	//   Reloads a script
	// Parameters
	//   sFileName - path of the script file to reload
	//   bRaiseError - When set to true, the script engine will call CryWarning
	//                 when an error in the script file occurs
	// Returns
	//   false if the execution fails, otherwise it will be true
	// See Also
	//   ReloadScripts
	virtual bool ReloadScript(const char *sFileName,bool bRaiseError = true) = 0;

	// Summary
	//   Reloads all the scripts previously loaded
	// Returns
	//   false if the execution of one of the script fails, otherwise it will be
	//   true.
	virtual bool ReloadScripts() = 0;

	// Summary
	//   Generates a OnLoadedScriptDump() for every loaded script
	virtual void DumpLoadedScripts() = 0 ;

	// Summary
	//   Creates a new table accessible to the scripts
	/*!	create a new IScriptTable
		@return a pointer to the created object, with the reference count of 0
	*/
	virtual IScriptTable* CreateTable( bool bEmpty=false ) = 0;


	/*! start a call to script function
		@param sTableName name of the script table that contai the function
		@param sFuncName function name


		CALLING A SCRIPT FUNCTION:
			to call a function in the script object you must
			call BeginCall
			push all parameters whit PushParam
			call EndCall


		EXAMPLE:

			m_ScriptSystem->BeginCall("Player","OnInit");

			m_ScriptSystem->PushParam(pObj);

			m_ScriptSystem->PushParam(nTime);

			m_ScriptSystem->EndCall();

	*/
	//##@{
	virtual int BeginCall(HSCRIPTFUNCTION hFunc) = 0;						 // Márcio: changed the return type
	// Calls a named method inside specified table.
	virtual int BeginCall(const char *sFuncName) = 0;						 // from void to int for error checking
	virtual int BeginCall(const char *sTableName, const char *sFuncName) = 0;//
	// Calls a named method inside specified table.
	virtual int BeginCall( IScriptTable *pTable, const char *sFuncName ) = 0;
	//##@}

	/*! end a call to script function
		@param ret reference to the variable that will store an eventual return value
	*/
	//##@{
	virtual bool EndCall()=0;
	virtual bool EndCallAny( ScriptAnyValue &any )=0;
	virtual bool EndCallAnyN( int n, ScriptAnyValue* anys )=0;

	template <class T>
	bool EndCall( T &value )
	{
		ScriptAnyValue any(value,0);
		return EndCallAny(any) && any.CopyTo(value);
	}

	//##@}

	// Description:
	//    Get reference to the lua function.
	//    This reference must be released with IScriptSystem::ReleaseFunc()
	//##@{
	virtual HSCRIPTFUNCTION GetFunctionPtr(const char *sFuncName) = 0;
	virtual	HSCRIPTFUNCTION GetFunctionPtr(const char *sTableName, const char *sFuncName) = 0;
	//##@}

	// Description:
	//    Frees references created with GetFunctionPtr or GetValue for HSCRIPTFUNCTION.
	virtual void ReleaseFunc(HSCRIPTFUNCTION f) = 0;

	// Description:
	//    Push a parameter during a function call
	virtual void PushFuncParamAny( const ScriptAnyValue &any ) = 0;
	template <class T> void PushFuncParam( const T &value ) { PushFuncParamAny(value); }


	// Set Global value.
	virtual void SetGlobalAny( const char *sKey,const ScriptAnyValue &any ) = 0;
	// Get Global value.
	virtual bool GetGlobalAny( const char *sKey,ScriptAnyValue &any ) = 0;

	// Set Global value to Null.
	virtual void SetGlobalToNull(const char *sKey) { SetGlobalAny(sKey,ScriptAnyValue(ANY_TNIL)); }

	// Set Global value.
	template <class T> void SetGlobalValue( const char *sKey, const T &value ) { SetGlobalAny(sKey,ScriptAnyValue(value)); }
	// Get Global value.
	template <class T> bool GetGlobalValue( const char *sKey, T &value )
	{
		ScriptAnyValue any(value,0);
		return GetGlobalAny(sKey,any) && any.CopyTo(value);
	}

	virtual IScriptTable * CreateUserData(void * ptr, size_t size)=0;
	/*! remove a tagged value
		@param tag handle to a tagged value
	*/

	/*! force a Garbage collection cycle
		in the current status of the engine the automatic GC is disabled
		so this function must be called explicitly
	*/
	virtual void ForceGarbageCollection() = 0;

	/*! number of "garbaged" object
	*/
	virtual int GetCGCount() = 0;

	/*! legacy function*/
	virtual void SetGCThreshhold(int nKb) = 0;

	/*! release and destroy the script system*/
	virtual void Release() {}

	virtual void ShowDebugger(const char *file, int line, const char *reason) {}
	virtual int AddBreakPoint(const char *file, int line) { return 0; }

	virtual IScriptTable *GetLocalVariables(int nLevel = 0) { return 0; }

	/*!return a table containing 1 entry per stack level(aka per call)
		an entry will look like this table

		[1]={
			description="function bau()",
			line=234,
			sourcefile="/scripts/bla/bla/bla.lua"
		}

	 */
	virtual IScriptTable *GetCallsStack() { return 0; }

	virtual void DebugContinue() {}
	virtual void DebugStepNext() {}
	virtual void DebugStepInto() {}
	virtual void DebugDisable() {}
	virtual int GetBreakState() { return 4; }  // bsNoBreak

	virtual void GetMemoryStatistics(ICrySizer *pSizer) = 0;

	//! Is not recursive but combines the hash values of the whole table when the specifies variable is a table
	//! otherwise is has to be a lua function
	//!	@param sPath zero terminated path to the variable (e.g. _localplayer.cnt), max 255 characters
	//!	@param szKey zero terminated name of the variable (e.g. luaFunc), max 255 characters
	//! @param dwHash is used as input and output
	virtual void GetScriptHash( const char *sPath, const char *szKey, unsigned int &dwHash ) {}

	virtual void RaiseError( const char *format,... ) PRINTF_PARAMS(2, 3) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Called one time after initialization of system to register script system console vars.
	//////////////////////////////////////////////////////////////////////////
	virtual void PostInit() = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void LoadScriptedSurfaceTypes( const char *sFolder,bool bReload ) {}

	//////////////////////////////////////////////////////////////////////////
	// Serializes script timers.
	virtual void SerializeTimers( struct ISerialize *pSer ) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Resetting all the script timers.
	virtual void ResetTimers(  ) = 0;

	virtual int GetStackSize() = 0;

	// Retrieve size of memory allocated in script.
	virtual uint32 GetScriptAllocSize() = 0;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

struct IScriptTableDumpSink
{
	virtual void OnElementFound(const char *sName,ScriptVarType type) = 0;
	virtual void OnElementFound(int nIdx,ScriptVarType type) = 0;
};

////////////////////////////////////////////////////////////////////////////
struct IScriptTable
{
	typedef Functor1wRet<IFunctionHandler*,int> FunctionFunctor;
	typedef int (*UserDataFunction)( IFunctionHandler* pH,void *pBuffer,int nSize );

	// Get script system of this table
	virtual IScriptSystem* GetScriptSystem() const = 0;

	// Increment reference count to the script table.
	virtual void AddRef() = 0;
	// Call to decrement reference count for script table.
	// when reference count reaches zero, table will be deleted.
	virtual void Release() = 0;

	virtual void Delegate(IScriptTable *pObj) = 0;
	virtual void *GetUserDataValue() = 0;

	// Set the value of a table memeber.
	virtual void SetValueAny( const char *sKey,const ScriptAnyValue &any,bool bChain=false ) = 0;
	// Get the value of a table memeber.
	virtual bool GetValueAny( const char *sKey,ScriptAnyValue &any,bool bChain=false ) = 0;

	// Set value of a table member.
	template <class T> void SetValue( const char *sKey,const T &value ) { SetValueAny(sKey,value); }
	// Get value of a table member.
	template <class T> bool GetValue( const char *sKey,T &value )
	{
		ScriptAnyValue any(value,0);
		return GetValueAny(sKey,any) && any.CopyTo(value);
	}
	bool HaveValue( const char * sKey )
	{
		ScriptAnyValue any;
		GetValueAny(sKey, any);
		return any.type != ANY_TNIL;
	}
	// Set member value to nil.
	void SetToNull( const char *sKey )  { SetValueAny(sKey,ScriptAnyValue(ANY_TNIL)); }

	//////////////////////////////////////////////////////////////////////////
	// Chain Set/Get.
	// Is a faster version when doing a big ammount of SetValue/GetValue.
	//////////////////////////////////////////////////////////////////////////
	virtual bool BeginSetGetChain() = 0;
	virtual void EndSetGetChain() = 0;
	// Set value of a table member.
	template <class T> void SetValueChain( const char *sKey,const T &value ) { SetValueAny(sKey,value,true); }
	// Get value of a table member.
	template <class T> bool GetValueChain( const char *sKey,T &value )
	{
		ScriptAnyValue any(value,0);
		return GetValueAny(sKey,any,true) && any.CopyTo(value);
	}
	void SetToNullChain( const char *sKey )  { SetValueChain(sKey,ScriptAnyValue(ANY_TNIL)); }

	/*!Get the value type of a table member
		@param sKey variable name
		@return the value type (svtNull if doesn't exist)
	*/
	virtual ScriptVarType GetValueType( const char *sKey ) = 0;
	virtual ScriptVarType GetAtType( int nIdx ) = 0;

	// Description:
	//    Set the value of a memeber varible at the specified index
	//    this mean that you will use the object as vector into the script.
	virtual void SetAtAny( int nIndex,const ScriptAnyValue &any ) = 0;

	// Description:
	//    Get the value of a memeber varible at the specified index
	virtual bool GetAtAny( int nIndex,ScriptAnyValue &any ) = 0;

	// Description:
	//    Set the value of a memeber varible at the specified index
	template <class T> void SetAt( int nIndex,const T &value ) { SetAtAny(nIndex,value); }
	// Description:
	//    Get the value of a memeber varible at the specified index
	template <class T> bool GetAt( int nIndex,T &value )
	{
		ScriptAnyValue any( value,0 );
		return GetAtAny(nIndex,any) && any.CopyTo(value);
	}
	bool HaveAt( int elem )
	{
		ScriptAnyValue any;
		GetAtAny(elem, any);
		return any.type != ANY_TNIL;
	}
	// Description:
	//    Set the value of a member variable to nil at the specified index
	void SetNullAt( int nIndex ) { SetAtAny(nIndex,ScriptAnyValue(ANY_TNIL)); }

	// Description, adds value at next available index.
	template <class T> void PushBack( const T &value )
	{
		int nNextPos = Count() + 1;
		SetAtAny(nNextPos,value);
	}

	//////////////////////////////////////////////////////////////////////////
	// Iteration over table parameters.
	//////////////////////////////////////////////////////////////////////////
	struct Iterator
	{
		const char *sKey;
		int nKey;
		ScriptAnyValue value;
		int nInternal;
	};

	virtual Iterator BeginIteration() = 0;
	virtual bool MoveNext( Iterator &iter ) = 0;
	virtual void EndIteration( const Iterator &iter ) = 0;

	// Clears the table,removes all the entries in the table.
	virtual void Clear() = 0;
	//! get the count of elements into the object
	virtual int Count()=0 ;

  // Description:
	//    Produce a copy of the src table.
	// Arguments
	//    pSrcTable - Source table to clone from.
	//    bDeepCopy - Defines if source table is cloned recursively or not,
	//                 if bDeepCopy is false Only does shallow copy (no deep copy, table entries are not cloned hierarchically).
	//                 If bDeepCopy is true, all sub tables are also cloned recursively.
	virtual bool Clone( IScriptTable *pSrcTable,bool bDeepCopy=false )=0;

	// Description:
	//    Dump all table entries to the IScriptTableDumpSink interface.
	virtual void Dump(IScriptTableDumpSink *p) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Structure that describe user data function.
	//////////////////////////////////////////////////////////////////////////
	struct SUserFunctionDesc
	{
		const char *sFunctionName;    // Name of function.
		const char *sFunctionParams;  // List of parameters (ex "nSlot,vDirection" ).
		const char *sGlobalName;      // Name of global table (ex "System")
		FunctionFunctor pFunctor;     // Pointer to simple function.
		int   nParamIdOffset;         // Offset of the parameter to accept as 1st function argument.
		// Pointer to function with associated data buffer.
		UserDataFunction pUserDataFunc;
		void *pDataBuffer;    // Pointer to the data buffer associated to the user data function.
		int   nDataSize;      // Size of data associated with user data function.

		// Constructor that initialize all data memmbers to initial state.
		SUserFunctionDesc() : sFunctionName(""),sFunctionParams(""),sGlobalName(""),nParamIdOffset(0),pUserDataFunc(0),pDataBuffer(0),nDataSize(0) {}
	};

	// Description:
	//    Add a C++ callback function to the table.
	//    The function a standart function that returns number of arguments and accept IFunctionHandler as argument.
	virtual bool AddFunction( const SUserFunctionDesc &fd ) = 0;

	//! @param szPath e.g. "cnt.table1.table2", "", "mytable", max 255 characters
	//! @return true=path was valid, false otherwise
	virtual bool GetValueRecursive( const char *szPath, IScriptTable *pObj ) = 0;
};


////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// Description:
//   This interface is used by the C++ function mapped to the script
//   to retrieve the function parameters passed by the script and
//   to return an optiona result value to the script.
struct IFunctionHandler
{
	// Return pointer to the script system.
	virtual IScriptSystem* GetIScriptSystem() = 0;

	// Get This pointer of table which called C++ method.
	// This pointer is assigned to key "__this" in the table.
	virtual void* GetThis() = 0;

	// Description.
	//    Retrieve the value of the self passed when calling the table (Always the 1st argument of the function).
	virtual bool GetSelfAny( ScriptAnyValue &any ) = 0;

	// Description:
	//    Retrieve the value of the self passed when calling the table (Always the 1st argument of the function).
	template <class T>
	bool GetSelf( T &value )
	{
		ScriptAnyValue any(value,0);
		return GetSelfAny(any) && any.CopyTo(value);
	}

	// Description:
	//     Returns function name of the curently called function
	//     Use this only used for error reporting.
	virtual const char* GetFuncName() = 0;

	// Description:
	//    Get the number of parameter at specified index passed by the Lua.
	virtual int GetParamCount() = 0;

	// Description:
	//    Get the type of the parameter at specified index passed by the Lua.
	virtual ScriptVarType GetParamType( int nIdx ) = 0;

	// Description:
	//    Get the nIdx param passed by the script.
	//    @param nIdx 1-based index of the parameter
	//    @param val reference to the C++ variable that will store the value
	virtual bool GetParamAny( int nIdx,ScriptAnyValue &any ) = 0;

	// Description:
	//    Get the nIdx param passed by the script.
	//    @param nIdx 1-based index of the parameter
	//    @param val reference to the C++ variable that will store the value
	template <class T>
	bool GetParam( int nIdx,T &value )
	{
		ScriptAnyValue any(value,0);
		return GetParamAny(nIdx,any) && any.CopyTo(value);
	}

	// Description:
	//
	virtual int EndFunctionAny( const ScriptAnyValue &any ) = 0;
	virtual int EndFunctionAny( const ScriptAnyValue &any1,const ScriptAnyValue &any2 ) = 0;
	virtual int EndFunctionAny( const ScriptAnyValue &any1,const ScriptAnyValue &any2, const ScriptAnyValue &any3) = 0;
	virtual int EndFunction() = 0;

	template <class T>
	int EndFunction( const T &value ) { return EndFunctionAny(value); }
	template <class T1,class T2>
	int EndFunction( const T1 &value1,const T2 &value2 ) { return EndFunctionAny(value1,value2); }
	template <class T1,class T2,class T3>
	int EndFunction( const T1 &value1,const T2 &value2,const T3 &value3 ) { return EndFunctionAny(value1,value2,value3); }
	int EndFunctionNull() { return EndFunction(); }

	//////////////////////////////////////////////////////////////////////////
	// Template methods to get multiple parameters.
	//////////////////////////////////////////////////////////////////////////
	template <class P1>
	bool GetParams( P1 &p1 )
	{
		if (!GetParam(1,p1)) return false;
		return true;
	}
	template <class P1,class P2>
	bool GetParams( P1 &p1,P2 &p2 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2)) return false;
		return true;
	}
	template <class P1,class P2,class P3>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5,P6 &p6 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5) || !GetParam(6,p6)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6,class P7>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5,P6 &p6,P7 &p7 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5) || !GetParam(6,p6) || !GetParam(7,p7)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6,class P7,class P8>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5,P6 &p6,P7 &p7,P8 &p8 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5) || !GetParam(6,p6) || !GetParam(7,p7) || !GetParam(8,p8)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6,class P7,class P8,class P9>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5,P6 &p6,P7 &p7,P8 &p8,P9 &p9 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5) || !GetParam(6,p6) || !GetParam(7,p7) || !GetParam(8,p8)) return false;
		if (!GetParam(9,p9)) return false;
		return true;
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6,class P7,class P8,class P9,class P10>
	bool GetParams( P1 &p1,P2 &p2,P3 &p3,P4 &p4,P5 &p5,P6 &p6,P7 &p7,P8 &p8,P9 &p9,P10 &p10 )
	{
		if (!GetParam(1,p1) || !GetParam(2,p2) || !GetParam(3,p3) || !GetParam(4,p4)) return false;
		if (!GetParam(5,p5) || !GetParam(6,p6) || !GetParam(7,p7) || !GetParam(8,p8)) return false;
		if (!GetParam(9,p9) || !GetParam(10,p10)) return false;
		return true;
	}
};

/////////////////////////////////////////////////////////////////////////////
//Utility classes
/////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Helper for faster Set/Gets on the table.
//////////////////////////////////////////////////////////////////////////
class CScriptSetGetChain
{
	IScriptTable* m_pTable;

public:
	CScriptSetGetChain(IScriptTable* pTable) : m_pTable(pTable)
	{
		m_pTable->BeginSetGetChain();
	}

	~CScriptSetGetChain()
	{
		m_pTable->EndSetGetChain();
	}

	void SetToNull(const char* key)
	{
		m_pTable->SetToNull(key);
	}

	template<typename T>
	void SetValue(const char* key, const T& value)
	{
		m_pTable->SetValueChain(key, value);
	}

	template<typename T>
	bool GetValue(const char* key, T& value)
	{
		return m_pTable->GetValueChain(key, value);
	}
};

//////////////////////////////////////////////////////////////////////////
// Helper template class.
// Handles dispatching of Lua to C script function callbacks to the specified
// member functions of the CScriptableBase.
// See Also: CScriptableBase::RegisterTemplateFunction
//////////////////////////////////////////////////////////////////////////
struct ScriptTemplateCallHelper
{
	template <typename Callee>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*), IFunctionHandler *pH )
	{
		return (callee->*func)(pH);
	}
	template <typename Callee, typename P1>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1), IFunctionHandler *pH )
	{
		P1 p1;
		if (!pH->GetParams(p1))
			return pH->EndFunction();
		return  (callee->*func)( pH,p1 );
	}
	template <typename Callee, typename P1, typename P2>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2), IFunctionHandler *pH )
	{
		P1 p1; P2 p2;
		if (!pH->GetParams(p1,p2))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2 );
	}
	template <typename Callee, typename P1, typename P2, typename P3>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3;
		if (!pH->GetParams(p1,p2,p3))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4;
		if (!pH->GetParams(p1,p2,p3,p4))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5;
		if (!pH->GetParams(p1,p2,p3,p4,p5))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5, typename P6>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5, P6), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6;
		if (!pH->GetParams(p1,p2,p3,p4,p5,p6))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5,p6 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5, typename P6, typename P7>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5, P6, P7), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7;
		if (!pH->GetParams(p1,p2,p3,p4,p5,p6,p7))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5,p6,p7 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5, typename P6, typename P7, typename P8>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5, P6, P7, P8), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7; P8 p8;
		if (!pH->GetParams(p1,p2,p3,p4,p5,p6,p7,p8))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5,p6,p7,p8 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5, typename P6, typename P7, typename P8, typename P9>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5, P6, P7, P8, P9), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7; P8 p8; P9 p9;
		if (!pH->GetParams(p1,p2,p3,p4,p5,p6,p7,p8,p9))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5,p6,p7,p8,p9 );
	}
	template <typename Callee, typename P1, typename P2, typename P3,typename P4, typename P5, typename P6, typename P7, typename P8, typename P9, typename P10>
	static int Call(Callee* callee, int (Callee::*func)(IFunctionHandler*,P1, P2, P3, P4, P5, P6, P7, P8, P9, P10), IFunctionHandler *pH )
	{
		P1 p1; P2 p2; P3 p3; P4 p4; P5 p5; P6 p6; P7 p7; P8 p8; P9 p9; P10 p10;
		if (!pH->GetParams(p1,p2,p3,p4,p5,p6,p7,p8,p9,p10))
			return pH->EndFunction();
		return (callee->*func)( pH,p1,p2,p3,p4,p5,p6,p7,p8,p9,p10 );
	}

	template <typename Callee, typename Func>
	struct CallDispatcher
	{
		static int Dispatch( IFunctionHandler *pH,void *pBuffer,int nSize )
		{
			unsigned char* buffer = (unsigned char*)pBuffer;
			return ScriptTemplateCallHelper::Call(*(Callee**)buffer,*(Func*)(buffer+sizeof(Callee*)),pH );
		}
	};
};

//////////////////////////////////////////////////////////////////////////
// Classes that want to register functions to the script must derive from this interface.
//////////////////////////////////////////////////////////////////////////
class CScriptableBase
{
public:
	virtual ~CScriptableBase() { Done(); };
	virtual void Init( IScriptSystem *pSS,ISystem *pSystem,int nParamIdOffset=0 )
	{
		m_pSS = pSS;
		m_pMethodsTable = m_pSS->CreateTable();
		m_pMethodsTable->AddRef();
		m_nParamIdOffset = nParamIdOffset;
	}
	virtual void Done()
	{
		if (m_pSS && *m_sGlobalName)
		{
			m_pSS->SetGlobalToNull(m_sGlobalName);
		}

		if (m_pMethodsTable)
		{
			m_pMethodsTable->Release();
			m_pMethodsTable = nullptr;
		}
	}
	virtual void GetMemoryStatistics(ICrySizer *pSizer) {};

	void SetGlobalName( const char *sGlobalName )
	{
		assert( strlen(sGlobalName) < sizeof(m_sGlobalName) );
		strcpy_s( m_sGlobalName,sGlobalName );
		if (m_pMethodsTable)
			m_pSS->SetGlobalValue( sGlobalName,m_pMethodsTable );
	}

	IScriptTable *GetMethodsTable() { return m_pMethodsTable; };

protected:
	CScriptableBase() { m_pSS = NULL; m_pMethodsTable = NULL; m_sGlobalName[0] = 0; }

	void RegisterGlobal( const char *sName,int nValue )
	{
		m_pSS->SetGlobalValue(sName,nValue);
	}

	void RegisterFunction( const char *sFuncName,IScriptTable::FunctionFunctor function )
	{
		if (m_pMethodsTable)
		{
			IScriptTable::SUserFunctionDesc fd;
			fd.sGlobalName = m_sGlobalName;
			fd.sFunctionName = sFuncName;
			fd.pFunctor = function;
			fd.nParamIdOffset = m_nParamIdOffset;
			m_pMethodsTable->AddFunction( fd );
		}
	}
	template <typename Callee, typename Func>
	void RegisterTemplateFunction( const char *sFuncName,const char *sFuncParams,Callee& callee,const Func &func )
	{
		typedef Callee* Callee_pointer;
		if (m_pMethodsTable)
		{
			Callee_pointer pCalleePtr = &callee;
			unsigned char pBuffer[sizeof(Callee_pointer)+sizeof(func)];
			memcpy( pBuffer,&pCalleePtr,sizeof(Callee_pointer) );
			memcpy( pBuffer+sizeof(Callee_pointer),&func,sizeof(func) );

			IScriptTable::SUserFunctionDesc fd;
			fd.sGlobalName = m_sGlobalName;
			fd.sFunctionName = sFuncName;
			fd.sFunctionParams = sFuncParams;
			fd.pUserDataFunc = ScriptTemplateCallHelper::CallDispatcher<Callee,Func>::Dispatch;
			fd.nDataSize = sizeof(Callee_pointer) + sizeof(func);
			fd.pDataBuffer = pBuffer;
			fd.nParamIdOffset = m_nParamIdOffset;

			m_pMethodsTable->AddFunction( fd );
		}
	}

protected:
	//////////////////////////////////////////////////////////////////////////
	char m_sGlobalName[64];
	IScriptTable *m_pMethodsTable;
	IScriptSystem *m_pSS;
	int m_nParamIdOffset;
	//////////////////////////////////////////////////////////////////////////
};

#define SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_FUNC(func) RegisterFunction( #func,functor_ret(*this,SCRIPT_REG_CLASSNAME func) );
#define SCRIPT_REG_GLOBAL(var) RegisterGlobal( #var,var );
#define SCRIPT_REG_TEMPLFUNC(func,sFuncParams) RegisterTemplateFunction( #func,sFuncParams,*this,SCRIPT_REG_CLASSNAME func );

#define SCRIPT_CHECK_PARAMETERS(_n) \
	if (pH->GetParamCount() != _n) \
{ gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,0,nullptr,"[%s] %d arguments passed, " #_n " expected)", __FUNCTION__,pH->GetParamCount()); \
	return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS_MIN(_n) \
	if (pH->GetParamCount() < _n) \
{ gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,0,nullptr,"[%s] %d arguments passed, at least " #_n " expected)", __FUNCTION__,pH->GetParamCount()); \
	return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS2(_n, _n2) \
	if ((pH->GetParamCount() != _n) && (pH->GetParamCount() != _n2)) \
{ gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,0,nullptr,"[%s] %d arguments passed, " #_n " or " #_n2 " expected)", __FUNCTION__,pH->GetParamCount()); \
	return pH->EndFunction(); }

#define SCRIPT_CHECK_PARAMETERS3(_n, _n2, _n3) \
	if ((pH->GetParamCount() != _n) && (pH->GetParamCount() != _n2) && (pH->GetParamCount() != _n3)) \
{ gEnv->pSystem->Warning(VALIDATOR_MODULE_SCRIPTSYSTEM,VALIDATOR_WARNING,0,nullptr,"[%s] %d arguments passed, " #_n ", " #_n2 " or " #_n3 " expected)", __FUNCTION__,pH->GetParamCount()); \
	return pH->EndFunction(); }

//////////////////////////////////////////////////////////////////////////
// Auto Wrapper on IScriptTable interface, will destroy table when exiting scope.
//////////////////////////////////////////////////////////////////////////
class SmartScriptTable
{
	IScriptTable *m_pTable = nullptr;

public:
	SmartScriptTable() = default;

	explicit SmartScriptTable(IScriptSystem *pSS, bool empty = false)
	{
		m_pTable = pSS->CreateTable(empty);

		if (m_pTable)
			m_pTable->AddRef();
	}

	SmartScriptTable(const SmartScriptTable & other)
	{
		m_pTable = other.m_pTable;

		if (m_pTable)
			m_pTable->AddRef();
	}

	SmartScriptTable(IScriptTable *pTable)
	{
		m_pTable = pTable;

		if (m_pTable)
			m_pTable->AddRef();
	}

	SmartScriptTable & operator=(const SmartScriptTable & other)
	{
		if (m_pTable != other.m_pTable)
		{
			if (m_pTable)
				m_pTable->Release();

			m_pTable = other.m_pTable;

			if (m_pTable)
				m_pTable->AddRef();
		}

		return *this;
	}

	SmartScriptTable & operator=(IScriptTable *pTable)
	{
		if (m_pTable != pTable)
		{
			if (m_pTable)
				m_pTable->Release();

			m_pTable = pTable;

			if (m_pTable)
				m_pTable->AddRef();
		}

		return *this;
	}

	~SmartScriptTable()
	{
		if (m_pTable)
			m_pTable->Release();
	}

	//////////////////////////////////////////////////////////////////////////
	// Casts
	//////////////////////////////////////////////////////////////////////////
	IScriptTable *operator->() const { return m_pTable; }
	IScriptTable *operator*() const { return m_pTable; }
	operator IScriptTable*() const { return m_pTable; }
	explicit operator bool() const { return (m_pTable != nullptr); }

	//////////////////////////////////////////////////////////////////////////
	// Boolean comparasions.
	//////////////////////////////////////////////////////////////////////////
	bool operator ==(const IScriptTable* p2) const { return m_pTable == p2; };
	bool operator !=(const IScriptTable* p2) const { return m_pTable != p2; };
	bool operator < (const IScriptTable* p2) const { return m_pTable < p2; };
	bool operator > (const IScriptTable* p2) const { return m_pTable > p2; };

	//////////////////////////////////////////////////////////////////////////
	IScriptTable* GetPtr() const { return m_pTable; }
	//////////////////////////////////////////////////////////////////////////

	bool Create(IScriptSystem *pSS, bool empty = false)
	{
		if (m_pTable)
			m_pTable->Release();

		m_pTable = pSS->CreateTable(empty);

		if (m_pTable)
		{
			m_pTable->AddRef();
			return true;
		}
		else
		{
			return false;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
// Smart wrapper on top of script function handle.
//////////////////////////////////////////////////////////////////////////
class SmartScriptFunction
{
	HSCRIPTFUNCTION m_hFunc = nullptr;
	IScriptSystem *m_pSS = nullptr;

public:
	SmartScriptFunction() = default;

	explicit SmartScriptFunction(IScriptSystem *pSS, HSCRIPTFUNCTION hFunc = nullptr)
	: m_hFunc(hFunc),
	  m_pSS(pSS)
	{
	}

	SmartScriptFunction(const SmartScriptFunction &) = delete;

	SmartScriptFunction & operator=(const SmartScriptFunction &) = delete;

	SmartScriptFunction(SmartScriptFunction&& other) noexcept : m_hFunc(other.m_hFunc), m_pSS(other.m_pSS)
	{
		other.m_hFunc = nullptr;
		other.m_pSS = nullptr;
	}

	SmartScriptFunction& operator=(SmartScriptFunction&& other) noexcept
	{
		if (m_hFunc)
		{
			m_pSS->ReleaseFunc(m_hFunc);

			m_hFunc = nullptr;
			m_pSS = nullptr;
		}

		std::swap(m_hFunc, other.m_hFunc);
		std::swap(m_pSS, other.m_pSS);

		return *this;
	}

	~SmartScriptFunction()
	{
		if (m_hFunc)
			m_pSS->ReleaseFunc(m_hFunc);
	}

	operator HSCRIPTFUNCTION() const
	{
		return m_hFunc;
	}
};

//////////////////////////////////////////////////////////////////////////
// Script call helper.
struct Script
{
	static SmartScriptTable GetCachedTable( IFunctionHandler * pH, int funcParam )
	{
		SmartScriptTable out;
		if (pH->GetParamCount() >= funcParam && pH->GetParamType(funcParam)==svtObject)
			pH->GetParam(funcParam, out);
		if (!out.GetPtr())
			out = SmartScriptTable(pH->GetIScriptSystem());
		return out;
	}

	static SmartScriptTable SetCachedVector( const Vec3& value, IFunctionHandler * pH, int funcParam )
	{
		SmartScriptTable out = GetCachedTable( pH, funcParam );
		{
			CScriptSetGetChain chain(out);
			chain.SetValue( "x", value.x );
			chain.SetValue( "y", value.y );
			chain.SetValue( "z", value.z );
		}
		return out;
	}

	//////////////////////////////////////////////////////////////////////////
	static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func )
	{
		if (!pSS->BeginCall(func)) return false;
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1>
	static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1 )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2>
	static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2 )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3>
	static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3 )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class P4>
	static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4 )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3,p4);
		return pSS->EndCall();
	}

	template <class P1,class P2,class P3,class P4, class P5>
		static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4, const P5 &p5)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3,p4,p5);
		return pSS->EndCall();
	}

	template <class P1,class P2,class P3,class P4, class P5, class P6>
		static bool Call( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4, const P5 &p5, const P6 &p6)
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3,p4,p5,p6);
		return pSS->EndCall();
	}

	//////////////////////////////////////////////////////////////////////////
	// Call to table.
	//////////////////////////////////////////////////////////////////////////
	// Arguments:
	//   pTable - must not be 0
	static bool CallMethod( IScriptTable *pTable,const char* sMethod )
	{
		assert(pTable);
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable,sMethod)) return false;
		PushParams(pSS,pTable);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	// Arguments:
	//   pTable - must not be 0
	template <class P1>
	static bool CallMethod( IScriptTable *pTable,const char* sMethod,const P1 &p1 )
	{
		assert(pTable);
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable,sMethod)) return false;
		PushParams(pSS,pTable,p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2>
	static bool CallMethod( IScriptTable *pTable,const char* sMethod,const P1 &p1,const P2 &p2 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable,sMethod)) return false;
		PushParams(pSS,pTable,p1,p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3>
	static bool CallMethod( IScriptTable *pTable,const char* sMethod,const P1 &p1,const P2 &p2,const P3 &p3 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable,sMethod)) return false;
		PushParams(pSS,pTable,p1,p2,p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class P4>
	static bool CallMethod( IScriptTable *pTable,const char* sMethod,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(pTable,sMethod)) return false;
		PushParams(pSS,pTable,p1,p2,p3,p4);
		return pSS->EndCall();
	}


	//////////////////////////////////////////////////////////////////////////
	// Call to table.
	//////////////////////////////////////////////////////////////////////////
	static bool CallMethod( IScriptTable *pTable,HSCRIPTFUNCTION func)
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,pTable);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1>
		static bool CallMethod( IScriptTable *pTable,HSCRIPTFUNCTION func,const P1 &p1 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,pTable,p1);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2>
		static bool CallMethod( IScriptTable *pTable,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,pTable,p1,p2);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3>
		static bool CallMethod( IScriptTable *pTable,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,pTable,p1,p2,p3);
		return pSS->EndCall();
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class P4>
		static bool CallMethod( IScriptTable *pTable,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4 )
	{
		IScriptSystem *pSS = pTable->GetScriptSystem();
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,pTable,p1,p2,p3,p4);
		return pSS->EndCall();
	}

	//////////////////////////////////////////////////////////////////////////
	template <class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3);
		return pSS->EndCall(ret);
	}
	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class P4,class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3,p4);
		return pSS->EndCall(ret);
	}	//////////////////////////////////////////////////////////////////////////
	template <class P1,class P2,class P3,class P4,class P5,class Ret>
	static bool CallReturn( IScriptSystem *pSS,HSCRIPTFUNCTION func,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4,const P5 &p5,Ret &ret )
	{
		if (!pSS->BeginCall(func)) return false;
		PushParams(pSS,p1,p2,p3,p4,p5);
		return pSS->EndCall(ret);
	}


	//////////////////////////////////////////////////////////////////////////
private:
	template <class P1>
	static void PushParams( IScriptSystem *pSS,const P1 &p1 )
	{
		pSS->PushFuncParam(p1);
	}
	template <class P1,class P2>
		static void PushParams( IScriptSystem *pSS,const P1 &p1,const P2 &p2 )
	{
		pSS->PushFuncParam(p1);	pSS->PushFuncParam(p2);
	}
	template <class P1,class P2,class P3>
		static void PushParams( IScriptSystem *pSS,const P1 &p1,const P2 &p2,const P3 &p3 )
	{
		pSS->PushFuncParam(p1);	pSS->PushFuncParam(p2); pSS->PushFuncParam(p3);
	}
	template <class P1,class P2,class P3,class P4>
	static void PushParams( IScriptSystem *pSS,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4 )
	{
		pSS->PushFuncParam(p1);	pSS->PushFuncParam(p2); pSS->PushFuncParam(p3);	pSS->PushFuncParam(p4);
	}
	template <class P1,class P2,class P3,class P4,class P5>
	static void PushParams( IScriptSystem *pSS,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4,const P5 &p5 )
	{
		pSS->PushFuncParam(p1);	pSS->PushFuncParam(p2); pSS->PushFuncParam(p3);	pSS->PushFuncParam(p4); pSS->PushFuncParam(p5);
	}
	template <class P1,class P2,class P3,class P4,class P5,class P6>
	static void PushParams( IScriptSystem *pSS,const P1 &p1,const P2 &p2,const P3 &p3,const P4 &p4,const P5 &p5,const P6 &p6 )
	{
		pSS->PushFuncParam(p1);	pSS->PushFuncParam(p2); pSS->PushFuncParam(p3);	pSS->PushFuncParam(p4); pSS->PushFuncParam(p5);	pSS->PushFuncParam(p6);
	}
};

//////////////////////////////////////////////////////////////////////////
// After SmartScriptTable defined, now implement ScriptAnyValue constructor for it.
//////////////////////////////////////////////////////////////////////////
inline ScriptAnyValue::ScriptAnyValue(IScriptTable *value)
: type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
};

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable & value)
: type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
};

inline ScriptAnyValue::ScriptAnyValue(IScriptTable *value, int)
: type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
}

inline ScriptAnyValue::ScriptAnyValue(const SmartScriptTable & value, int)
: type(ANY_TTABLE)
{
	table = value;
	if (table)
		table->AddRef();
}

inline bool ScriptAnyValue::CopyTo(IScriptTable *value)
{
	if (type == ANY_TTABLE)
	{
		value = table;
		return true;
	}
	else
	{
		return false;
	}
};

inline bool ScriptAnyValue::CopyTo(SmartScriptTable & value)
{
	if (type == ANY_TTABLE)
	{
		value = table;
		return true;
	}
	else
	{
		return false;
	}
}

inline void ScriptAnyValue::Clear()
{
	if (type == ANY_TTABLE && table)
	{
		table->Release();
	}

	table = nullptr;
	type = ANY_ANY;
}

inline ScriptAnyValue::~ScriptAnyValue()
{
	if (type == ANY_TTABLE && table)
	{
		table->Release();
	}
}

inline ScriptAnyValue::ScriptAnyValue(const ScriptAnyValue & rhs)
: type(rhs.type)
{
	switch (type)
	{
		case ANY_ANY:
			table = nullptr;
			break;
		case ANY_TBOOLEAN:
			b = rhs.b;
			break;
		case ANY_TFUNCTION:
			function = rhs.function;
			break;
		case ANY_THANDLE:
			ptr = rhs.ptr;
			break;
		case ANY_TNIL:
			table = nullptr;
			break;
		case ANY_TNUMBER:
			number = rhs.number;
			break;
		case ANY_TSTRING:
			str = rhs.str;
			break;
		case ANY_TTABLE:
			table = rhs.table;
			if (table)
				table->AddRef();
			break;
		case ANY_TUSERDATA:
			break;
		case ANY_TVECTOR:
			vec3.x = rhs.vec3.x;
			vec3.y = rhs.vec3.y;
			vec3.z = rhs.vec3.z;
			break;
	}
}

inline void ScriptAnyValue::Swap(ScriptAnyValue & value)
{
	char temp[sizeof(ScriptAnyValue)];
	memcpy( temp, this, sizeof(ScriptAnyValue) );
	memcpy( this, &value, sizeof(ScriptAnyValue) );
	memcpy( &value, temp, sizeof(ScriptAnyValue) );
}
