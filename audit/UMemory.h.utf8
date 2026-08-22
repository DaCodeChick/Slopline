/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#pragma once
#include "typedefs.h"

/*
 * Structures
 */

struct SDataPtr {
	void *data;
	Uint32 size;
};

struct SFlattenInfo {
	const SDataPtr *items;	// ptr to array of items
	Uint32 count;			// number of items
	Uint32 options;			// 1 = align items to 4-byte boundary
	Uint32 index;			// start off at 0
	Uint32 offset;			// start off at 0
	void *ref;				// for your own use
};

/*
 * Types
 */

typedef class TPtrObj *TPtr;
typedef class THdlObj *THdl;
/*
 * Memory Manager
 */

class UMemory
{
	public:
		// initialize
		static void Init();
		
		// alloc fixed
		static TPtr New(Uint32 inSize);
		static TPtr New(const void *inData, Uint32 inSize);
		static TPtr NewClear(Uint32 inSize);
		static void Dispose(TPtr inPtr);
		
		// alloc moveable
		static THdl NewHandle(Uint32 inSize = 0, Uint32 inOptions = 0);
		static THdl NewHandle(const void *inData, Uint32 inSize, Uint32 inOptions = 0);
		static THdl NewHandleClear(Uint32 inSize, Uint32 inOptions = 0);
		static THdl Clone(THdl inHdl);
		static void Dispose(THdl inHdl);
		
		// get and set size
		static void SetSize(THdl inHdl, Uint32 inSize);
		static Uint32 GetSize(THdl inHdl);
		static Uint32 Grow(THdl inHdl, Int32 inDelta);
		static TPtr Reallocate(TPtr inPtr, Uint32 inSize);
		static void Reallocate(THdl inHdl, Uint32 inSize);
		static void ReallocateClear(THdl inHdl, Uint32 inSize);

		// locking and discard
		static Uint8 *Lock(THdl inHdl);
		static void Unlock(THdl inHdl);
		static void SetDiscardable(THdl inHdl);
		static bool ClearDiscardable(THdl inHdl);
		
		// operations on handles
		static void Set(THdl inHdl, const void *inData, Uint32 inSize);
		static void Set(THdl inHdl, THdl inSource);
		static void Clear(THdl inHdl);
		static void Append(THdl inHdl, const void *inData, Uint32 inSize);
		static Uint32 Insert(THdl inHdl, Uint32 inOffset, const void *inData, Uint32 inDataSize);
		static Uint32 Remove(THdl inHdl, Uint32 inOffset, Uint32 inSize);
		static Uint32 Replace(THdl inHdl, Uint32 inOffset, Uint32 inExistingSize, const void *inData, Uint32 inDataSize);
		static Uint32 SearchAndReplaceAll(THdl inHdl, Uint32 inOffset, const void *inSearchData, Uint32 inSearchSize, const void *inReplaceData, Uint32 inReplaceSize);
		static Uint32 Read(THdl inHdl, Uint32 inOffset, void *outData, Uint32 inMaxSize);
		static Uint32 Write(THdl inHdl, Uint32 inOffset, const void *inData, Uint32 inDataSize);
		static Uint32 ReadLong(THdl inHdl, Uint32 inOffset);
		static void WriteLong(THdl inHdl, Uint32 inOffset, Uint32 inVal);
		static Uint16 ReadWord(THdl inHdl, Uint32 inOffset);
		static void WriteWord(THdl inHdl, Uint32 inOffset, Uint16 inVal);

		// misc
		static bool IsValid(THdl inHdl);
		static bool IsValid(TPtr inPtr);
		static Uint32 GetUsedSize();
		static bool IsAvailable(Uint32 inSize);

		// read-only data operations
		static bool Equal(const void *inDataA, const void *inDataB, Uint32 inSize);
		static bool EqualPString(const Uint8 *inStrA, const Uint8 *inStrB);
		static Int16 Compare(const void *inDataA, const void *inDataB, Uint32 inSize);
		static Int16 Compare(const void *inDataA, Uint32 inSizeA, const void *inDataB, Uint32 inSizeB);
		static Uint8 *Search(const void *inSearchData, Uint32 inSearchSize, const void *inData, Uint32 inDataSize);
		static Uint8 *SearchByte(Uint8 inByte, const void *inData, Uint32 inSize);
		static Uint8 *SearchByteBackwards(Uint8 inByte, const void *inData, Uint32 inSize);
		static Uint32 *SearchLongArray(Uint32 inLong, const Uint32 *inArray, Uint32 inCount);
		static Uint32 Checksum(const void *inData, Uint32 inDataSize, Uint32 inInit = 0);
		static Uint32 CRC(const void *inData, Uint32 inDataSize, Uint32 inInit = -1);
		static Uint32 AdlerSum(const void *inData, Uint32 inDataSize, Uint32 inInit = 0);
		static bool Match(const void *inData, Uint32 inDataSize, const Int8 *inPattern);
		static bool Token(const void *inData, Uint32 inDataSize, const Int8 *inDelims, Uint32& ioTokenOffset, Uint32& ioTokenSize);
		static Uint32 HexDumpLine(Uint32 inOffset, Uint32 inLineBytes, const void *inData, Uint32 inDataSize, void *outData, Uint32 inMaxSize);
		static THdl HexDump(const void *inData, Uint32 inDataSize, Uint32 inLineBytes);
		static Uint32 HexToData(const void *inHex, Uint32 inHexSize, void *outData, Uint32 inMaxSize);
		
		// modify data operations
		static Uint32 Copy(void *outDest, const void *inSource, Uint32 inSize);
		static void Fill(void *outDest, Uint32 inSize, Uint8 inChar);
		static void Clear(void *outDest, Uint32 inSize);
		static void Swap(void *ioDataA, Uint32 inSizeA, void *ioDataB, Uint32 inSizeB);
		static void Swap(void *ioDataA, void *ioDataB, Uint32 inSize);
		static void Insert(void *ioDest, Uint32 inDestSize, Uint32 inDestOffset, const void *inNewData, Uint32 inNewDataSize);
		static void Move(void *ioDest, Uint32 inSize, Int32 inDelta);
		static void AppendPString(Uint8 *ioDest, const Uint8 *inSource);
		
		// flattening
		static Uint32 Flatten(SFlattenInfo& ioInfo, void *outData, Uint32 inMaxSize);
		static THdl FlattenToHandle(const SFlattenInfo& inInfo);
		static Uint32 GetFlattenSize(const SFlattenInfo& inInfo);

		// packed integers
		static Uint32 PackIntegers(const Uint32 *inIntList, Uint32 inIntCount, void *outData, Uint32 inMaxSize);
		static Uint32 UnpackIntegers(const void *inData, Uint32 inDataSize, Uint32 *outIntList, Uint32 inMaxCount);
};

typedef UMemory UMem;

/*
 * Bit array functions.  <inIndex> is 0-based (first bit is 0) and can be any
 * number but make sure <inData> points to enough bytes.  Bytes and bits are
 * ordered left-to-right regardless of processor.
 */

inline bool GetBit(const void *inData, Uint32 inIndex)
{
	return ( ((Uint8 *)inData)[inIndex >> 3] >> (7 - (inIndex & 7)) ) & 1;	// note (7-(i&7)) can also be written as ((i&7)^7)
}

inline void SetBit(void *ioData, Uint32 inIndex)
{
	((Uint8 *)ioData)[inIndex >> 3] |= (Uint8)1 << (7 - (inIndex & 7));
}

inline void ClearBit(void *ioData, Uint32 inIndex)
{
	((Uint8 *)ioData)[inIndex >> 3] &= ~((Uint8)1 << (7 - (inIndex & 7)));
}

inline void InvertBit(void *ioData, Uint32 inIndex)
{
	((Uint8 *)ioData)[inIndex >> 3] ^= (Uint8)1 << (7 - (inIndex & 7));
}

inline void SetBit(void *ioData, Uint32 inIndex, bool inVal)
{
	if (inVal) SetBit(ioData, inIndex); else ClearBit(ioData, inIndex);
}

/*
 * Memory Error Codes
 */

enum {
	errorType_Memory					= 3,
	memError_Unknown					= 100,
	memError_NotEnough					= memError_Unknown + 1,
	memError_BlockInvalid				= memError_Unknown + 2
};

/*
 * Helpers
 */

#define ClearStruct(S)		UMemory::Clear(&S, sizeof S)

class StHandleLocker
{
	public:
		StHandleLocker(THdl inHdl, void*& outPtr) : mHdl(inHdl)	{	outPtr = UMemory::Lock(inHdl);	}
		~StHandleLocker()										{	UMemory::Unlock(mHdl);			}
	
	private:
		THdl mHdl;
};

class StPtr
{
	public:
		StPtr(Uint32 inSize)						{	mPtr = UMemory::New(inSize);			}
		StPtr(const void *inData, Uint32 inSize)	{	mPtr = UMemory::New(inData, inSize);	}
		~StPtr()									{	UMemory::Dispose(mPtr);					}
		TPtr Detach()								{	TPtr p = mPtr; mPtr = nil; return p;	}
		
		Uint8 *GetPtr() const			{	return (Uint8*)mPtr;	}
		TPtrObj *operator->() const		{	return mPtr;			}
		operator TPtr()					{	return mPtr;			}
		operator void*()				{	return mPtr;			}
		operator Uint8*()				{	return (Uint8*)mPtr;	}
		operator Uint16*()				{	return (Uint16*)mPtr;	}
		operator Uint32*()				{	return (Uint32*)mPtr;	}
		operator Int8*()				{	return (Int8*)mPtr;		}
		operator Int16*()				{	return (Int16*)mPtr;	}
		operator Int32*()				{	return (Int32*)mPtr;	}

	protected:
		TPtr mPtr;
};

class StHdl
{
	public:
		StHdl(Uint32 inSize, Uint32 inOptions = 0)						{	mHdl = UMemory::NewHandle(inSize, inOptions);			}
		StHdl(const void *inData, Uint32 inSize, Uint32 inOptions = 0)	{	mHdl = UMemory::NewHandle(inData, inSize, inOptions);	}
		~StHdl()														{	UMemory::Dispose(mHdl);									}
		THdl Detach()													{	THdl h = mHdl; mHdl = nil; return h;					}
		
		THdlObj *operator->() const		{	return mHdl;	}
		operator THdl()					{	return mHdl;	}
		
	protected:
		THdl mHdl;
};

/*
 * UMemory Object Interface
 */

class TPtrObj
{
	public:
		void operator delete(void *p)	{	UMemory::Dispose((TPtr)p);	}
	protected:
		TPtrObj() {}
};

class THdlObj
{
	public:
		THdl Clone()															{	return UMemory::Clone(this);								}
	
		void SetSize(Uint32 inSize)												{	UMemory::SetSize(this, inSize);								}
		Uint32 GetSize()														{	return UMemory::GetSize(this);								}
		Uint32 Grow(Int32 inDelta)												{	return UMemory::Grow(this, inDelta);						}
		void Reallocate(Uint32 inSize)											{	UMemory::Reallocate(this, inSize);							}
		void ReallocateClear(Uint32 inSize)										{	UMemory::ReallocateClear(this, inSize);						}

		Uint8 *Lock()															{	return UMemory::Lock(this);									}
		void Unlock()															{	UMemory::Unlock(this);										}
		void SetDiscardable()													{	UMemory::SetDiscardable(this);								}
		bool ClearDiscardable()													{	return UMemory::ClearDiscardable(this);						}

		void Set(const void *inData, Uint32 inSize)								{	UMemory::Set(this, inData, inSize);							}
		void Set(THdl inSource)													{	UMemory::Set(this, inSource);								}
		void Clear()															{	UMemory::Clear(this);										}
		void Append(const void *inData, Uint32 inSize)							{	UMemory::Append(this, inData, inSize);						}
		Uint32 Insert(Uint32 inOffset, const void *inData, Uint32 inDataSize)	{	return UMemory::Insert(this, inOffset, inData, inDataSize);	}
		Uint32 Remove(Uint32 inOffset, Uint32 inSize)							{	return UMemory::Remove(this, inOffset, inSize);				}
		Uint32 Replace(Uint32 inOffset, Uint32 inExistingSize, const void *inData, Uint32 inDataSize)												{	return UMemory::Replace(this, inOffset, inExistingSize, inData, inDataSize);									}
		Uint32 SearchAndReplaceAll(Uint32 inOffset, const void *inSearchData, Uint32 inSearchSize, const void *inReplaceData, Uint32 inReplaceSize)	{	return UMemory::SearchAndReplaceAll(this, inOffset, inSearchData, inSearchSize, inReplaceData, inReplaceSize);	}
		Uint32 Read(Uint32 inOffset, void *outData, Uint32 inMaxSize)			{	return UMemory::Read(this, inOffset, outData, inMaxSize);	}
		Uint32 Write(Uint32 inOffset, const void *inData, Uint32 inDataSize)	{	return UMemory::Write(this, inOffset, inData, inDataSize);	}
		Uint32 ReadLong(Uint32 inOffset)										{	return UMemory::ReadLong(this, inOffset);					}
		void WriteLong(Uint32 inOffset, Uint32 inVal)							{	UMemory::WriteLong(this, inOffset, inVal);					}
		Uint16 ReadWord(Uint32 inOffset)										{	return UMemory::ReadWord(this, inOffset);					}
		void WriteWord(Uint32 inOffset, Uint16 inVal)							{	UMemory::WriteWord(this, inOffset, inVal);					}

		bool IsValid()															{	return UMemory::IsValid(this);								}

		void operator delete(void *p)											{	UMemory::Dispose((THdl)p);									}
	protected:
		THdlObj() {}
};

