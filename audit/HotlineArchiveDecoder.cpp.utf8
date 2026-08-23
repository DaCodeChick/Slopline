/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#include "HotlineArchiveDecoder.h"


/* ————————————————————————————————————————————————————————————————————————— */
 
CMyBuffer::CMyBuffer()
{
	mBuf = nil;
	mSize = 0;
	mMaxSize = 0;

}

CMyBuffer::~CMyBuffer()
{
	if (mBuf)
		delete mBuf;
}

Uint32 CMyBuffer::WriteBuf(const void *inData, Uint32 inDataSize, Uint32 inNeededSize)
{
	Uint32 requiredSize = (inNeededSize == max_Uint32 ? inDataSize : inNeededSize);
	
	// chances are that inNeededSize will not change, so let's allocate that size now
	if (mMaxSize < requiredSize)
	{
		mBuf = UMemory::Reallocate(mBuf, requiredSize);
		mMaxSize = requiredSize;
	}
	
	Uint32 s = UMemory::Copy(BPTR(mBuf) + mSize, inData, min(inDataSize, inNeededSize - mSize));

	mSize += s;
	return s;
}

/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -

enum
{
	arcState_ArchiveHeader	= 0,
	arcState_HeadRsvd		= 1,
	arcState_PathHead		= 2,
	arcState_Path			= 3,
	arcState_FileRsvdHead	= 4,
	arcState_FileRsvd		= 5,
	arcState_FileHead		= 6,
	arcState_File			= 7,
	arcState_Folder			= 8,
	
	arcState_Unknown		= 98,
	arcState_JumpToNextFile	= 99,
	
	arcState_Done			= 100

};

CMyArchiveDecoder::CMyArchiveDecoder()
{
	// Initialize members
	mState = arcState_ArchiveHeader;
	mProcessedSize = 0;
	mCurrentItem = 0;
	ClearStruct(mBuf);
	ClearStruct(mArchive);
	ClearStruct(mCurrentFile);

}

CMyArchiveDecoder::~CMyArchiveDecoder()
{
	if (mCurrentFile.ref)
		delete mCurrentFile.ref;
	
	if (mCurrentFile.decompressor)
		delete mCurrentFile.decompressor;
		
	if (mArchive.autoLaunchRef)
		delete mArchive.autoLaunchRef;
}

Uint32 CMyArchiveDecoder::ProcessData(const void *inData, Uint32 inDataSize)
{
	// all functions called by this must process all the data
	// it will not be held otherwise

	#define data		(BPTR(inData) + processedSize)
	#define dataSize	(inDataSize - processedSize)
	
	Uint32 processedSize = 0;
	
	while (processedSize != inDataSize)
	{
		Uint32 s = 0;
		
		switch (mState)
		{
			case arcState_ArchiveHeader:
				s = ProcessHeader(data, dataSize);
				break;
				
			case arcState_HeadRsvd:	
			case arcState_FileRsvd:
			case arcState_Unknown:
				s = ProcessFileRsvd(data, dataSize);
				break;

			case arcState_PathHead:
				s = ProcessPathHead(data, dataSize);
				break;
				
			case arcState_Path:
				s = ProcessPath(data, dataSize);
				break;
				
			case arcState_FileRsvdHead:
				s = ProcessFileRsvdHead(data, dataSize);
				break;
						
			case arcState_FileHead:
				s = ProcessFileHead(data, dataSize);
				break;
				
			case arcState_File:
				s = ProcessFile(data, dataSize);
				break;
			
			case arcState_Folder:
				s = ProcessFolder(data, dataSize);
				break;				
			
			case arcState_JumpToNextFile:
				if (mCurrentItem == mArchive.fileCount)
					mState = arcState_Done;
				else
					mState = arcState_PathHead;
				break;
			
			case arcState_Done:
				// we should not be getting data when done...
				goto goReturn;
				break;
						
			default:	// I don't recognize this state!
				Fail(errorType_Misc, error_Protocol);
				goto goReturn;
				break;
		}
		
		processedSize += s;
		mProcessedSize += s;
	}

goReturn:
		
	if (mState == arcState_ArchiveHeader)
		return arcStatus_WaitingForData;
	else if (mState == arcState_Done || (mState == arcState_JumpToNextFile && mCurrentItem == mArchive.fileCount))
		return arcStatus_Complete;
	else
		return arcStatus_ReceivingFiles;
		
	#undef data
	#undef dataSize
}

Uint32 CMyArchiveDecoder::ProcessHeader(const void *inData, Uint32 inDataSize)
{
	Uint32 s = mBuf.WriteBuf(inData, inDataSize, sizeof(SMyArcHead));
	if (mBuf.GetSize() == sizeof(SMyArcHead))
	{
		// we're done!
		SMyArcHead *head = (SMyArcHead *)mBuf.GetPtr();
		
		if (head->sig != TB((Uint32)'harc'))
			Fail(errorType_Misc, error_FormatUnknown);
		
		if (head->vers != TB((Uint32)1))
			Fail(errorType_Misc, error_VersionUnknown);
		
		mArchive.totalSize = FB(head->archiveSize);
		mArchive.fileCount = FB(head->fileCount);
		mArchive.autoLaunchNum = FB(head->fileAutoLaunch);
		mArchive.name[0] = UMemory::Copy(mArchive.name + 1, head->archiveName + 1, min((Uint32)sizeof(mArchive.name) - 1, (Uint32)head->archiveName[0]));
		
		mCurrentFile.rsvdSize = FB(head->rsvd3size);
			
		mBuf.Clear();
		mState = arcState_HeadRsvd;
	}

	return s;
}

Uint32 CMyArchiveDecoder::ProcessPathHead(const void *inData, Uint32 inDataSize)
{
	Uint32 s = mBuf.WriteBuf(inData, inDataSize, sizeof(SMyArcPathHead));
	if (mBuf.GetSize() == sizeof(SMyArcPathHead))
	{
		ClearStruct(mCurrentFile);
		
		SMyArcPathHead *pathHead = (SMyArcPathHead *)mBuf.GetPtr();
		
		mCurrentFile.type = FB(pathHead->type);
		mCurrentFile.pathSize = FB(pathHead->pathSize);
		
		mBuf.Clear();
		
		mState = arcState_Path;
		
		// we're now on the next file
		mCurrentItem++;
	}

	return s;
}

Uint32 CMyArchiveDecoder::ProcessPath(const void *inData, Uint32 inDataSize)
{
	Uint32 s = mBuf.WriteBuf(inData, inDataSize, mCurrentFile.pathSize);
	
	Uint32 bufSize = mBuf.GetSize();
	if (bufSize == mCurrentFile.pathSize)
	{
		mCurrentFile.ref = UFS::New(kProgramFolder, mBuf.GetPtr(), bufSize, nil);
		
		if (mArchive.autoLaunchNum == mCurrentItem)
			mArchive.autoLaunchRef = mCurrentFile.ref->Clone();
		
		mBuf.Clear();
		mState = arcState_FileRsvdHead;
	}

	return s;
}

Uint32 CMyArchiveDecoder::ProcessFileRsvdHead(const void *inData, Uint32 inDataSize)
{
	Uint32 s = mBuf.WriteBuf(inData, inDataSize, sizeof(Uint16));
	if (mBuf.GetSize() == sizeof(Uint16))
	{
		mCurrentFile.rsvdSize = FB(*((Uint16*)mBuf.GetPtr()));
	
		mBuf.Clear();
		mState = arcState_FileRsvd;
	}
	
	return s;
}

Uint32 CMyArchiveDecoder::ProcessFileRsvd(const void *inData, Uint32 inDataSize)
{
	#pragma unused(inData)
	
	Uint32 s = min(inDataSize, (Uint32)mCurrentFile.rsvdSize);		
	mCurrentFile.rsvdSize -= s;

	if (!mCurrentFile.rsvdSize)
		mState++;

	return s;
}

Uint32 CMyArchiveDecoder::ProcessFileHead(const void *inData, Uint32 inDataSize)
{
	Uint32 s = mBuf.WriteBuf(inData, inDataSize, sizeof(SMyArcFileHead));
	if (mBuf.GetSize() == sizeof(SMyArcFileHead))
	{
		SMyArcFileHead *fileHead = (SMyArcFileHead *)mBuf.GetPtr();
		
		mCurrentFile.compressionType = FB(fileHead->compressionType);
		mCurrentFile.decompressedSize = FB(fileHead->decompressedSize);
		mCurrentFile.compressedSize = FB(fileHead->compressedSize);
		
		if (mCurrentFile.compressionType != 'zlib' && mCurrentFile.compressionType != 'raw ')
			Fail(errorType_Misc, error_FormatUnknown);
		
		// should do some checks on compressedSize and all
		
		mBuf.Clear();
		
		switch (mCurrentFile.type)
		{
			case 'file':
				if (mCurrentFile.compressionType == 'zlib')
					mCurrentFile.decompressor = UZlibDecompress::New();
				
				if (mCurrentFile.ref->Exists())
				{
					// let's ask the user if we should delete it or skip this file
					mCurrentFile.ref->DeleteFile();
				}	
				mCurrentFile.ref->CreateFile('HTft', 'HTLC');
				mCurrentFile.ref->StartUnflatten();

				mState = arcState_File;
				break;
			
			case 'fldr':
				bool folder;
				if (mCurrentFile.ref->Exists(&folder))
				{
					if (folder)
					{
						// this folder exists
						goto skipCreateFolder;
					}
					else
					{
						// what should we do?? should we delete this item?
						// let's ask the user
					}	
				}

				mCurrentFile.ref->CreateFolder();

skipCreateFolder:	
				delete mCurrentFile.ref;
				mCurrentFile.ref = nil;

				mState = arcState_Folder;
				break;
			
			default:
				// I don't support this type - skip over it
				mCurrentFile.rsvdSize = mCurrentFile.compressedSize;
				mState = arcState_Unknown;
				break;
		}
	}

	return s;
}	

Uint32 CMyArchiveDecoder::ProcessFile(const void *inData, Uint32 inDataSize)
{
	Uint32 s = min(mCurrentFile.compressedSize - mCurrentFile.processedSize, inDataSize);
	
	switch (mCurrentFile.compressionType)
	{
		case 'raw ':
			mCurrentFile.ref->ProcessUnflatten(inData, s);
			mCurrentFile.processedSize += s;
			
			if (mCurrentFile.processedSize == mCurrentFile.compressedSize)
			{
				mCurrentFile.ref->ProcessUnflatten(nil, 0);
				
				delete mCurrentFile.ref;
				mCurrentFile.ref = nil;

				mState = arcState_JumpToNextFile;
			}
			break;
			
		case 'zlib':
			// feed inData to zlib
			// if the result is some decompressed data, feed this to ProcessUnflatten();
			// need to allocate a temp buffer for the data - start at 100k and if we need more, grow it (if 1 message requires more)
		
			// or, we don't have to decompress on the fly
			// we could, instead, write to a file then read it back in to decompress
			// but it's more cool to do it on the fly!
			Uint32 outSize = 0;
			TPtr p = mCurrentFile.decompressor->Decompress(inData, s, outSize);
			
			if (p)
			{
				mCurrentFile.ref->ProcessUnflatten(BPTR(p), outSize);
				delete p;
			}
			
			mCurrentFile.processedSize += s;
			
			if (mCurrentFile.processedSize == mCurrentFile.compressedSize)
			{
				mCurrentFile.ref->ProcessUnflatten(nil, 0);
				
				delete mCurrentFile.ref;
				mCurrentFile.ref = nil;

				delete mCurrentFile.decompressor;
				mCurrentFile.decompressor = nil;

				mState = arcState_JumpToNextFile;
			}
			break;
	}

	return s;
}

Uint32 CMyArchiveDecoder::ProcessFolder(const void *inData, Uint32 inDataSize)
{
	#pragma unused(inData, inDataSize)
	// I don't need any of the data for the folder

	mCurrentFile.rsvdSize = mCurrentFile.compressedSize;
	mState = arcState_Unknown;
	return 0;
}

