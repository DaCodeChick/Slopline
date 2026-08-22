/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */
/*
Data is formatted like this:

Uint16 count;
struct {						// aligned to 2-byte boundary
	Uint16 id;
	Uint16 size;
	Uint8 data[size];			// + padding byte (if necessary)
} field[count];
*/

#include "UFieldData.h"

struct SFDEntry {
	Uint16 id;
	Uint16 index;				// 0-based
};

struct SFieldData {
	THdl h;						// handle that stores field data
	Uint32 headerSize;			// number of bytes at start of handle to ignore
	TPtr offsetTable;			// array of offsets to the start of each field-header (not including <headerSize>)
	TPtr lookupTable;			// sorted table of ID and index
	Uint32 tableSize;			// number of fields in tables
	Uint32 lastSearchIndex;		// index of last field found
	Uint16 lastSearchID;		// ID of last field found
};

#define REF		((SFieldData *)inRef)

static void _FDBuildTables(SFieldData *ref);
static void _FDSortLookupTable(SFieldData *ref);
static bool _FDSearchLookupTable(SFieldData *ref, Uint16 inID, Uint32& outIndex);
static bool _FDGetFieldOffset(TFieldData inRef, Uint16 inID, Uint32& outOffset);
static bool _FDManualGetFieldOffsetByIndex(TFieldData inRef, Uint16 inIndex, Uint32& outOffset, Uint32& outSize);
static bool _FDManualGetFieldOffset(TFieldData inRef, Uint16 inID, Uint32& outOffset, Uint32& outSize);

#define ALIGN_FIELDS	0		// set to 1 to align fields to 2-byte boundary

/* -------------------------------------------------------------------------- */

TFieldData UFieldData::New(THdl inData, Uint32 inHeaderSize)
{
	SFieldData *ref = (SFieldData *)UMemory::NewClear(sizeof(SFieldData));
	
	ref->h = inData;
	ref->headerSize = inHeaderSize;
	
	return (TFieldData)ref;
}

void UFieldData::Dispose(TFieldData inRef)
{
	if (inRef)
	{
		UMemory::Dispose(REF->h);
		UMemory::Dispose(REF->offsetTable);
		UMemory::Dispose(REF->lookupTable);
		UMemory::Dispose((TPtr)inRef);
	}
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// inIndex is 1-based, returns 0 if not found
Uint16 UFieldData::GetFieldID(TFieldData inRef, Uint16 inIndex)
{
	if (inRef && inIndex)
	{
		if (REF->offsetTable == nil) _FDBuildTables(REF);
		
		if (inIndex <= REF->tableSize)
		{
			return FB(UMemory::ReadWord(REF->h, REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1]));
		}
	}
	
	// not found
	return 0;
}

// index is 1-based, returns 0 if not found
Uint16 UFieldData::GetFieldIndex(TFieldData inRef, Uint16 inID)
{
	if (inRef && inID)
	{
		if (REF->lookupTable == nil) _FDBuildTables(REF);
		
		Uint32 i;
		if (_FDSearchLookupTable(REF, inID, i))
			return ((SFDEntry *)REF->lookupTable)[i].index + 1;
	}
	
	// not found
	return 0;
}

Uint16 UFieldData::GetFieldCount(TFieldData inRef)
{
	if (inRef && REF->h)
	{
		if (REF->offsetTable == nil)
			_FDBuildTables(REF);
		
		// do NOT use size from data handle as this may be incorrect (eg data is only partially transferred)
		return REF->tableSize;
	}
	
	return 0;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

Uint32 UFieldData::ReadFieldByIndex(TFieldData inRef, Uint16 inIndex, Uint32 inOffset, void *outData, Uint32 inMaxSize)
{
	if (inRef && inIndex)
	{
		if (REF->offsetTable == nil) _FDBuildTables(REF);
		
		if (inIndex <= REF->tableSize)
		{
			THdl h = REF->h;
			Uint8 *p;
			StHandleLocker lock(h, (void*&)p);
			
			p += REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1];
			Uint32 s = FB(((Uint16 *)p)[1]);
			p += 4;
			
			if (inOffset < s)
			{
				if (inOffset + inMaxSize > s)
					inMaxSize = s - inOffset;
				
				UMemory::Copy(outData, p + inOffset, inMaxSize);
				
				return inMaxSize;
			}
		}
	}
	
	return 0;
}

Uint32 UFieldData::WriteFieldByIndex(TFieldData inRef, Uint16 inIndex, Uint32 inOffset, const void *inData, Uint32 inDataSize)
{
	if (inRef && inIndex)
	{
		if (REF->offsetTable == nil) _FDBuildTables(REF);
		
		if (inIndex <= REF->tableSize)
		{
			THdl h = REF->h;
			Uint8 *p;
			StHandleLocker lock(h, (void*&)p);
			
			p += REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1];
			Uint32 s = FB(((Uint16 *)p)[1]);
			p += 4;
			
			if (inOffset < s)
			{
				if (inOffset + inDataSize > s)
					inDataSize = s - inOffset;
				
				UMemory::Copy(p + inOffset, inData, inDataSize);
				
				return inDataSize;
			}
		}
	}
	
	return 0;
}

void UFieldData::SetFieldSizeByIndex(TFieldData inRef, Uint16 inIndex, Uint16 inSize)
{
	Uint32 ofs, ns, s, n;
	THdl h;
	
	Require(inRef && inIndex);
	if (REF->offsetTable == nil) _FDBuildTables(REF);
	Require(inIndex <= REF->tableSize);
	
	// determine new size (rounded)
#if ALIGN_FIELDS
	ns = ((Uint32)inSize + 1) & ~1;
#else
	ns = inSize;
#endif

	// get offset to field header
	ofs = REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1];

	// determine existing size (rounded)
	h = REF->h;
	s = FB(UMemory::ReadWord(h, ofs + sizeof(Uint16)));
#if ALIGN_FIELDS
	s += 1;
	s &= ~1;
#endif
	
	// check for no change
	if (ns == s) return;
	
	// replace existing data
	UMemory::Replace(h, ofs + 4, s, nil, ns);

	// store new size
	UMemory::WriteWord(h, ofs + sizeof(Uint16), TB(inSize));
	
	// update offset table
	Uint32 *offsetTab = (Uint32 *)REF->offsetTable;
	offsetTab += inIndex;
	n = REF->tableSize - (inIndex-1);
	if (ns > s)			// if new size is greater than old size
	{
		s = ns - s;		// calc difference
		while (n--)
			*offsetTab++ += s;
	}
	else				// new size is less than old size
	{
		s = s - ns;		// calc difference
		while (n--)
			*offsetTab++ -= s;
	}
}

Uint32 UFieldData::GetFieldSizeByIndex(TFieldData inRef, Uint16 inIndex)
{
	if (inRef && inIndex)
	{
		if (REF->offsetTable == nil) _FDBuildTables(REF);
		
		if (inIndex <= REF->tableSize)
		{
			return FB(UMemory::ReadWord(REF->h, REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1] + sizeof(Uint16)));
		}
	}
	
	return 0;
}

void UFieldData::SetFieldByIndex(TFieldData inRef, Uint16 inIndex, const void *inData, Uint32 inDataSize)
{
	SetFieldSizeByIndex(inRef, inIndex, inDataSize);
	WriteFieldByIndex(inRef, inIndex, 0, inData, inDataSize);
}

Uint32 UFieldData::GetFieldByIndex(TFieldData inRef, Uint16 inIndex, void *outData, Uint32 inMaxSize)
{
	return ReadFieldByIndex(inRef, inIndex, 0, outData, inMaxSize);
}

void UFieldData::DeleteFieldByIndex(TFieldData inRef, Uint16 inIndex)
{
	Uint32 ofs, s;
	
	if (_FDManualGetFieldOffsetByIndex(inRef, inIndex, ofs, s))
	{
		// dump tables
		if (REF->offsetTable)
		{
			UMemory::Dispose(REF->offsetTable);
			UMemory::Dispose(REF->lookupTable);
			REF->offsetTable = nil;
			REF->lookupTable = nil;
			REF->tableSize = 0;
			REF->lastSearchID = 0;
		}
	
		// remove the field
#if ALIGN_FIELDS
		UMemory::Remove(REF->h, ofs, (s + 4 + 1) & ~1);
#else
		UMemory::Remove(REF->h, ofs, s + 4);
#endif

	}
}

/* -------------------------------------------------------------------------- */
#pragma mark -

Uint32 UFieldData::ReadField(TFieldData inRef, Uint16 inID, Uint32 inOffset, void *outData, Uint32 inMaxSize)
{
	Uint32 ofs;
	
	if (_FDGetFieldOffset(inRef, inID, ofs))
	{
		THdl h = REF->h;
		Uint8 *p;
		StHandleLocker lock(h, (void*&)p);
		
		p += ofs;
		Uint32 s = FB(((Uint16 *)p)[1]);
		p += 4;
		
		if (inOffset < s)
		{
			if (inOffset + inMaxSize > s)
				inMaxSize = s - inOffset;
			
			UMemory::Copy(outData, p + inOffset, inMaxSize);
			
			return inMaxSize;
		}
	}
	
	return 0;
}

Uint32 UFieldData::WriteField(TFieldData inRef, Uint16 inID, Uint32 inOffset, const void *inData, Uint32 inDataSize)
{
	Uint32 ofs;
	
	if (_FDGetFieldOffset(inRef, inID, ofs))
	{
		THdl h = REF->h;
		Uint8 *p;
		StHandleLocker lock(h, (void*&)p);
		
		p += ofs;
		Uint32 s = FB(((Uint16 *)p)[1]);
		p += 4;
		
		if (inOffset < s)
		{
			if (inOffset + inDataSize > s)
				inDataSize = s - inOffset;
			
			UMemory::Copy(p + inOffset, inData, inDataSize);
			
			return inDataSize;
		}
	}
	
	return 0;
}

// adds the field if it doesn't exist
void UFieldData::SetFieldSize(TFieldData inRef, Uint16 inID, Uint16 inSize)
{
	Require(inRef && inID);
	
	if (REF->lookupTable == nil) _FDBuildTables(REF);
	
	Uint32 i;
	if (_FDSearchLookupTable(REF, inID, i))
	{
		SetFieldSizeByIndex(inRef, ((SFDEntry *)REF->lookupTable)[i].index + 1, inSize);
	}
	else
	{
		AddField(inRef, inID, nil, inSize);
	}
}

Uint32 UFieldData::GetFieldSize(TFieldData inRef, Uint16 inID)
{
	Uint32 ofs;
	
	if (_FDGetFieldOffset(inRef, inID, ofs))
	{
		return FB(UMemory::ReadWord(REF->h, ofs + sizeof(Uint16)));
	}
		
	return 0;
}

// adds the field if it doesn't exist
void UFieldData::SetField(TFieldData inRef, Uint16 inID, const void *inData, Uint32 inDataSize)
{
	SetFieldSize(inRef, inID, inDataSize);
	WriteField(inRef, inID, 0, inData, inDataSize);
}

void UFieldData::DeleteField(TFieldData inRef, Uint16 inID)
{
	Uint32 ofs, s;
	
	if (_FDManualGetFieldOffset(inRef, inID, ofs, s))
	{
		// dump tables
		if (REF->offsetTable)
		{
			UMemory::Dispose(REF->offsetTable);
			UMemory::Dispose(REF->lookupTable);
			REF->offsetTable = nil;
			REF->lookupTable = nil;
			REF->tableSize = 0;
			REF->lastSearchID = 0;
		}
	
		// remove the field
#if ALIGN_FIELDS
		UMemory::Remove(REF->h, ofs, (s + 4 + 1) & ~1);
#else
		UMemory::Remove(REF->h, ofs, s + 4);
#endif

	}
}

void UFieldData::DeleteAllFields(TFieldData inRef)
{
	Require(inRef);
	
	UMemory::Dispose(REF->h);
	UMemory::Dispose(REF->offsetTable);
	UMemory::Dispose(REF->lookupTable);
	REF->h = nil;
	REF->lookupTable = nil;
	REF->offsetTable = nil;
	REF->tableSize = 0;
	REF->lastSearchID = 0;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UFieldData::AddField(TFieldData inRef, Uint16 inID, const void *inData, Uint32 inDataSize)
{
	THdl h;
	Uint32 s, n;
	Uint8 *p;
	
	Require(inRef && inID);
	
	if (inDataSize)
	{
		if (inDataSize > max_Uint16)
		{
			DebugBreak("UFieldData::AddField() - maximum field size is 65535 bytes");
			Fail(errorType_Misc, error_LimitReached);
		}
		
		h = REF->h;
				
		if (h == nil)
		{
#if ALIGN_FIELDS
			s = (inDataSize + 6 + 1) & ~1;
#else
			s = inDataSize + 6;
#endif

			h = UMemory::NewHandle(REF->headerSize + s);
			StHandleLocker lock(h, (void*&)p);
			p += REF->headerSize;
			
			((Uint16 *)p)[0] = TB((Uint16)1);
			((Uint16 *)p)[1] = TB((Uint16)inID);
			((Uint16 *)p)[2] = TB((Uint16)inDataSize);
			
			if (inData) UMemory::Copy(p + 6, inData, inDataSize);
			
			REF->h = h;
		}
		else
		{
#if ALIGN_FIELDS
			s = (inDataSize + 4 + 1) & ~1;
#else
			s = inDataSize + 4;
#endif

			n = UMemory::GetSize(h);
			UMemory::SetSize(h, n + s);
			
			/*
	#if DEBUG
			Uint32 tempsize = UMemory::GetSize(h);
			Uint32 expectsize = (n + s);
			
			if (tempsize != expectsize)
			{
				DebugBreak("dammit!  tempsize=%lu, expectsize=%lu", tempsize, expectsize);
			}
	#endif
	*/
	
			StHandleLocker lock(h, (void*&)p);

			// add 1 to count
#if CONVERT_INTS
			*(Uint16 *)(p + REF->headerSize) = TB((Uint16)( FB(*(Uint16 *)(p + REF->headerSize)) + 1 ));
#else
			*(Uint16 *)(p + REF->headerSize) += 1;	// add 1 to count
#endif

			p += n;
			((Uint16 *)p)[0] = TB((Uint16)inID);
			((Uint16 *)p)[1] = TB((Uint16)inDataSize);
			
			if (inData) UMemory::Copy(p + 4, inData, inDataSize);
		}
		
		// dump tables (no longer correct because just added an item)
		if (REF->offsetTable)
		{
			UMemory::Dispose(REF->offsetTable);
			UMemory::Dispose(REF->lookupTable);
			REF->lookupTable = nil;
			REF->offsetTable = nil;
			REF->tableSize = 0;
			REF->lastSearchID = 0;
		}
	}
}

void UFieldData::AddInteger(TFieldData inRef, Uint16 inID, Int32 inInteger)
{
	if (inInteger & 0xFFFF0000)						// if needs 4 bytes
	{
		Uint32 ln = TB((Uint32)inInteger);			// convert to network byte order
		AddField(inRef, inID, &ln, sizeof(ln));
	}
	else											// can store in 2 bytes
	{
		Uint16 sn = TB((Uint16)inInteger);			// convert to network byte order
		AddField(inRef, inID, &sn, sizeof(sn));
	}
}

void UFieldData::AddPString(TFieldData inRef, Uint16 inID, const Uint8 inPString[])
{
	if (inPString) AddField(inRef, inID, inPString+1, inPString[0]);
}

void UFieldData::AddCString(TFieldData inRef, Uint16 inID, const Int8 inCString[])
{
	if (inCString)
	{
		const Int8 *s = inCString;
		Uint32 len = 0;
		while (*s++) ++len;
		AddField(inRef, inID, inCString, len);
	}
}

/* -------------------------------------------------------------------------- */
#pragma mark -

Uint32 UFieldData::GetField(TFieldData inRef, Uint16 inID, void *outData, Uint32 inMaxSize)
{
	return ReadField(inRef, inID, 0, outData, inMaxSize);
}

Int32 UFieldData::GetInteger(TFieldData inRef, Uint16 inID)
{
	Uint8 buf[4];
	Uint32 val;

	switch (ReadField(inRef, inID, 0, buf, sizeof(buf)))
	{
		case sizeof(Uint8):
			val = *(Uint8 *)buf;
			break;
		case sizeof(Uint16):
			val = FB(*(Uint16 *)buf);
			break;
		case sizeof(Uint32):
			val = FB(*(Uint32 *)buf);
			break;
		default:
			val = 0;
			break;
	}

	return val;
}

Int32 UFieldData::GetIntegerByIndex(TFieldData inRef, Uint16 inIndex)
{
	Uint8 buf[4];
	Uint32 val;

	switch (ReadFieldByIndex(inRef, inIndex, 0, buf, sizeof(buf)))
	{
		case sizeof(Uint8):
			val = *(Uint8 *)buf;
			break;
		case sizeof(Uint16):
			val = FB(*(Uint16 *)buf);
			break;
		case sizeof(Uint32):
			val = FB(*(Uint32 *)buf);
			break;
		default:
			val = 0;
			break;
	}

	return val;
}

// inBufSize is the number of bytes that outPString points to (including length-byte)
void UFieldData::GetPString(TFieldData inRef, Uint16 inID, Uint8 *outPString, Uint32 inBufSize)
{
	outPString[0] = ReadField(inRef, inID, 0, outPString+1, inBufSize-1);
}

// inBufSize is the number of bytes that outCString points to (including terminate-byte)
void UFieldData::GetCString(TFieldData inRef, Uint16 inID, Int8 *outCString, Uint32 inBufSize)
{
	outCString[ReadField(inRef, inID, 0, outCString, inBufSize-1)] = 0;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// returns false if not found, offset is to field data (not header)
bool UFieldData::GetFieldInfo(TFieldData inRef, Uint16 inID, Uint16& outIndex, Uint32& outSize, Uint32& outOffset)
{
	Uint32 i;

	if (inRef && inID)
	{
		if (REF->lookupTable == nil) _FDBuildTables(REF);
		
		if (_FDSearchLookupTable(REF, inID, i))
		{
			SFDEntry *lookupTab = (SFDEntry *)REF->lookupTable;
			Uint32 *offsetTab = (Uint32 *)REF->offsetTable;
			Uint32 fieldOffset = REF->headerSize + offsetTab[lookupTab[i].index];
			
			outIndex = i + 1;
			outSize = FB(UMemory::ReadWord(REF->h, fieldOffset + sizeof(Uint16)));
			outOffset = fieldOffset + 4;
			
			return true;
		}
	}
	
	// not found
	outIndex = 0;
	outOffset = outSize = 0;
	return false;
}

// inIndex is 1-based, returns false if not found, offset is to field data (not header)
bool UFieldData::GetFieldInfoByIndex(TFieldData inRef, Uint16 inIndex, Uint16& outID, Uint32& outSize, Uint32& outOffset)
{
	if (inRef && inIndex)
	{
		if (REF->offsetTable == nil) _FDBuildTables(REF);
		
		if (inIndex <= REF->tableSize)
		{
			Uint32 fieldOffset = REF->headerSize + ((Uint32 *)REF->offsetTable)[inIndex-1];
			
			Uint32 n = UMemory::ReadLong(REF->h, fieldOffset);
			
			outID = FB( *(Uint16 *)&n );
			outSize = FB( ((Uint16 *)&n)[1] );
			outOffset = fieldOffset + 4;

			return true;
		}
	}
	
	// not found
	outID = 0;
	outOffset = outSize = 0;
	return false;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

THdl UFieldData::GetDataHandle(TFieldData inRef)
{
	Require(inRef);
	
	if (REF->h == nil)
		REF->h = UMemory::NewHandleClear(REF->headerSize + sizeof(Uint16));
	
	return REF->h;
}

THdl UFieldData::DetachDataHandle(TFieldData inRef)
{
	Require(inRef);

	THdl h = REF->h;
	
	if (h == nil)
		h = UMemory::NewHandleClear(REF->headerSize + sizeof(Uint16));
	else
	{
		UMemory::Dispose(REF->offsetTable);
		UMemory::Dispose(REF->lookupTable);
		REF->offsetTable = nil;
		REF->lookupTable = nil;
		REF->tableSize = 0;
		REF->lastSearchID = 0;
		REF->h = nil;
	}

	return h;
}

void UFieldData::SetDataHandle(TFieldData inRef, THdl inHdl, Uint32 inHeaderSize)
{
	Require(inRef && inHdl);
	
	UMemory::Dispose(REF->h);
	UMemory::Dispose(REF->offsetTable);
	UMemory::Dispose(REF->lookupTable);
	REF->offsetTable = nil;
	REF->lookupTable = nil;
	REF->tableSize = 0;
	REF->lastSearchID = 0;
	
	REF->h = inHdl;
	REF->headerSize = inHeaderSize;
}

// call this if you modify the data handle
void UFieldData::DataChanged(TFieldData inRef)
{
	Require(inRef);
	UMemory::Dispose(REF->offsetTable);
	UMemory::Dispose(REF->lookupTable);
	REF->offsetTable = nil;
	REF->lookupTable = nil;
	REF->tableSize = 0;
	REF->lastSearchID = 0;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

static void _FDBuildTables(SFieldData *ref)
{
	if (ref->h == nil) return;

	UMemory::Dispose(ref->offsetTable);
	UMemory::Dispose(ref->lookupTable);
	ref->offsetTable = nil;
	ref->lookupTable = nil;
	ref->tableSize = 0;
	ref->lastSearchID = 0;
	
	try
	{
		Uint32 *offsetTab;
		SFDEntry *lookupTab;
		Uint32 s, i, c, dataSize;
		Uint8 *p, *ep, *sp;
		THdl h;
		
		// get ptr to field data
		h = ref->h;
		dataSize = UMemory::GetSize(h);
		StHandleLocker locker(h, (void*&)p);
		ep = p + dataSize;
		
		// skip header
		s = ref->headerSize;
		if (s)
		{
			p += s;
			if (dataSize <= s)
				dataSize = 0;
			else
				dataSize -= s;
		}
		sp = p;
		
		// get field count
		if (dataSize <= sizeof(Uint16))
			c = 0;
		else
			c = FB(*(Uint16 *)p);
		p += sizeof(Uint16);
		
		// allocate offset table
		s = (c + 1) * sizeof(Uint32);
		ref->offsetTable = UMemory::NewClear(s);
		offsetTab = (Uint32 *)ref->offsetTable;
		
		// allocate lookup table
		if (c)
		{
			s = c * sizeof(SFDEntry);
			ref->lookupTable = UMemory::NewClear(s);
			lookupTab = (SFDEntry *)ref->lookupTable;		
		}
		
		// build offset and lookup tables
		for (i=0; i<c; i++)
		{
			if (p+4 > ep) break;		// stop if we don't have the entire field header
			s = FB(((Uint16 *)p)[1]);	// get size of this fields data
			if (p+4+s > ep) break;		// stop if we don't have the entire field data
			
			offsetTab[i] = p - sp;
			lookupTab[i].id = FB(*(Uint16 *)p);
			lookupTab[i].index = i;

#if ALIGN_FIELDS
			p += (s + 4 + 1) & ~1;		// advance to next field on 2-byte boundary
#else
			p += s + 4;					// advance to next field
#endif
		}
		offsetTab[i] = p - sp;
		
		ref->tableSize = i;
		
		// sort lookup table by ID
		_FDSortLookupTable(ref);
	}
	catch(...)
	{
		// clean up
		UMemory::Dispose(ref->offsetTable);
		UMemory::Dispose(ref->lookupTable);
		ref->offsetTable = nil;
		ref->lookupTable = nil;
		ref->tableSize = 0;
		ref->lastSearchID = 0;
		throw;
	}
}

static void _FDSortLookupTable(SFieldData *ref)
{
	if (ref->lookupTable == nil) return;
	
	Uint32 num_members = ref->tableSize;
	SFDEntry *lookupTab = (SFDEntry *)ref->lookupTable;
	SFDEntry tmp;
	
	Uint32 l, r, j;
	SFDEntry *lp, *rp, *ip, *jp, *kp;
	
	if (num_members < 2)
		return;
	
	r = num_members;
	l = (r / 2) + 1;
	
	lp = lookupTab + (l - 1);
	rp = lookupTab + (r - 1);
	
	for (;;)
	{
		if (l > 1)
		{
			l--;
			lp--;
		}
		else
		{
			tmp = *lp;
			*lp = *rp;
			*rp = tmp;
			
			if (--r == 1)
				return;
			
			rp--;
		}
		
		j = l;
		
		jp = lookupTab + (j - 1);
		
		while (j*2 <= r)
		{
			j *= 2;
			
			ip = jp;
			jp = lookupTab + (j - 1);
			
			if (j < r)
			{
				kp = jp + 1;
				
				if (jp->id < kp->id)
				{
					j++;
					jp = kp;
				}
			}
			
			if (ip->id < jp->id)
			{
				tmp = *ip;
				*ip = *jp;
				*jp = tmp;
			}
			else
				break;
		}
	}
}

// outIndex is 0-based index into lookupTable
static bool _FDSearchLookupTable(SFieldData *ref, Uint16 inID, Uint32& outIndex)
{
	if (inID == 0)
	{
		outIndex = 0;
		return false;
	}

	if (ref->lastSearchID == inID)
	{
		outIndex = ref->lastSearchIndex;
		return true;
	}

	SFDEntry *lookupTab = (SFDEntry *)ref->lookupTable;
	Uint16 id;
	Uint32 l, r, i;
	
	if (lookupTab == nil || ref->tableSize == 0)
	{
		outIndex = 0;
		return false;
	}
	
	id = lookupTab[0].id;
	
	if (inID == id)
	{
		outIndex = 0;
		goto found;
	}
	else if (inID < id)
	{
		outIndex = 0;
		return false;
	}
	
	l = 1;
	r = ref->tableSize - 1;
	
	if (l > r)
	{
		outIndex = 1;
		return false;
	}
	
	while (l <= r)
	{
		i = (l + r) >> 1;
		
		id = lookupTab[i].id;

		if (inID == id)
		{
			outIndex = i;
			goto found;
		}
		else if (inID < id)
			r = i - 1;
		else
			l = i + 1;
	}
	
	if (inID > id)
		i++;
	
	outIndex = i;
	return false;

found:
	ref->lastSearchID = inID;
	ref->lastSearchIndex = outIndex;
	return true;
}

// offset is to field header, includes main header
static bool _FDGetFieldOffset(TFieldData inRef, Uint16 inID, Uint32& outOffset)
{
	if (inRef && inID)
	{
		if (REF->lookupTable == nil) _FDBuildTables(REF);
		
		Uint32 i;
		if (_FDSearchLookupTable(REF, inID, i))
		{
			SFDEntry *lookupTab = (SFDEntry *)REF->lookupTable;
			Uint32 *offsetTab = (Uint32 *)REF->offsetTable;
			
			outOffset = REF->headerSize + offsetTab[lookupTab[i].index];
			return true;
		}
	}
	
	return false;
}

// manual means doesn't use tables, offset is to field header and includes main header, outSize is size of field data
static bool _FDManualGetFieldOffsetByIndex(TFieldData inRef, Uint16 inIndex, Uint32& outOffset, Uint32& outSize)
{
	THdl h;
	Uint8 *p, *ep, *sp;
	Uint32 dataSize, c;
	
	if (inRef && inIndex)
	{
		h = REF->h;
		if (h)
		{
			dataSize = UMemory::GetSize(h);
			if (dataSize >= 6)
			{
				StHandleLocker locker(h, (void*&)sp);
				p = sp;
				ep = p + dataSize;
				
				p += REF->headerSize;
				if (p+2 > ep) return false;
				c = FB( *((Uint16 *)p)++ );
				
				if (inIndex <= c)
				{
					c = inIndex - 1;
					
					while (c--)
					{
						if (p+4 > ep) return false;
				#if ALIGN_FIELDS
						p += ( (Uint32) FB(((Uint16 *)p)[1]) + 4 + 1) & ~1;
				#else
						p += FB(((Uint16 *)p)[1]) + 4;
				#endif
						if (p > ep) return false;
					}
					
					if (p+4 > ep) return false;
					dataSize = outSize = FB(((Uint16 *)p)[1]);
				#if ALIGN_FIELDS
					dataSize += 1;
					dataSize &= ~1;
				#endif
					if (p+4+dataSize > ep) return false;
					
					outOffset = p - sp;
					return true;
				}
			}
		}
	}
	
	// not found
	return false;
}

// manual means doesn't use tables, offset is to field header and includes main header, outSize is size of field data
static bool _FDManualGetFieldOffset(TFieldData inRef, Uint16 inID, Uint32& outOffset, Uint32& outSize)
{
	THdl h;
	Uint8 *p, *ep, *sp;
	Uint32 dataSize, c;
	
	if (inRef && inID)
	{
		h = REF->h;
		if (h)
		{
			dataSize = UMemory::GetSize(h);
			if (dataSize >= 6)
			{
				StHandleLocker locker(h, (void*&)sp);
				p = sp;
				ep = p + dataSize;
				
				p += REF->headerSize;
				if (p+2 > ep) return false;
				c = FB( *((Uint16 *)p)++ );
				
				inID = TB(inID);
				while (c--)
				{
					if (p+4 > ep) return false;
					
					if (*(Uint16 *)p == inID)	// note TB(inID) above
					{
						dataSize = outSize = FB(((Uint16 *)p)[1]);
					#if ALIGN_FIELDS
						dataSize += 1;
						dataSize &= ~1;
					#endif
						if (p+4+dataSize > ep) return false;
						
						outOffset = p - sp;
						return true;
					}
					
			#if ALIGN_FIELDS
					p += ( (Uint32) FB(((Uint16 *)p)[1]) + 4 + 1) & ~1;
			#else
					p += FB(((Uint16 *)p)[1]) + 4;
			#endif
					if (p > ep) return false;
				}
			}
		}
	}
	
	// not found
	return false;
}










