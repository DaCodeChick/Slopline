/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */
/*
UIDVarArray implements an array of variable-sized items each with a unique
identifier (ID).  Thus, items are usually referred to by ID (rather than index)
and the order of the items is not of interest (users of UIDVarArray should not
expect the items to be in any particular order).

If at some stage we do care about the order, it could perhaps be implemented
in a UOrderedIDVarArray.  This would use the same flattened format (as follows)
except that the items could be in any order, not necessarily sorted.

-- Flattened Format --

Uint32 format;				// always 'IVA1'
Uint32 rsvd;				// reserved, set to zero
Uint32 textEncoding;		// used if the items are text
Uint32 itemCount;			// number of items in the array
struct {
	Uint32 id;				// ID of this item
	Uint32 offset;			// offset from end of offset table to items data
} offsetTab[itemCount+1];	// extra entry is for getting the size of the last item
Uint8 data[];				// data for each item stored in same order as table

The items in the offset table must be stored in ascending order of ID.  Also,
ID 0 is not valid.
*/

#include "UIDVarArray.h"

#pragma options align=packed
struct SIVAOffsetTabEntry {
	Uint32 id;
	Uint32 offset;
};
#pragma options align=reset

struct SIDVarArray {
	Uint32 itemCount;
	Uint32 offsetTabAllocCount;		// including extra entry
	SIVAOffsetTabEntry *offsetTab;	// contains one extra entry with last offset (size of data)
	THdl dataHdl;
	Uint32 lastSearchID, lastSearchIndex;
};

#define REF		((SIDVarArray *)inRef)

static bool _IVASearchItemList(TIDVarArray inRef, Uint32 inID, Uint32& outIndex);

/* -------------------------------------------------------------------------- */

TIDVarArray UIDVarArray::New()
{
	return (TIDVarArray)UMemory::NewClear(sizeof(SIDVarArray));
}

void UIDVarArray::Dispose(TIDVarArray inRef)
{
	if (inRef)
	{
		UMemory::Dispose(REF->dataHdl);
		UMemory::Dispose((TPtr)REF->offsetTab);
		UMemory::Dispose((TPtr)inRef);
	}
}

// by default, fails if item already exists
void UIDVarArray::AddItem(TIDVarArray inRef, Uint32 inID, const void *inData, Uint32 inDataSize, Uint32 inOptions)
{
	Require(inRef && inID);

	// check if an item with the specified ID already exists (and get insert index at the same time)
	Uint32 i;
	if (_IVASearchItemList(inRef, inID, i))
	{
		if (inOptions & kReplaceIfExists)
		{
			UIDVarArray::SetItem(inRef, inID, inData, inDataSize);
			return;
		}
		
		Fail(errorType_Misc, error_ItemAlreadyExists);
	}
	
	// expand or allocate offset tab if necessary
	if (REF->offsetTab == nil)
	{
		REF->offsetTab = (SIVAOffsetTabEntry *)UMemory::New(64);
		REF->offsetTabAllocCount = 8;
		REF->offsetTab[0].id = REF->offsetTab[0].offset = 0;
	}
	else if (REF->itemCount+2 > REF->offsetTabAllocCount)	// if tab full not forgetting the extra offset entry
	{
		Uint32 nac = RoundUp8(REF->offsetTabAllocCount + 4);
		REF->offsetTab = (SIVAOffsetTabEntry *)UMemory::Reallocate((TPtr)REF->offsetTab, nac * sizeof(SIVAOffsetTabEntry));
		REF->offsetTabAllocCount = nac;
	}
	
	// insert new items data into the data handle
	SIVAOffsetTabEntry *offsetTab = REF->offsetTab;
	if (REF->dataHdl)
		UMemory::Insert(REF->dataHdl, offsetTab[i].offset, inData, inDataSize);
	else
		REF->dataHdl = UMemory::NewHandle(inData, inDataSize);
	
	// update item count
	REF->itemCount++;

	// update offsets
	SIVAOffsetTabEntry *entry = offsetTab + REF->itemCount;
	Uint32 cd = REF->itemCount - i;
	while (cd--)
	{
		entry->id = entry[-1].id;
		entry->offset = entry[-1].offset + inDataSize;
		entry--;
	}
	
	// store ID for newly inserted item
	offsetTab[i].id = inID;

	// update last search index
	if (REF->lastSearchID && REF->lastSearchIndex >= i) REF->lastSearchIndex++;
}

// does nothing if specified item doesn't exist
void UIDVarArray::RemoveItem(TIDVarArray inRef, Uint32 inID)
{
	Uint32 i;

	if (inRef && _IVASearchItemList(inRef, inID, i))
	{
		// get offset and size of the existing data for the item
		SIVAOffsetTabEntry *offsetTab = REF->offsetTab;
		Uint32 offset = offsetTab[i].offset;
		Uint32 size = offsetTab[i+1].offset - offset;
		
		// remove items data
		UMemory::Remove(REF->dataHdl, offset, size);
		
		// update offsets
		Uint32 cd = REF->itemCount - i;
		REF->itemCount--;
		SIVAOffsetTabEntry *entry = offsetTab + i;
		while (cd--)
		{
			entry->id = entry[1].id;
			entry->offset = entry[1].offset - size;
			entry++;
		}
		
		// update last search index
		if (REF->lastSearchID)
		{
			if (REF->lastSearchIndex == i)
				REF->lastSearchID = 0;
			else if (REF->lastSearchIndex > i)
				REF->lastSearchIndex--;
		}
		
		// if wasting too much space in the offset table, then shrink it
		if (REF->offsetTabAllocCount - REF->itemCount > 32)
		{
			Uint32 nac = RoundUp4(REF->itemCount + 4);
			REF->offsetTab = (SIVAOffsetTabEntry *)UMemory::Reallocate((TPtr)offsetTab, nac * sizeof(SIVAOffsetTabEntry));
			REF->offsetTabAllocCount = nac;
		}
	}
}

Uint32 UIDVarArray::GetItemCount(TIDVarArray inRef)
{
	Require(inRef);
	return REF->itemCount;
}

// returns true if an item with the specified ID exists
bool UIDVarArray::ItemExists(TIDVarArray inRef, Uint32 inID)
{
	Uint32 i;
	return inRef && _IVASearchItemList(inRef, inID, i);
}

// fails if specified item doesn't exist
void UIDVarArray::SetItem(TIDVarArray inRef, Uint32 inID, const void *inData, Uint32 inDataSize)
{
	Require(inRef);

	// find the index of the item with the specified ID (if any)
	Uint32 i;
	if (!_IVASearchItemList(inRef, inID, i)) Fail(errorType_Misc, error_NoSuchItem);
	
	// get offset and size of the existing data for the item
	SIVAOffsetTabEntry *offsetTab = REF->offsetTab;
	Uint32 offset = offsetTab[i].offset;
	Uint32 oldSize = offsetTab[i+1].offset - offset;
	
	// replace the old item data with the new
	UMemory::Replace(REF->dataHdl, offset, oldSize, inData, inDataSize);
	
	// update offsets in table (offsets following the item that changed size need to be updated)
	if (inDataSize != oldSize)		// if size of item changed
	{
		Uint32 n = REF->itemCount + 1;
		Uint32 d;
		i++;
		
		if (inDataSize > oldSize)	// if data was added
		{
			d = inDataSize - oldSize;
			for(; i!=n; i++)
				offsetTab[i].offset += d;
		}
		else						// less data
		{
			d = oldSize - inDataSize;
			for(; i!=n; i++)
				offsetTab[i].offset -= d;
		}
	}
}

// returns 0 if the item doesn't exist or contains no data
Uint32 UIDVarArray::GetItem(TIDVarArray inRef, Uint32 inID, void *outData, Uint32 inMaxSize)
{
	Require(inRef);

	Uint32 i;
	if (_IVASearchItemList(inRef, inID, i))		// if found ID
	{
		Uint32 offset = REF->offsetTab[i].offset;
		Uint32 size = REF->offsetTab[i+1].offset - offset;
		
		return UMemory::Read(REF->dataHdl, offset, outData, min(inMaxSize, size));
	}
	
	// not found
	return 0;
}

// returns 0 if the item doesn't exist or contains no data
Uint32 UIDVarArray::GetItemSize(TIDVarArray inRef, Uint32 inID)
{
	Require(inRef);

	Uint32 i;
	return _IVASearchItemList(inRef, inID, i) ? (REF->offsetTab[i+1].offset - REF->offsetTab[i].offset) : 0;
}

// returns 0 if the item doesn't exist or contains no data or offset too big etc
Uint32 UIDVarArray::ReadItem(TIDVarArray inRef, Uint32 inID, Uint32 inOffset, void *outData, Uint32 inMaxSize)
{
	Require(inRef);

	Uint32 i;
	if (_IVASearchItemList(inRef, inID, i))		// if found ID
	{
		Uint32 offset = REF->offsetTab[i].offset;
		Uint32 size = REF->offsetTab[i+1].offset - offset;
		
		if (inOffset < size)
		{
			if (inOffset + inMaxSize > size) inMaxSize = size - inOffset;
			return UMemory::Read(REF->dataHdl, offset + inOffset, outData, inMaxSize);
		}
	}
	
	// not found or invalid offset or whatever
	return 0;
}

// fails if specified item doesn't exist, doesn't resize the item
Uint32 UIDVarArray::WriteItem(TIDVarArray inRef, Uint32 inID, Uint32 inOffset, const void *inData, Uint32 inDataSize)
{
	Require(inRef);

	// find the index of the item with the specified ID (if any)
	Uint32 i;
	if (!_IVASearchItemList(inRef, inID, i)) Fail(errorType_Misc, error_NoSuchItem);
	
	// get offset and size of the existing data for the item
	SIVAOffsetTabEntry *offsetTab = REF->offsetTab;
	Uint32 offset = offsetTab[i].offset;
	Uint32 size = offsetTab[i+1].offset - offset;
	
	// bring supplied offset etc into range
	if (inOffset >= size) return 0;
	if (inOffset + inDataSize > size) inDataSize = size - inOffset;

	// perform the write
	return UMemory::Write(REF->dataHdl, offset + inOffset, inData, inDataSize);
}

// every call to GetItemPtr() must be matched by a call to ReleaseItemPtr()
// items cannot be added, removed, resized etc while any GetItemPtr() calls are "open" (not matched by ReleaseItemPtr() yet)
// fails if the specified item doesn't exist (in which case, you should NOT call ReleaseItemPtr())
Uint8 *UIDVarArray::GetItemPtr(TIDVarArray inRef, Uint32 inID, Uint32 *outSize)
{
	Require(inRef);
	
	// it's important to fail (eg rather than returning nil) if item not found because otherwise StIDVarArrayPtr could accidentally call ReleaseItemPtr even tho the GetItemPtr call was not successful
	Uint32 i;
	if (!_IVASearchItemList(inRef, inID, i)) Fail(errorType_Misc, error_NoSuchItem);

	Uint32 offset = REF->offsetTab[i].offset;
	if (outSize) *outSize = REF->offsetTab[i+1].offset - offset;

	return UMemory::Lock(REF->dataHdl) + offset;
}

// after this call, the ptr from GetItemPtr() becomes invalid and you must not use it again
void UIDVarArray::ReleaseItemPtr(TIDVarArray inRef, Uint32 /* inID */)
{
	if (inRef) UMemory::Unlock(REF->dataHdl);
}

THdl UIDVarArray::FlattenToHandle(TIDVarArray inRef)
{
	Require(inRef);

	// calculate sizes etc
	Uint32 itemCount = REF->itemCount;
	Uint32 offsetTabSize = (itemCount + 1) * sizeof(SIVAOffsetTabEntry);
	THdl dataHdl = REF->dataHdl;
	Uint32 arrayDataSize = dataHdl ? dataHdl->GetSize() : 0;
	Uint32 s = 16 + offsetTabSize + arrayDataSize;
	
	// allocate handle to store the flattened array
	THdl h = UMemory::NewHandle(s);
	
	// write flattened data to handle
	try
	{
		// get ptr to handle's data
		Uint32 *lp;
		StHandleLocker lock(h, (void*&)lp);
		
		// write header
		lp[0] = TB((Uint32)'IVA1');
		lp[1] = 0;
		lp[2] = 0;
		lp[3] = TB(itemCount);
		lp += 4;
		
		// write offset/id table
		if (itemCount == 0)		// need special case for this since REF->offsetTab is probably nil
		{
			// no items, write just the one extra offset entry
			lp[0] = lp[1] = 0;
			lp += 2;
		}
		else
		{
			UMemory::Copy(lp, REF->offsetTab, offsetTabSize);
		#if CONVERT_INTS
			itemCount++;			// don't forget the extra entry in the offset table
			while (itemCount--)
			{
				*lp++ = TB(*lp);
				*lp++ = TB(*lp);
			}
		#else
			(Uint8 *)lp += offsetTabSize;
		#endif
		}
		
		// write array data
		if (arrayDataSize)
		{
			void *arrayData;
			StHandleLocker(dataHdl, arrayData);
			UMemory::Copy(lp, arrayData, arrayDataSize);
		}
	}
	catch(...)
	{
		// clean up
		UMemory::Dispose(h);
		throw;
	}
	
	// all done, return flattened handle
	return h;
}

TIDVarArray UIDVarArray::Unflatten(const void *inData, Uint32 inDataSize)
{
	enum {
		tabEntrySize	= sizeof(Uint32) * 2,
		headerSize		= sizeof(Uint32) * 4,
		minDataSize		= headerSize + tabEntrySize
	};
	
	THdl dataHdl = nil;
	SIVAOffsetTabEntry *offsetTab = nil;
	SIDVarArray *ref = nil;
	
	// immediately fail if less data than the minimum required
	if (inDataSize < minDataSize) goto corrupt;
	
	// check format tag
	Uint32 *lp = (Uint32 *)inData;
	if (lp[0] != TB((Uint32)'IVA1')) Fail(errorType_Misc, error_FormatUnknown);
	
	// immediately fail on ridiculous item counts to avoid overflow
	Uint32 itemCount = FB(lp[3]);
	if (itemCount & 0xFF000000) goto corrupt;
	
	// check that got enough data for the entire offset table
	Uint32 offsetTabSize = (itemCount + 1) * tabEntrySize;
	Uint32 arrayDataOffset = headerSize + offsetTabSize;
	if (inDataSize < arrayDataOffset) goto corrupt;
	Uint32 arrayDataSize = inDataSize - arrayDataOffset;
	
	// allocate offset table, data handle, and ref
	try
	{
		offsetTab = (SIVAOffsetTabEntry *)UMemory::New(offsetTabSize);
		dataHdl = UMemory::NewHandle(BPTR(inData) + arrayDataOffset, arrayDataSize);
		ref = (SIDVarArray *)UMemory::NewClear(sizeof(SIDVarArray));
	}
	catch(...)
	{
		// clean up and bail out
		UMemory::Dispose(dataHdl);
		UMemory::Dispose((TPtr)offsetTab);
		UMemory::Dispose((TPtr)ref);
		throw;
	}
	
	// prepare to extract and validate the offset table
	(Uint8 *)lp += headerSize;
	Uint32 *dp = (Uint32 *)offsetTab;
	Uint32 prevID = 0;
	Uint32 prevOffset = 0;
	Uint32 cd = itemCount;

	// extract and validate each entry in the offset table
	while (cd--)
	{
		// extract ID and check that sorted and no duplicate IDs
		*dp = FB(*lp);
		if (*dp <= prevID) goto corrupt;
		prevID = *dp;
		
		// extract offset and check that in range
		dp[1] = FB(lp[1]);
		if (dp[1] < prevOffset || dp[1] > arrayDataSize) goto corrupt;
		
		// next entry
		dp += 2;
		lp += 2;
	}

	// extract and validate the extra offset entry
	*dp = 0;
	dp[1] = FB(lp[1]);
	if (dp[1] < prevOffset || dp[1] > arrayDataSize) goto corrupt;
	lp += 2;

	// fill in the IDVarArray ref
	ref->itemCount = itemCount;
	ref->offsetTabAllocCount = itemCount + 1;
	ref->offsetTab = offsetTab;
	ref->dataHdl = dataHdl;

	// all done!
	return (TIDVarArray)ref;

	// corrupt exit point
corrupt:
	UMemory::Dispose(dataHdl);
	UMemory::Dispose((TPtr)offsetTab);
	UMemory::Dispose((TPtr)ref);
	Fail(errorType_Misc, error_Corrupt);
	return nil;
}

// returns 0 if the item doesn't exist or contains no data
Uint32 UIDVarArray::GetItem(const void *inData, Uint32 inDataSize, Uint32 inID, void *outData, Uint32 inMaxSize)
{
	enum {
		tabEntrySize	= sizeof(Uint32) * 2,
		headerSize		= sizeof(Uint32) * 4,
		minDataSize		= headerSize + tabEntrySize
	};
		
	// immediately fail if less data than the minimum required or if format tag is wrong
	if (!inID || (inDataSize < minDataSize) || (*(Uint32 *)inData != TB((Uint32)'IVA1')))
		goto notFound;
	
	// immediately fail on ridiculous item counts to avoid overflow
	Uint32 itemCount = FB( ((Uint32 *)inData)[3] );
	if (!itemCount || (itemCount & 0xFF000000)) goto notFound;
	
	// check that got enough data for the entire offset table
	Uint32 arrayDataOffset = headerSize + ((itemCount + 1) * tabEntrySize);
	if (inDataSize < arrayDataOffset) goto notFound;
	Uint32 arrayDataSize = inDataSize - arrayDataOffset;
	
	/*
	 * To test how well the binary search handles corruption, I ran it on an array of 
	 * random numbers hundreds of thousands of times and it didn't crash or get stuck 
	 * in a infinite loop so I presume it's okay.
	 */

	SIVAOffsetTabEntry *offsetTab = (SIVAOffsetTabEntry *)( BPTR(inData) + headerSize );
	
	Uint32 i;
	Uint32 id = FB(offsetTab[0].id);
	if (inID == id)
	{
		i = 0;
		goto found;
	}
	else if (inID < id)
		goto notFound;
	
	Uint32 l = 1;
	Uint32 r = itemCount - 1;
	
	while (l <= r)
	{
		i = (l + r) >> 1;
		id = FB(offsetTab[i].id);

		if (inID == id)
			goto found;
		else if (inID < id)
			r = i - 1;
		else
			l = i + 1;
	}
	
	// couldn't find item with specified ID or data is corrupt etc
notFound:
	return 0;

	// found an item with specified ID
found:
	Uint32 offset = FB( offsetTab[i].offset );
	Uint32 endOffset = FB( offsetTab[i+1].offset );
	if (offset >= endOffset || endOffset > arrayDataSize) goto notFound;
	
	Uint32 size = endOffset - offset;
	if (inMaxSize > size) inMaxSize = size;
	
	UMemory::Copy(outData, BPTR(inData) + arrayDataOffset + offset, inMaxSize);
	return inMaxSize;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// outIndex is 0-based, returns false if not found (outIndex is still valid)
static bool _IVASearchItemList(TIDVarArray inRef, Uint32 inID, Uint32& outIndex)
{
	SIVAOffsetTabEntry *lookupTab;
	Uint32 id, l, r, i;
	
	r = REF->itemCount;
	
	if (!inID || !r)
	{
		outIndex = 0;
		return false;
	}

	if (inID == REF->lastSearchID)
	{
		outIndex = REF->lastSearchIndex;
		return true;
	}
	
	lookupTab = REF->offsetTab;
	id = lookupTab[0].id;
	
	if (inID == id)
	{
		REF->lastSearchID = inID;
		REF->lastSearchIndex = outIndex = 0;
		return true;
	}
	else if (inID < id)
	{
		outIndex = 0;
		return false;
	}
	
	l = 1;
	r--;	// itemCount - 1
	
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
			REF->lastSearchID = inID;
			REF->lastSearchIndex = outIndex = i;
			return true;
		}
		else if (inID < id)
			r = i - 1;
		else
			l = i + 1;
	}
	
	if (inID > id) i++;
	outIndex = i;
	return false;
}




