/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#pragma once
#include "typedefs.h"
#include "MoreTypes.h"

/*
 * Types
 */

typedef class TMessageSysObj *TMessageSys;

// message system
class UMessageSys
{
	public:
		static TMessageSys New();
		static void Dispose(TMessageSys inSys);

		static void SetDefaultProc(TMessageSys inSys, TMessageProc inProc, void *inContext);
		static void GetDefaultProc(TMessageSys inSys, TMessageProc& outProc, void*& outContext);
		
		static void Post(TMessageSys inSys, Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal, TMessageProc inProc = nil, void *inContext = nil, void *inObject = nil);
		static void Replace(TMessageSys inSys, Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal, TMessageProc inProc = nil, void *inContext = nil, void *inObject = nil);
		static void Flush(TMessageSys inSys, TMessageProc inProc, void *inContext, void *inObject = nil);
		static bool Peek(TMessageSys inSys, Uint32 inMsg, void *outData, Uint32& ioDataSize, TMessageProc inProc, void *inContext, void *inObject = nil);

		static bool Execute(TMessageSys inSys, const Uint32 inTypeList[] = nil);
};

// object interface
class TMessageSysObj
{
	public:
		void SetDefaultProc(TMessageProc inProc, void *inContext)		{	UMessageSys::SetDefaultProc(this, inProc, inContext);	}
		void GetDefaultProc(TMessageProc& outProc, void*& outContext)	{	UMessageSys::GetDefaultProc(this, outProc, outContext);	}

		void Post(Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal, TMessageProc inProc = nil, void *inContext = nil, void *inObject = nil)	{	UMessageSys::Post(this, inMsg, inData, inDataSize, inPriority, inProc, inContext, inObject);	}
		void Replace(Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal, TMessageProc inProc = nil, void *inContext = nil, void *inObject = nil)	{	UMessageSys::Replace(this, inMsg, inData, inDataSize, inPriority, inProc, inContext, inObject);	}
		void Flush(TMessageProc inProc, void *inContext, void *inObject = nil)																													{	UMessageSys::Flush(this, inProc, inContext, inObject);											}
		bool Peek(Uint32 inMsg, void *outData, Uint32& ioDataSize, TMessageProc inProc, void *inContext, void *inObject = nil)																	{	return UMessageSys::Peek(this, inMsg, outData, ioDataSize, inProc, inContext, inObject);		}
	
		bool Execute(const Uint32 inTypeList[])							{	return UMessageSys::Execute(this, inTypeList);			}

		void operator delete(void *p)									{	UMessageSys::Dispose((TMessageSys)p);					}
	protected:
		TMessageSysObj() {}		// force creation via UMessageSys
};



