/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */


/*  Pseudo Struct
struct 
{
	Uint32 sig;			// 'harc'
	Uint32 vers;		// 1
	
	Uint32 archiveSize;	// size of the entire archive
	
	Uint32 rsvd[4];
	
	Uint8 archiveName[64];	// pstring
	Uint16 fileCount;	// the number of files in this archive

	Uint16 fileAutoLaunch;	// when complete, this item is launched (0 means none)
	
	Uint16 rsvd3size;
	Uint8 rsvd3Data[rsvd3size];

	struct
	{
		Uint32 type;		// 'file' 'fldr' 'link' 'text' (text clipping)
		Uint32 rsvd;
		
		Uint16 pathSize;	// size of path
		struct
		{
			Uint16 pathcount;	// must be at least 1 - last item specifies file name
			
			struct
			{
				Uint16 script;
				Uint8 namelen;
				Uint8 name[namelen];	// name of folder/directory
			} dir[pathcount];
		} path;
		
		// do I want to store things like date, Media type, flags, etc?
		// (this is all stored in the flat data, but as an archive,
		// it may be more efficient to have access to it)
		
		// just in case we wish to do something later, keep some space reserved.
		Uint16 rsvdSize;
		Uint8 rsvd[rsvdSize];
		
		Uint32 compressionType;		// 'zlib' for now
		Uint32 decompressedSize;	// the size of the data below when decompressed
		Uint32 compressedSize;
		Uint8 compressedFileFlatData[compressedSize];
	} file[fileCount];
};
*/

#pragma options align=packed

struct SMyArcHead
{
	Uint32 sig;			// 'harc'
	Uint32 vers;		// 1
	
	Uint32 archiveSize;	// size of everything below this Uint32
	
	Uint32 rsvd[4];
	
	Uint8 archiveName[64];	// pstring
	
	Uint16 fileCount;		// the number of files in this archive
	Uint16 fileAutoLaunch;	// when complete, this item is launched (0 means none)

	Uint16 rsvd3size;
	Uint8 rsvd3data[];
};
#pragma options align=reset

#pragma options align=packed
struct SMyArcPathHead
{
	Uint32 type;
	Uint32 rsvd;
	
	Uint16 pathSize;
	Uint8 pathData[];		// contains pathcount + SMyArcPathItems
};
#pragma options align=reset

#pragma options align=packed
struct SMyArcPathItem
{
	Uint16 script;
	Uint8 namelen;
	Uint8 name[];	// name of folder/directory
};
#pragma options align=reset

#pragma options align=packed
struct SMyArcFileHead
{
		Uint32 compressionType;		// 'zlib' for now
		Uint32 decompressedSize;	// the size of the data below when decompressed
		Uint32 compressedSize;
		Uint8 compressedFileFlatData[];
};
#pragma options align=reset

/*  So essentially, the archive looks like:
struct
{
	SMyArcHead head;
	Uint8 rsvd3Data[head.rsvd3size];
	
	struct
	{
		SMyArcPathHead fileHead;
	
		Uint16 pathCount;
		
		struct
		{
			SMyArcPathItem path;
			Uint8 name[path.namelen];
			
		} filepath[pathCount];
		
		SMyArcFileHead fileTail;
		
		Uint8 data[fileTail.compressedSize];
	
	} file[head.fileCount];
};
*/

