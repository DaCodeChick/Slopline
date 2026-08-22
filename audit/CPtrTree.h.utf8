/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#pragma once
#include "typedefs.h"
#include "MoreTypes.h"
#include "UMemory.h"

struct STreeItem {
	Uint16 nItemLevel;	// 1 based
	Uint32 nChildCount;
	void *pItem;
};

template <class T>
class CPtrTree
{
	public:
		CPtrTree();
		~CPtrTree();

		void SetData(TPtr inPtr, Uint32 inTreeCount, Uint32 nRootCount, Uint32 inMaxCount);
		STreeItem *GetArrayPtr() const;
		
		Uint32 AddItem(Uint32 inParentIndex, T *inPtr);
		Uint32 InsertItem(Uint32 inParentIndex, Uint32 inIndex, T *inPtr);
		T *RemoveItem(Uint32 inIndex, bool inRemoveChild = true);
		Uint32 RemoveItem(T *inPtr, bool inRemoveChild = true);
		bool RemoveChildTree(Uint32 inParentIndex);
		bool RemoveChildTree(T *inParentPtr);
		void Clear();

		bool SetItem(Uint32 inIndex, T *inPtr);
		T *GetItem(Uint32 inIndex) const;
		Uint32 GetItemIndex(T *inPtr) const;
		
		bool SetParentItem(Uint32 inChildIndex, T *inPtr);
		T *GetParentItem(Uint32 inChildIndex, Uint32 *outParentIndex = nil) const;
		T *GetParentItem(T *inChildPtr, Uint32 *outParentIndex = nil) const;
		Uint32 GetParentIndex(Uint32 inChildIndex) const;
		Uint32 GetParentIndex(T *inChildPtr) const;
		
		Uint32 GetItemLevel(Uint32 inIndex) const;
		Uint32 GetItemLevel(T *inPtr) const;
		Uint32 GetTreeCount() const;
		Uint32 GetRootCount() const;
		Uint32 GetChildTreeCount(Uint32 inParentIndex) const;
		Uint32 GetChildTreeCount(T *inParentPtr) const;
		
		bool GetNext(T*& ioPtr, Uint32& ioIndex, bool inSameParent = false) const;
		bool GetPrev(T*& ioPtr, Uint32& ioIndex, bool inSameParent = false) const;
		bool IsInList(T *inPtr) const;
		
		void Sort(Uint32 inParentIndex, TComparePtrsProc inProc, void *inRef = nil);

	protected:
		STreeItem *mPtrTree;
		Uint32 mTreeCount, mRootCount, mMaxCount;

		STreeItem *GetTreeItem(T *inPtr, Uint32 *outIndex = nil) const;
};

template<class T> CPtrTree<T>::CPtrTree()
{
	mPtrTree = nil;
	
	mTreeCount = 0;
	mRootCount = 0;
	mMaxCount = 0;
}

template<class T> CPtrTree<T>::~CPtrTree()
{
	if (mPtrTree)
		UMemory::Dispose((TPtr)mPtrTree);
}

template<class T> void CPtrTree<T>::SetData(TPtr inPtr, Uint32 inTreeCount, Uint32 inRootCount, Uint32 inMaxCount)
{
	if (mPtrTree)
		UMemory::Dispose((TPtr)mPtrTree);

	mPtrTree = (STreeItem *)inPtr;
	
	mTreeCount = inTreeCount;
	mRootCount = inRootCount;
	mMaxCount = inMaxCount;
}

template<class T> STreeItem *CPtrTree<T>::GetArrayPtr() const
{
	return mPtrTree;
}

template<class T> Uint32 CPtrTree<T>::AddItem(Uint32 inParentIndex, T *inPtr)
{
	if (inParentIndex > mTreeCount || !inPtr)
		return 0;
	
	Uint32 nIndex = 0;
	Uint16 nParentLevel = 0;
	
	if (mPtrTree)
	{
		if (mTreeCount >= mMaxCount)
		{
			mMaxCount = RoundUp8(mTreeCount + 8);
			mPtrTree = (STreeItem *)UMemory::Reallocate((TPtr)mPtrTree, mMaxCount * sizeof(STreeItem));
		}
		
		if (inParentIndex)
		{
			nParentLevel = mPtrTree[inParentIndex - 1].nItemLevel;
			mPtrTree[inParentIndex - 1].nChildCount++;

			nIndex = inParentIndex;
			while (nIndex < mTreeCount && mPtrTree[nIndex].nItemLevel > nParentLevel)
				nIndex++;
					
			if (nIndex != mTreeCount)
				UMemory::Copy(mPtrTree + nIndex + 1, mPtrTree + nIndex, (mTreeCount - nIndex) *sizeof(STreeItem));
		}
		else
		{
			nIndex = mTreeCount;
			mRootCount++;
		}	
	}
	else
	{
		mMaxCount = 8;
		mPtrTree = (STreeItem *)UMemory::New(mMaxCount * sizeof(STreeItem));
		mRootCount++;
	}

	mPtrTree[nIndex].nItemLevel = nParentLevel + 1;
	mPtrTree[nIndex].nChildCount = 0;
	mPtrTree[nIndex].pItem = inPtr;
		
	mTreeCount++;

	return nIndex + 1;
}

template<class T> Uint32 CPtrTree<T>::InsertItem(Uint32 inParentIndex, Uint32 inIndex, T *inPtr)
{
	if (inParentIndex > mTreeCount || !inPtr)
		return 0;
	
	if (!mPtrTree)
		return AddItem(0, inPtr);
	
	Uint32 nIndex = 0;
	Uint16 nParentLevel = 0;
	
	if (mTreeCount >= mMaxCount)
	{
		mMaxCount = RoundUp8(mTreeCount + 8);
		mPtrTree = (STreeItem *)UMemory::Reallocate((TPtr)mPtrTree, mMaxCount * sizeof(STreeItem));
	}
		
	if (inParentIndex)
	{
		nParentLevel = mPtrTree[inParentIndex - 1].nItemLevel;
		mPtrTree[inParentIndex - 1].nChildCount++;
	}
	else
	{
		mRootCount++;
	}
	
	Uint32 nCount = 0;
	nIndex = inParentIndex;
		
	while (nIndex < mTreeCount && mPtrTree[nIndex].nItemLevel > nParentLevel)
	{
		if (mPtrTree[nIndex].nItemLevel == nParentLevel + 1 && ++nCount >= inIndex)
			break;
			
		nIndex++;
	}
					
	if (nIndex != mTreeCount)
		UMemory::Copy(mPtrTree + nIndex + 1, mPtrTree + nIndex, (mTreeCount - nIndex) *sizeof(STreeItem));
					
	mPtrTree[nIndex].nItemLevel = nParentLevel + 1;
	mPtrTree[nIndex].nChildCount = 0;
	mPtrTree[nIndex].pItem = inPtr;
		
	mTreeCount++;

	return nIndex + 1;
}

// inRemoveChild == true --> remove this item and all his children's 
// inRemoveChild == false --> replace this item with his first child
template<class T> T *CPtrTree<T>::RemoveItem(Uint32 inIndex, bool inRemoveChild)
{
	if (!inIndex || inIndex > mTreeCount)
		return nil;

	T *pItem = (T *)mPtrTree[inIndex - 1].pItem;
	Uint32 nItemLevel = mPtrTree[inIndex - 1].nItemLevel;
	Uint32 nChildCount = mPtrTree[inIndex - 1].nChildCount;
	
	if (inRemoveChild || !nChildCount)
	{
		Uint32 nParentLevel = nItemLevel - 1;

		if (nParentLevel)
		{
			Uint32 nParentIndex = inIndex - 2;
			while (nParentIndex && mPtrTree[nParentIndex].nItemLevel > nParentLevel)
				nParentIndex--;
	
			mPtrTree[nParentIndex].nChildCount--;
		}
		else
		{
			mRootCount--;
		}

		Uint32 nNextIndex = inIndex;
		while (nNextIndex < mTreeCount && mPtrTree[nNextIndex].nItemLevel > nItemLevel)
			nNextIndex++;
	
		if (nNextIndex != mTreeCount)
			UMemory::Copy(mPtrTree + inIndex - 1, mPtrTree + nNextIndex, (mTreeCount - nNextIndex) *sizeof(STreeItem));
	
		mTreeCount -= nNextIndex - inIndex + 1;
	}
	else
	{		
		Uint32 nChildLevel = nItemLevel + 1;
		
		Uint32 nNextIndex = inIndex + 1;
		while (nNextIndex < mTreeCount && mPtrTree[nNextIndex].nItemLevel > nChildLevel)
		{
			mPtrTree[nNextIndex].nItemLevel--;
			nNextIndex++;
		}
		
		mPtrTree[inIndex].nItemLevel = nItemLevel;
		mPtrTree[inIndex].nChildCount += nChildCount - 1;

		if (inIndex != mTreeCount)
			UMemory::Copy(mPtrTree + inIndex - 1, mPtrTree + inIndex, (mTreeCount - inIndex) *sizeof(STreeItem));

		mTreeCount--;
	}
		
	if (!mTreeCount)
	{
		Clear();
	}
	else if (mMaxCount - mTreeCount > 32)
	{
		mMaxCount = RoundUp8(mTreeCount + 8);
		mPtrTree = (STreeItem *)UMemory::Reallocate((TPtr)mPtrTree, mMaxCount * sizeof(STreeItem));
	}
		
	return pItem;
}

// inRemoveChild == true --> remove this item and all his children's 
// inRemoveChild == false --> replace this item with his first child
template<class T> Uint32 CPtrTree<T>::RemoveItem(T *inPtr, bool inRemoveChild)
{
	Uint32 nIndex = 0;
	if (GetTreeItem(inPtr, &nIndex))
		RemoveItem(nIndex, inRemoveChild);
	
	return nIndex;
}

template<class T> bool CPtrTree<T>::RemoveChildTree(Uint32 inParentIndex)
{
	if (!inParentIndex || inParentIndex > mTreeCount)
		return false;

	if (!mPtrTree[inParentIndex - 1].nChildCount)
		return true;
	
	Uint32 nParentLevel = mPtrTree[inParentIndex - 1].nItemLevel;
	mPtrTree[inParentIndex - 1].nChildCount = 0;

	Uint32 nNextIndex = inParentIndex;
	while (nNextIndex < mTreeCount && mPtrTree[nNextIndex].nItemLevel > nParentLevel)
		nNextIndex++;
	
	if (nNextIndex != mTreeCount)
		UMemory::Copy(mPtrTree + inParentIndex, mPtrTree + nNextIndex, (mTreeCount - nNextIndex) *sizeof(STreeItem));
	
	mTreeCount -= nNextIndex - inParentIndex;
	
	if (!mTreeCount)
	{
		Clear();
	}
	else if (mMaxCount - mTreeCount > 32)
	{
		mMaxCount = RoundUp8(mTreeCount + 8);
		mPtrTree = (STreeItem *)UMemory::Reallocate((TPtr)mPtrTree, mMaxCount * sizeof(STreeItem));
	}
		
	return true;
}

template<class T> bool CPtrTree<T>::RemoveChildTree(T *inParentPtr)
{
	Uint32 nParentIndex;
	if (GetTreeItem(inParentPtr, &nParentIndex))
		return RemoveChildTree(nParentIndex);
	
	return false;
}

template<class T> void CPtrTree<T>::Clear()
{
	if (mPtrTree)
	{
		UMemory::Dispose((TPtr)mPtrTree);
		mPtrTree = nil;
	}
	
	mTreeCount = 0;
	mRootCount = 0;
	mMaxCount = 0;
}

template<class T> bool CPtrTree<T>::SetItem(Uint32 inIndex, T *inPtr)
{
	if (!inIndex || inIndex > mTreeCount)
		return false;

	mPtrTree[inIndex - 1].pItem = inPtr;
	
	return true;
}

template<class T> T *CPtrTree<T>::GetItem(Uint32 inIndex) const
{
	if (!inIndex || inIndex > mTreeCount)
		return nil;

	return (T *)mPtrTree[inIndex - 1].pItem;
}

template<class T> Uint32 CPtrTree<T>::GetItemIndex(T *inPtr) const
{
	Uint32 nIndex = 0;
	GetTreeItem(inPtr, &nIndex);
		
	return nIndex;
}

template<class T> bool CPtrTree<T>::SetParentItem(Uint32 inChildIndex, T *inPtr)
{
	if (!inChildIndex || inChildIndex > mTreeCount)
		return false;

	Uint32 nParentLevel = mPtrTree[inChildIndex - 1].nItemLevel - 1;
	if (!nParentLevel)
		return false;
		
	Uint32 nParentIndex = inChildIndex - 2;
	while (nParentIndex && mPtrTree[nParentIndex].nItemLevel > nParentLevel)
		nParentIndex--;
	
	mPtrTree[nParentIndex].pItem = inPtr;
	
	return true;
}

template<class T> T *CPtrTree<T>::GetParentItem(Uint32 inChildIndex, Uint32 *outParentIndex) const
{
	if (!inChildIndex || inChildIndex > mTreeCount)
		return nil;

	Uint32 nParentLevel = mPtrTree[inChildIndex - 1].nItemLevel - 1;
	if (!nParentLevel)
		return nil;
		
	Uint32 nParentIndex = inChildIndex - 2;
	while (nParentIndex && mPtrTree[nParentIndex].nItemLevel > nParentLevel)
		nParentIndex--;
	
	if (outParentIndex)
		*outParentIndex = nParentIndex + 1;
	
	return (T *)mPtrTree[nParentIndex].pItem;
}

template<class T> T *CPtrTree<T>::GetParentItem(T *inChildPtr, Uint32 *outParentIndex) const
{
	Uint32 nChildIndex;
	if (GetTreeItem(inChildPtr, &nChildIndex))
		return GetParentItem(nChildIndex, outParentIndex);
	
	return nil;
}

template<class T> Uint32 CPtrTree<T>::GetParentIndex(Uint32 inChildIndex) const
{
	if (!inChildIndex || inChildIndex > mTreeCount)
		return 0;

	Uint32 nParentLevel = mPtrTree[inChildIndex - 1].nItemLevel - 1;
	if (!nParentLevel)
		return 0;
		
	Uint32 nParentIndex = inChildIndex - 2;
	while (nParentIndex && mPtrTree[nParentIndex].nItemLevel > nParentLevel)
		nParentIndex--;
	
	return nParentIndex + 1;
}

template<class T> Uint32 CPtrTree<T>::GetParentIndex(T *inChildPtr) const
{
	Uint32 nChildIndex;
	if (GetTreeItem(inChildPtr, &nChildIndex))
		return GetParentIndex(nChildIndex);
	
	return 0;
}

template<class T> Uint32 CPtrTree<T>::GetItemLevel(Uint32 inIndex) const
{
	if (!inIndex || inIndex > mTreeCount)
		return 0;

	return mPtrTree[inIndex - 1].nItemLevel;
}

template<class T> Uint32 CPtrTree<T>::GetItemLevel(T *inPtr) const
{
	Uint32 nIndex;
	if (GetTreeItem(inPtr, &nIndex))
		return GetItemLevel(nIndex);
		
	return 0;
}

template<class T> Uint32 CPtrTree<T>::GetTreeCount() const
{	
	return mTreeCount;
}

template<class T> Uint32 CPtrTree<T>::GetRootCount() const
{	
	return mRootCount;
}

template<class T> Uint32 CPtrTree<T>::GetChildTreeCount(Uint32 inParentIndex) const
{
	if (!inParentIndex)
		return mRootCount;

	if (inParentIndex > mTreeCount)
		return 0;
			
	return mPtrTree[inParentIndex - 1].nChildCount;
}

template<class T> Uint32 CPtrTree<T>::GetChildTreeCount(T *inParentPtr) const
{
	if (!inParentPtr)
		return mRootCount;
	
	Uint32 nParentIndex;
	if (GetTreeItem(inParentPtr, &nParentIndex))
		return GetChildTreeCount(nParentIndex);
		
	return 0;
}
		
template<class T> bool CPtrTree<T>::GetNext(T*& ioPtr, Uint32& ioIndex, bool inSameParent) const
{
	if (ioIndex >= mTreeCount)
		return false;
	
	if (inSameParent && ioIndex)
	{			
		Uint32 nIndex = ioIndex;
		Uint32 nItemLevel = mPtrTree[ioIndex - 1].nItemLevel;
		
		while (nIndex < mTreeCount && mPtrTree[nIndex].nItemLevel > nItemLevel)
			nIndex++;
	
		if (nIndex == mTreeCount || mPtrTree[nIndex].nItemLevel != nItemLevel)
			return false;
			
		ioIndex = nIndex + 1;
		ioPtr = (T *)mPtrTree[nIndex].pItem;
		
		return true;	
	}
	
	ioPtr = (T *)mPtrTree[ioIndex].pItem;
	ioIndex++;
		
	return true;
}

template<class T> bool CPtrTree<T>::GetPrev(T*& ioPtr, Uint32& ioIndex, bool inSameParent) const
{
	if (ioIndex <= 1 || ioIndex > mTreeCount + 1)
		return false;
	
	if (inSameParent && ioIndex <= mTreeCount)
	{			
		Uint32 nIndex = ioIndex - 2;
		Uint32 nItemLevel = mPtrTree[ioIndex - 1].nItemLevel;
		
		while (nIndex > 0 && mPtrTree[nIndex].nItemLevel > nItemLevel)
			nIndex--;
	
		if (mPtrTree[nIndex].nItemLevel != nItemLevel)
			return false;
			
		ioIndex = nIndex + 1;
		ioPtr = (T *)mPtrTree[nIndex].pItem;
		
		return true;	
	}
	
	ioIndex--;
	ioPtr = (T *)mPtrTree[ioIndex - 1].pItem;
		
	return true;
}

template<class T> bool CPtrTree<T>::IsInList(T *inPtr) const
{
	if (GetTreeItem(inPtr))
		return true;

	return false;
}

/*
The compare function must return one of the following:

	< 0		item A is less than item B
	0		item A is equal to item B
	> 0		item A is greater than item B
*/
template<class T> void CPtrTree<T>::Sort(Uint32 inParentIndex, TComparePtrsProc inProc, void *inRef)
{
	if (!mRootCount || inParentIndex > mTreeCount)
		return;

	Uint32 nParentLevel = 0;
	if (inParentIndex)
	{
	 	if (!mPtrTree[inParentIndex - 1].nChildCount)
			return;

		nParentLevel = mPtrTree[inParentIndex - 1].nItemLevel;
	}
	
	bool bChanged = false;
	Uint32 nFirstIndex = inParentIndex;
	
	Uint32 nBufferSize = 0;
	void *pTempBuffer = nil;

	while (true)
	{
		Uint32 nSecondIndex = nFirstIndex + 1;
		while (nSecondIndex < mTreeCount && mPtrTree[nSecondIndex].nItemLevel > nParentLevel + 1)
			nSecondIndex++;
	
		if (nSecondIndex == mTreeCount || mPtrTree[nSecondIndex].nItemLevel <= nParentLevel)
		{
			if (!bChanged)
				return;
			
			bChanged = false;
			nFirstIndex = inParentIndex;
			continue;
		}

		if (inProc(mPtrTree[nFirstIndex].pItem, mPtrTree[nSecondIndex].pItem, inRef) <= 0)
		{
			nFirstIndex = nSecondIndex;
			continue;
		}
		
		Uint32 nThirdIndex = nSecondIndex + 1;
		while (nThirdIndex < mTreeCount && mPtrTree[nThirdIndex].nItemLevel > nParentLevel + 1)
			nThirdIndex++;
			
		Uint32 nNewBufferSize = (nSecondIndex - nFirstIndex) * sizeof(STreeItem);
		
		if (!pTempBuffer)
			pTempBuffer = UMemory::New(nNewBufferSize);
		else if (nNewBufferSize > nBufferSize)
			pTempBuffer = UMemory::Reallocate((TPtr)pTempBuffer, nNewBufferSize);

		if (!pTempBuffer)
			return;
		
		nBufferSize = nNewBufferSize;
		UMemory::Copy(pTempBuffer, mPtrTree + nFirstIndex, nBufferSize);
		UMemory::Copy(mPtrTree + nFirstIndex, mPtrTree + nSecondIndex, (nThirdIndex - nSecondIndex) * sizeof(STreeItem));
		
		nFirstIndex += nThirdIndex - nSecondIndex;
		UMemory::Copy(mPtrTree + nFirstIndex, pTempBuffer, nBufferSize);
		
		bChanged = true;
	}
	
	if (pTempBuffer)
		UMemory::Dispose((TPtr)pTempBuffer);
}

template<class T> STreeItem *CPtrTree<T>::GetTreeItem(T *inPtr, Uint32 *outIndex) const
{
	if (!inPtr)
		return nil;
	
	for (Uint32 i=0; i < mTreeCount; i++)
	{
		STreeItem *pTreeItem = mPtrTree + i;
		
		if (pTreeItem->pItem == inPtr)
		{
			if (outIndex)
				*outIndex = i + 1;
			
			return pTreeItem;
		}			
	}
	
	return nil;
}

