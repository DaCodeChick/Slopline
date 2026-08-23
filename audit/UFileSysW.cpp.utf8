#if WIN32

/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */


// there's some weird SHELL32 problem with using SHGetSpecialFolderPath
// Don't want to figure it out now, but since no app uses it, we won't yet either...
#define ALLOW_DESKTOP_FOLDER	0



#include "UFileSys.h"
#include "UMemory.h"

#include <SHLOBJ.H>

struct SFileSysRef {
	HANDLE h;
	Uint32 pathSize;	// not including null
	Int8 *path;			// null-terminated DOS style path, eg C:\myworld\private
	TPtr flatData, unflatData;
};

//#define REF		((SFileSysRef *)inRef)

static void _FSFindPathTargetName(const void *inPath, Uint32 inPathSize, Uint8*& outName, Uint32& outNameSize);
static void _FSFindPathParentName(const void *inPath, Uint32 inPathSize, Uint8*& outName, Uint32& outNameSize);
static void _FSGetPathExtension(const void *inPath, Uint32 inPathSize, Uint8 *outExt);
static void _FSWinExtToMacTypeCode(const Uint8 *inExt, Uint32& outTypeCode, Uint32& outCreatorCode);
static void _FSStripName(void *ioName, Uint32 inSize);
static void _FSAccessToWinAccess(Uint32 inPerm, Uint32& outAccess, Uint32& outShare);
static void _FSWinTimeToStamp(const FILETIME& inWinTime, SDateTimeStamp& outStamp);
static bool _FSParentExists(const Int8 *inPath);
static Uint32 _FSConvertPathString(const Uint8 *inPath, void *outData);
static Uint32 _FSConvertPathData(const void *inPath, Uint32 inPathSize, void *outData);
static bool _FSDoNewOptions(const Int8 *inPath, Int8 *outResolvedPath, Uint32 inOptions, Uint16 *outType);
static Uint32 _FSCountDirItems(const void *inPath, Uint32 inPathSize, const void *inDirName, Uint32 inDirNameSize);
static HRESULT _FSCreateLink(LPCSTR lpszPathObj, LPSTR lpszPathLink, LPSTR lpszDesc);
static HRESULT _FSResolveLink(HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath, WIN32_FIND_DATA *outWFD = nil, int levels = 10);
static bool _FSIsLinkFile(const void *inPath, Uint32 inPathSize);
static Uint32 _FSPathDataToActualDir(const void *inPathData, Uint32 inPathDataSize, void *ioPathStr, Uint32 inPathStrSize, Uint32 inMaxSize);
static HRESULT _FSResolveLinksInPath(HWND hwnd, LPCSTR lpszInPath, LPSTR lpszOutPath, WIN32_FIND_DATA *outWFD);
Uint32 _GetMaxChildPathSize(TFSRefObj* inRef);
TFSRefObj* _FSNewRefFromWinPath(const void *inPath, Uint32 inPathSize);
TFSRefObj* _PathToRef(const void *inPath, Uint32 inPathSize);
TFSRefObj* _FSGetModuleRef();


void _FailLastWinError(const Int8 *inFile, Uint32 inLine);
void _FailWinError(Int32 inWinError, const Int8 *inFile, Uint32 inLine);
#if DEBUG
	#define FailLastWinError()		_FailLastWinError(__FILE__, __LINE__)
	#define FailWinError(id)		_FailWinError(id, __FILE__, __LINE__)
#else
	#define FailLastWinError()		_FailLastWinError(nil, 0)
	#define FailWinError(id)		_FailWinError(id, nil, 0)
#endif

// set this to false to stop user interface being shown in the process of resolving an alias
bool _FSAliasUI = true;

/* -------------------------------------------------------------------------- */

// should fsOption_ResolveAliases check for blah.lnk if blah does not exist?  (also needs to be done in path)
TFSRefObj* UFileSys::New(TFSRefObj* inStartFolder, const Uint8 *inPath, const Uint8 *inName, Uint32 inOptions, Uint16 *outType)
{
	Int8 *startDir, *newPath, *p;
	Uint32 startDirSize, pathSize, nameSize, newPathSize;
	SFileSysRef *ref;
	Int8 startDirBuf[2048];
	Int8 path[256];
	Int8 name[256];

	Require(inStartFolder);
	
	// get starting directory
	if (inStartFolder == kRootFolderHL)
	{
		startDirBuf[0] = 'C';
		startDirBuf[1] = ':';
		startDirBuf[2] = '\\';
		startDirBuf[3] = 0;
		startDir = startDirBuf;
		startDirSize = 3;
	}
	else if (inStartFolder == kProgramFolder || inStartFolder == kPrefsFolder)
	{
		startDir = startDirBuf;
		//startDirSize = ::GetCurrentDirectory(sizeof(startDirBuf), startDirBuf);
		
		startDirSize = ::GetModuleFileName(NULL, startDirBuf, sizeof(startDirBuf));
		
		Uint8 *tempp;
		_FSFindPathTargetName(startDirBuf, startDirSize, tempp, nameSize);
		startDirSize -= nameSize;	// lose the file name
		startDirSize--;				// lose the slash
	}
#if ALLOW_DESKTOP_FOLDER
	else if (inStartFolder == kDesktopFolder)
	{
		startDir = startDirBuf;
		::SHGetSpecialFolderPath(NULL, startDirBuf, CSIDL_DESKTOPDIRECTORY, TRUE);
		startDirSize = strlen(startDirBuf);
	}
#endif // ALLOW_DESKTOP_FOLDER
	else if (inStartFolder == kTempFolder)
	{
		startDir = startDirBuf;
		startDirSize = ::GetTempPath(sizeof(startDirBuf), startDirBuf);
		startDirSize--;				// lose the slash
	}
	else
	{
		startDir = ((SFileSysRef *)inStartFolder)->path;
		startDirSize = ((SFileSysRef *)inStartFolder)->pathSize;
	}
	
	// get path to follow on from starting directory
	pathSize = _FSConvertPathString(inPath, path);

	// get name to follow on from path
	if (inName)
	{
		nameSize = inName[0];
		if (nameSize > 248)	
			Fail(errorType_FileSys, fsError_PathToLong);
		::CopyMemory(name, inName+1, nameSize);
		_FSStripName(name, nameSize);
	}
	else
		nameSize = 0;

	// allocate memory for new path
	newPathSize = startDirSize + pathSize + nameSize;
	if (pathSize) newPathSize++;	// for slash
	if (nameSize) newPathSize++;	// for slash
	if (newPathSize > 248)	
		Fail(errorType_FileSys, fsError_PathToLong);
	newPath = (Int8 *)UMemory::New(newPathSize+1);	// +1 is for the null
	
	try
	{
		// build new path
		::CopyMemory(newPath, startDir, startDirSize);
		p = newPath + startDirSize;
		if (pathSize)
		{
			*p++ = '\\';
			::CopyMemory(p, path, pathSize);
			p += pathSize;
		}
		if (nameSize)
		{
			*p++ = '\\';
			::CopyMemory(p, name, nameSize);
		}
		newPath[newPathSize] = 0;		// null term
	
		// do options
		Int8 resolvedPath[MAX_PATH];
		if (_FSDoNewOptions(newPath, resolvedPath, inOptions, outType))
		{
			UMemory::Dispose((TPtr)newPath);
			return nil;
		}
		if (resolvedPath[0])
		{
			UMemory::Dispose((TPtr)newPath);
			newPath = nil;
			newPathSize = strlen(resolvedPath);
			newPath = (Int8 *)UMemory::New(resolvedPath, newPathSize+1);
		}
		
		// make the FS ref
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		ref->path = newPath;
		ref->pathSize = newPathSize;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)newPath);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

/*
The path is formatted like this:

	Uint16 count;
	struct {
		Uint16 script;
		Uint8 len;
		Uint8 name[len];	// name of folder/directory
	} dir[count];
	
All integers are in network byte order.
*/
TFSRefObj* UFileSys::New(TFSRefObj* inStartFolder, const void *inPath, Uint32 inPathSize, const Uint8 *inName, Uint32 inOptions, Uint16 *outType)
{
	Uint8 *tempp;
	Uint32 pathSize, nameSize, n;
	SFileSysRef *ref;
	Int8 path[4096];

	Require(inStartFolder);
	nameSize = inName ? inName[0] : 0;
	
	// set path to starting directory
	if (inStartFolder == kRootFolderHL)
	{
		path[0] = 'C';
		path[1] = ':';
		path[2] = '\\';
		path[3] = 0;
		pathSize = 3;
	}
	else if (inStartFolder == kProgramFolder || inStartFolder == kPrefsFolder)
	{
		pathSize = ::GetModuleFileName(NULL, path, sizeof(path));
		
		_FSFindPathTargetName(path, pathSize, tempp, n);
		pathSize -= n;		// loose the file name
		pathSize--;			// loose the slash
	}
#if ALLOW_DESKTOP_FOLDER
	else if (inStartFolder == kDesktopFolder)
	{
		::SHGetSpecialFolderPath(NULL, path, CSIDL_DESKTOPDIRECTORY, TRUE);
		pathSize = strlen(path);
	}
#endif // ALLOW_DESKTOP_FOLDER
	else if (inStartFolder == kTempFolder)
	{
		pathSize = ::GetTempPath(sizeof(path), path);
		pathSize--;			// loose the slash
	}
	else	// not a special folder
	{
		pathSize = ((SFileSysRef *)inStartFolder)->pathSize;
		if (pathSize >= sizeof(path)) goto noSuchFolder;
		::CopyMemory(path, ((SFileSysRef *)inStartFolder)->path, pathSize);
	}

	// extend path with names from inPath
	if (nameSize)
	{
		pathSize = _FSPathDataToActualDir(inPath, inPathSize, path, pathSize, sizeof(path));
	}
	else if (inPathSize >= 6)
	{
		// no have no name, target is last item in path
		Uint16 tmpCountOrig = *(Uint16 *)inPath;
		Uint16 tmpCount = FB( tmpCountOrig );
		if (tmpCount > 1)
		{
			*(Uint16 *)inPath = TB((Uint16)(tmpCount-1));
			try
			{
				pathSize = _FSPathDataToActualDir(inPath, inPathSize, path, pathSize, sizeof(path));
			}
			catch(...)
			{
				*(Uint16 *)inPath = tmpCountOrig;	// inPath is const, so restore original value
				throw;
			}
			*(Uint16 *)inPath = tmpCountOrig;
		}
	}
	
	// check if dir can't be found
	if (pathSize == 0)
	{
noSuchFolder:
		if (inOptions & (fsOption_NilIfNonExistant | fsOption_NilIfNonExistantParent))	// if either option is on
		{
			if (outType) *outType = fsItemType_NonExistantParent;
			return nil;
		}
		
		Fail(errorType_FileSys, fsError_NoSuchFolder);
	}
	
	// add the name to the path
	if (nameSize == 0)
	{
		UFileSys::GetPathTargetName(inPath, inPathSize, nil, 0, nil, &tempp);
		inName = tempp;
		nameSize = inName ? inName[0] : 0;
	}
	if (nameSize)
	{
		// if size of name is greater than space left in the buf
		if ((nameSize+2) > (sizeof(path) - pathSize))
			goto noSuchFolder;
			
		// TG strip out any slashes in the name, for security reasons
		for (Uint32 s = 1; s != nameSize; s++)
		{
			if (inName[s] == '\\' || inName[s] == '/' || inName[s] == ':')
			Fail(errorType_FileSys, fsError_BadName);
		}
		
		// TG also check that name isn't '..', again for security
		if (nameSize == 2 && inName[1] == '.' && inName[2] == '.')
			Fail(errorType_FileSys, fsError_BadName);
				
		// TG and finally, because windows does not allow any '.' at the end of the filename
		// complain if we have one.  this will solve the .lnk. problem in Hotline Server
		if (inName[nameSize] == '.')
			Fail(errorType_FileSys, fsError_BadName);
		if (pathSize+1+nameSize > 248)	
			Fail(errorType_FileSys, fsError_PathToLong);
			
		path[pathSize++] = '\\';
		::CopyMemory(path + pathSize, inName+1, nameSize);
		_FSStripName(path + pathSize, nameSize);
		pathSize += nameSize;
	}
	
	// null terminate the path
	path[pathSize] = 0;
	
	// handle the options
	Int8 resolvedPath[MAX_PATH];
	if (_FSDoNewOptions(path, resolvedPath, inOptions, outType)) return nil;
	if (resolvedPath[0])
	{
		strcpy(path, resolvedPath);
		pathSize = strlen(resolvedPath);
	}

	// path munging complete, make the FS ref
	Int8 *newPath = (Int8 *)UMemory::New(path, pathSize+1);	// +1 is for the null
	try
	{
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		ref->path = newPath;
		ref->pathSize = pathSize;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)newPath);
		throw;
	}
	
	// all done
	return (TFSRefObj*)ref;
}

// should this create the file to ensure that the name doesn't get used by some other app?
// but you'd have to specify extra info like the type string...
TFSRefObj* UFileSys::NewTemp(TFSRefObj* inRef)
{
	#pragma unused(inRef)
	Fail(errorType_Misc, error_Unimplemented);
	return nil;
	
	/*
	Int8 *tempDir[2048];
	
	tempDir[0] = '.';
	tempDir[1] = 0;
	::GetTempPath(sizeof(tempDir), tempDir);
	
	
	UINT GetTempFileName(tempDir, "TMP",
    UINT  uUnique,
    LPTSTR  lpTempFileName);*/
}

// if the ref is open, the clone ref is not
TFSRefObj* UFileSys::Clone(TFSRefObj* inRef)
{
	Int8 *path = (Int8 *)UMemory::New(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize+1);	// +1 is for the null
	SFileSysRef *ref;
	try
	{
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		
		ref->path = path;
		ref->pathSize = ((SFileSysRef *)inRef)->pathSize;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)path);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

void UFileSys::Dispose(TFSRefObj* inRef)
{
	if (inRef)
	{
		if (((SFileSysRef *)inRef)->h) ::CloseHandle(((SFileSysRef *)inRef)->h);
		UMemory::Dispose((TPtr)((SFileSysRef *)inRef)->path);
		UMemory::Dispose((TPtr)((SFileSysRef *)inRef)->flatData);
		UMemory::Dispose((TPtr)((SFileSysRef *)inRef)->unflatData);
		UMemory::Dispose((TPtr)inRef);
	}
}

void UFileSys::SetRef(TFSRefObj* inDst, TFSRefObj* inSrc)
{
	Require(inDst && inSrc);
	
	if (((SFileSysRef *)inDst)->h == NULL)
	{
		Int8 *path = (Int8 *)UMemory::New(((SFileSysRef *)inSrc)->path, ((SFileSysRef *)inSrc)->pathSize+1);	// +1 is for the null
		
		UMemory::Dispose((TPtr)((SFileSysRef *)inDst)->path);
		
		((SFileSysRef *)inDst)->path = path;
		((SFileSysRef *)inDst)->pathSize = ((SFileSysRef *)inSrc)->pathSize;
	}
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// fails if file already exists
void UFileSys::CreateFile(TFSRefObj* inRef, Uint32 /* inTypeCode */, Uint32 /* inCreatorCode */)
{
	Require(inRef);
	
	HANDLE h = ::CreateFileA(((SFileSysRef *)inRef)->path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) 
		FailLastWinError();
	
	::CloseHandle(h);
}

void UFileSys::CreateFileAndOpen(TFSRefObj* inRef, Uint32 /* inTypeCode */, Uint32 /* inCreatorCode */, Uint32 inOptions, Uint32 inPerm)
{
	DWORD cd, access, share;
	HANDLE h;
	
	Require(inRef);
	if (((SFileSysRef *)inRef)->h) Fail(errorType_Misc, error_Protocol);
	
	switch (inOptions)
	{
		case kCreateNewFile:		cd = CREATE_NEW;		break;
		case kAlwaysCreateFile:		cd = CREATE_ALWAYS;		break;
		case kOpenExistingFile:		cd = OPEN_EXISTING;		break;
		case kAlwaysOpenFile:		cd = OPEN_ALWAYS;		break;
		default:					cd = CREATE_NEW;		break;
	}
	
	_FSAccessToWinAccess(inPerm, access, share);
	
	h = ::CreateFileA(((SFileSysRef *)inRef)->path, access, share, NULL, cd, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) FailLastWinError();
	
	((SFileSysRef *)inRef)->h = h;
}

void UFileSys::CreateFolder(TFSRefObj* inRef)
{
	Require(inRef);
	if (!::CreateDirectoryA(((SFileSysRef *)inRef)->path, NULL))
		FailLastWinError();
}

void UFileSys::CreateAlias(TFSRefObj* inRef, TFSRefObj* inOriginal)
{
	Require(inRef && inOriginal);
	
	Uint8 ext[16];
	HRESULT err;
	
	Int8 *oldPath = ((SFileSysRef *)inRef)->path;
	Uint32 oldPathSize = ((SFileSysRef *)inRef)->pathSize;
	
	_FSGetPathExtension(oldPath, oldPathSize, ext);
	
	// if has dest has lnk extension, can just create the link, otherwise need to add lnk ext
	if (ext[0] == 3 && tolower(ext[1]) == 'l' && tolower(ext[2]) == 'n' && tolower(ext[3]) == 'k')
	{
		// create the link
		err = _FSCreateLink(((SFileSysRef *)inOriginal)->path, oldPath, nil);
		if (err) FailWinError(err);
	}
	else	// need to add lnk ext
	{
		// grab a copy of the old path
		Uint32 newPathSize = oldPathSize + 4;
		Int8 *newPath = (Int8 *)UMemory::New(newPathSize+1);	// +1 is for the null
		UMemory::Copy(newPath, oldPath, oldPathSize);
		
		// append the .lnk
		Int8 *p = newPath + oldPathSize;
		p[0] = '.';
		p[1] = 'l';
		p[2] = 'n';
		p[3] = 'k';
		p[4] = 0;
		
		// create the link
		err = _FSCreateLink(((SFileSysRef *)inOriginal)->path, newPath, nil);
		if (err)
		{
			UMemory::Dispose((TPtr)newPath);
			FailWinError(err);
		}
		
		// successfully created the link, update inRef
		UMemory::Dispose((TPtr)oldPath);
		((SFileSysRef *)inRef)->path = newPath;
		((SFileSysRef *)inRef)->pathSize = newPathSize;
	}
}

bool UFileSys::Exists(TFSRefObj* inRef, bool *outIsFolder)
{
	if (inRef)
	{
		Uint32 attrib = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
		
		if (attrib != 0xFFFFFFFF)	// if not error
		{
			if (outIsFolder) 
				*outIsFolder = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
			
			return true;
		}
	}
	
	if (outIsFolder) 
		*outIsFolder = false;
	
	return false;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// does nothing if the file doesn't exist
void UFileSys::DeleteFile(TFSRefObj* inRef)
{
	Require(inRef);
	
	// can't delete open file
	if (((SFileSysRef *)inRef)->h)
		Fail(errorType_FileSys, fsError_FileInUse);
	
	if (!::DeleteFileA(((SFileSysRef *)inRef)->path))
	{
		Uint32 err = ::GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND && err != ERROR_INVALID_NAME)
			FailWinError(err);
	}
}

// does nothing if the folder doesn't exist, fails if folder contains other items
void UFileSys::DeleteFolder(TFSRefObj* inRef)
{
	Require(inRef);
	
	if (!::RemoveDirectory(((SFileSysRef *)inRef)->path))
	{
		Uint32 err = ::GetLastError();
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND && err != ERROR_INVALID_NAME)
			FailWinError(err);
	}
}

// delete folder with all folder contents
void UFileSys::DeleteFolderContents(TFSRefObj* inRef)
{
	Require(inRef);
	
	SHFILEOPSTRUCT op;
	
	Int8 path[4096];
	Uint32 pathSize = ((SFileSysRef *)inRef)->pathSize;
	
	// even tho we have FOF_SILENT on, windoze likes to display the error box anyway when the file doesn't exist, so we'll preflight here
	if (::GetFileAttributes(((SFileSysRef *)inRef)->path) == 0xFFFFFFFF) return;
	
	if (pathSize > sizeof(path)-2) return;
	
	::CopyMemory(path, ((SFileSysRef *)inRef)->path, pathSize);
	path[pathSize] = 0;
	path[pathSize+1] = 0;		// this second null is necessary to terminate the list of paths
	
	op.hwnd = NULL;
    op.wFunc = FO_DELETE;
    op.pFrom = path;
    op.pTo = NULL;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    op.fAnyOperationsAborted = false;
    op.hNameMappings = NULL;
    op.lpszProgressTitle = NULL;
	
	SHFileOperation(&op);
}

// won't throw an exception if the target doesn't exist
void UFileSys::MoveToTrash(TFSRefObj* inRef)
{
	Require(inRef);
	
	SHFILEOPSTRUCT op;
	
	Int8 path[4096];
	Uint32 pathSize = ((SFileSysRef *)inRef)->pathSize;
	
	// even tho we have FOF_SILENT on, windoze likes to display the error box anyway when the file doesn't exist, so we'll preflight here
	if (::GetFileAttributes(((SFileSysRef *)inRef)->path) == 0xFFFFFFFF) return;
	
	if (pathSize > sizeof(path)-2) return;
	
	::CopyMemory(path, ((SFileSysRef *)inRef)->path, pathSize);
	path[pathSize] = 0;
	path[pathSize+1] = 0;		// this second null is necessary to terminate the list of paths
	
	op.hwnd = NULL;
    op.wFunc = FO_DELETE;
    op.pFrom = path;
    op.pTo = NULL;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_RENAMEONCOLLISION | FOF_SILENT | FOF_NOERRORUI;
    op.fAnyOperationsAborted = false;
    op.hNameMappings = NULL;
    op.lpszProgressTitle = NULL;
	
	SHFileOperation(&op);
	
/*
	DWORD attr = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
	
	if (attr != 0xFFFFFFFF)
	{
		// no idea how to move something to the recycle bin so we'll delete it instead
		if (attr & FILE_ATTRIBUTE_DIRECTORY)
			UFileSys::DeleteFolder(inRef);
		else
			UFileSys::DeleteFile(inRef);
	}
*/
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// if rename successful, ref is updated to point to same item (OR, ref still points to item after rename)
void UFileSys::SetName(TFSRefObj* inRef, const Uint8 *inName, Uint32 /* inEncoding */)
{
	Uint8 *oldName;
	Uint32 oldNameSize;
	Int8 *newPath, *oldPath;
	Uint32 newPathSize, oldPathSize, samePathSize;
	
	Require(inRef && inName && inName[0]);

	oldPath = ((SFileSysRef *)inRef)->path;
	oldPathSize = ((SFileSysRef *)inRef)->pathSize;
	_FSFindPathTargetName(oldPath, oldPathSize, oldName, oldNameSize);
	
	samePathSize = oldPathSize - oldNameSize;
	newPathSize = samePathSize + inName[0];
	// 248 characters is the limit for paths size (MAX_PATH is 260 characters)
	if (newPathSize > 248)							
		Fail(errorType_FileSys, fsError_PathToLong);

	Uint32 attrib = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
	if (attrib & FILE_ATTRIBUTE_DIRECTORY)
	{
		Uint32 maxChildPathSize = _GetMaxChildPathSize(inRef);
		if (newPathSize + maxChildPathSize > 248)	// check max child path size as well
			Fail(errorType_FileSys, fsError_ChildPathToLong);
	}
	
	newPath = (Int8 *)UMemory::New(newPathSize+1);	// +1 is for the null

	::CopyMemory(newPath, oldPath, samePathSize);
	::CopyMemory(newPath+samePathSize, inName+1, inName[0]);
	newPath[newPathSize] = 0;

	if (!::MoveFileA(oldPath, newPath))
	{
		DWORD err = ::GetLastError();	// must get before dispose so not overwritten
		UMemory::Dispose((TPtr)newPath);
		FailWinError(err);
	}
	
	UMemory::Dispose((TPtr)oldPath);
	((SFileSysRef *)inRef)->path = newPath;
	((SFileSysRef *)inRef)->pathSize = newPathSize;
}

void UFileSys::SetRefName(TFSRefObj* inRef, const Uint8 *inName, Uint32 /* inEncoding */)
{
	Uint8 *oldName;
	Uint32 oldNameSize;
	Int8 *newPath, *oldPath;
	Uint32 newPathSize, oldPathSize, samePathSize;
	
	Require(inRef && inName && inName[0]);

	oldPath = ((SFileSysRef *)inRef)->path;
	oldPathSize = ((SFileSysRef *)inRef)->pathSize;
	_FSFindPathTargetName(oldPath, oldPathSize, oldName, oldNameSize);
	
	samePathSize = oldPathSize - oldNameSize;
	newPathSize = samePathSize + inName[0];
	// 248 characters is the limit for paths size (MAX_PATH is 260 characters)
	if (newPathSize > 248)							
		Fail(errorType_FileSys, fsError_PathToLong);
	newPath = (Int8 *)UMemory::New(newPathSize+1);	// +1 is for the null

	::CopyMemory(newPath, oldPath, samePathSize);
	::CopyMemory(newPath+samePathSize, inName+1, inName[0]);
	newPath[newPathSize] = 0;
	
	UMemory::Dispose((TPtr)oldPath);
	((SFileSysRef *)inRef)->path = newPath;
	((SFileSysRef *)inRef)->pathSize = newPathSize;
}

// outName must point to 256 bytes
void UFileSys::GetName(TFSRefObj* inRef, Uint8 *outName, Uint32 *outEncoding)
{
	Require(inRef);
	
	if (outEncoding) 
		*outEncoding = 0;
	
	if (outName)
		outName[0] = GetName(inRef, outName+1, 255, outEncoding);
}

Uint32 UFileSys::GetName(TFSRefObj* inRef, void *outData, Uint32 inMaxSize, Uint32 *outEncoding)
{
	Require(inRef && outData);

	Uint8 *name;
	Uint32 nameSize;
	_FSFindPathTargetName(((SFileSysRef *)inRef)->path, 
						  ((SFileSysRef *)inRef)->pathSize, 
						  name, nameSize);
	
	if (nameSize > inMaxSize) nameSize = inMaxSize;
	::CopyMemory(outData, name, nameSize);
	
	if (outEncoding) *outEncoding = 0;

	return nameSize;
}

// return true if the name was changed
bool UFileSys::EnsureUniqueName(TFSRefObj* inRef)
{
	Require(inRef);

	if (!Exists(inRef))
		return false;
	
	Uint32 nNameCount = 0;
		
	Uint32 nPathSize;
	Int8 bufPath[4096];
	UMemory::Copy(bufPath, ((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize);

	do
	{
		nNameCount++;
		if (nNameCount >= 10000)
			return false;
		
		nPathSize = ((SFileSysRef *)inRef)->pathSize;
		nPathSize += UText::IntegerToText(bufPath + nPathSize, sizeof(bufPath) - nPathSize - 1, nNameCount);
		bufPath[nPathSize] = 0;
	
	} while(::GetFileAttributes(bufPath) != 0xFFFFFFFF);	// while not error
		
	((SFileSysRef *)inRef)->pathSize = nPathSize;
	((SFileSysRef *)inRef)->path = (Int8 *)UMemory::Reallocate((TPtr)((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize + 1);
	UMemory::Copy(((SFileSysRef *)inRef)->path, bufPath, ((SFileSysRef *)inRef)->pathSize + 1);
	
	return true;
}

Uint32 UFileSys::GetPath(TFSRefObj* inRef, void *outPath, Uint32 inMaxSize, Uint32 *outEncoding)
{
	Require(inRef && outPath);
	
	Uint32 nPathSize = ((SFileSysRef *)inRef)->pathSize;
	if (nPathSize > inMaxSize) nPathSize = inMaxSize;
	::CopyMemory(outPath, ((SFileSysRef *)inRef)->path, nPathSize);

	if (outEncoding) *outEncoding = 0;
	
	return nPathSize;
}

Uint32 UFileSys::GetFolder(TFSRefObj* inRef, void *outFolder, Uint32 inMaxSize, Uint32 *outEncoding)
{
	Require(inRef && outFolder);
	
	Uint8 *pName;
	Uint32 nNameSize;
	_FSFindPathTargetName(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, pName, nNameSize);

	Uint32 nFolderSize = ((SFileSysRef *)inRef)->pathSize - nNameSize;
	if (nFolderSize > inMaxSize) nFolderSize = inMaxSize;
	::CopyMemory(outFolder, ((SFileSysRef *)inRef)->path, nFolderSize);

	if (outEncoding) *outEncoding = 0;
	
	return nFolderSize;
}

TFSRefObj* UFileSys::GetFolder(TFSRefObj* inRef)
{
	Require(inRef);
	
	Uint8 *pName;
	Uint32 nNameSize;
	_FSFindPathTargetName(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, pName, nNameSize);
	
	if (nNameSize)
		nNameSize++;	// lose the slash
	
	return _FSNewRefFromWinPath(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize - nNameSize);
}

// outName must point to 256 bytes
void UFileSys::GetParentName(TFSRefObj* inRef, Uint8 *outName, Uint32 *outEncoding)
{
	Require(inRef);
	
	if (outName)
	{
		Uint8 *name;
		Uint32 nameSize;
		_FSFindPathParentName(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, name, nameSize);
		
		if (nameSize > 255) nameSize = 255;
		::CopyMemory(outName+1, name, nameSize);
		outName[0] = nameSize;
	}

	if (outEncoding) *outEncoding = 0;
}

Uint32 UFileSys::GetParentName(TFSRefObj* inRef, void *outData, Uint32 inMaxSize, Uint32 *outEncoding)
{
	Require(inRef && outData);
	
	Uint8 *name;
	Uint32 nameSize;
	_FSFindPathParentName(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, name, nameSize);
	
	if (nameSize > inMaxSize) nameSize = inMaxSize;
	::CopyMemory(outData, name, nameSize);
	
	if (outEncoding) *outEncoding = 0;
	
	return nameSize;
}

/*
 * GetNumberName() takes a file or folder name and starting number,
 * appends a number to the name to produce a unique name for the
 * specified folder, and returns the number that was used.
 */
Uint16 UFileSys::GetNumberName(TFSRefObj* inRef, Uint16 inNum, Uint8 *ioName, Uint32 /* inEncoding */)
{
	Uint8 *p, *buf;
	Uint32 n;
	Int8 name[256];
	Int8 ext[8];
	
	Require(inRef);
	
	if (ioName[0] > 3 && ioName[ioName[0]-3] == '.')	// if has extension
	{
		p = ioName + (ioName[0]-2);
		ext[0] = '.';
		ext[1] = p[0];
		ext[2] = p[1];
		ext[3] = p[2];
		ext[4] = 0;
		
		n = ioName[0] - 4;
		if (n > 32) n = 32;
		::CopyMemory(name, ioName+1, n);
		name[n] = 0;
	}
	else
	{
		ext[0] = 0;
		n = ioName[0];
		if (n > 32) n = 32;
		::CopyMemory(name, ioName+1, n);
		name[n] = 0;
	}
	
	n = ((SFileSysRef *)inRef)->pathSize;
	buf = (Uint8 *)UMemory::New(n + 64);
	::CopyMemory(buf, ((SFileSysRef *)inRef)->path, n);
	p = buf + n;
	if (p[-1] != '\\') *p++ = '\\';
	
	do {
		n = wsprintf((char *)p, "%s%hu%s", name, inNum, ext);
		p[n] = 0;
		inNum++;
	} while (::GetFileAttributes((char *)buf) != 0xFFFFFFFF);	// while not error (while exists)
	
	::CopyMemory(ioName+1, p, n);
	ioName[0] = n;
	
	UMemory::Dispose((TPtr)buf);
	
	return inNum;
}

// returns new size of name, which will not be greater than the input size
Uint32 UFileSys::ValidateFileName(void *ioName, Uint32 inSize, Uint32 /* inEncoding */)
{
	if (inSize > 255) 
		inSize = 255;
	
	Uint8 *p = (Uint8 *)ioName;
	Uint32 n = inSize;
	
	// first char can't be period
	if (n && *p == '.') 
		*p = '-';

	while (n--)
	{
		switch (*p)
		{
			case NULL:
			case '\\':
			case '/':
			case ':':
			case '*':
			case '?':
			case '\"':
			case '<':
			case '>':
			case '|':
			
			// these fail when tried to create, so should map them out too.
			case 0x01:	case 0x02:	case 0x03:	case 0x04:
			case 0x05:	case 0x06:	case 0x07:	case 0x08:
			case 0x09:	case 0x0A:	case 0x0B:	case 0x0C:
			case 0x0D:	case 0x0E:	case 0x0F:	case 0x10:
			case 0x11:	case 0x12:	case 0x13:	case 0x14:
			case 0x15:	case 0x16:	case 0x17:	case 0x18:
			case 0x19:	case 0x1A:	case 0x1B:	case 0x1C:
			case 0x1D:	case 0x1E:	case 0x1F:	case 0x81:
			case 0x8D:	case 0x8E:	case 0x8F:	case 0x90:
			case 0x9D:	case 0x9E:	
			
				*p = '-';
				break;
		};

		p++;
	}
	
	// now replace any trailing .s and spaces with - (since windows strips these out)
	n = inSize;
	
	while(n--)
	{
		switch(*--p)
		{
			case '.':
			case ' ':
			
				*p = '-';
				break;
				
			default:
				goto done;
		};
	}
	
done:	
	
	return inSize;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// if move successful, source ref is updated to point to item in new location
void UFileSys::Move(TFSRefObj* inRef, TFSRefObj* inDestFolder)
{
	Int8 *newPath, *oldPath, *destDir;
	Uint8 *name;
	Uint32 destDirSize, nameSize, newPathSize;
	bool addSlash;
	
	Require(inRef && inDestFolder);
	
	destDir = ((SFileSysRef *)inDestFolder)->path;
	destDirSize = ((SFileSysRef *)inDestFolder)->pathSize;
	addSlash = (destDir[destDirSize-1] != '\\');
	
	oldPath = ((SFileSysRef *)inRef)->path;
	_FSFindPathTargetName(oldPath, ((SFileSysRef *)inRef)->pathSize, name, nameSize);
	
	newPathSize = destDirSize + nameSize + (addSlash ? 1 : 0);
	newPath = (Int8 *)UMemory::New(newPathSize + 1);	// +1 is for the null
	
	::CopyMemory(newPath, destDir, destDirSize);
	if (addSlash)
	{
		newPath[destDirSize] = '\\';
		destDirSize++;
	}
	
	::CopyMemory(newPath + destDirSize, name, nameSize);
	newPath[newPathSize] = 0;
	
	if (!::MoveFileA(oldPath, newPath))	// don't use MoveFileEx because it's not implemented on win95
	{
		DWORD err = ::GetLastError();	// must get before dispose so not overwritten
		UMemory::Dispose((TPtr)newPath);
		FailWinError(err);
	}
	
	UMemory::Dispose((TPtr)oldPath);
	((SFileSysRef *)inRef)->path = newPath;
	((SFileSysRef *)inRef)->pathSize = newPathSize;
}

// if move successful, inRef is updated to point to item in new location
// if inNewName is not nil, inDest is the destination folder
// if inNewName is nil, the name from inDest is used (inDest should point to a non-existant file)
void UFileSys::MoveAndRename(TFSRefObj* inRef, TFSRefObj* inDest, const Uint8 *inNewName, Uint32 /* inEncoding */)
{
	Int8 *destPath, *newPath, *oldPath;
	Uint32 destPathSize, newPathSize;
	DWORD err;
	bool addSlash;

	Require(inRef && inDest);
	
	destPath = ((SFileSysRef *)inDest)->path;
	destPathSize = ((SFileSysRef *)inDest)->pathSize;
	oldPath = ((SFileSysRef *)inRef)->path;
	
	if (inNewName == nil)
	{
		newPath = (Int8 *)UMemory::New(destPath, destPathSize+1);	// +1 is for the null
		
		if (!::MoveFileA(oldPath, newPath))	// don't use MoveFileEx because it's not implemented on win95
		{
			err = ::GetLastError();
			UMemory::Dispose((TPtr)newPath);
			FailWinError(err);
		}
		
		UMemory::Dispose((TPtr)oldPath);
		((SFileSysRef *)inRef)->path = newPath;
		((SFileSysRef *)inRef)->pathSize = destPathSize;
	}
	else
	{
		addSlash = (destPath[destPathSize-1] != '\\');
		newPathSize = destPathSize + inNewName[0] + (addSlash ? 1 : 0);
		newPath = (Int8 *)UMemory::New(newPathSize + 1);	// +1 is for the null
		
		::CopyMemory(newPath, destPath, destPathSize);
		if (addSlash)
		{
			newPath[destPathSize] = '\\';
			destPathSize++;
		}
		
		::CopyMemory(newPath + destPathSize, inNewName+1, inNewName[0]);
		newPath[newPathSize] = 0;
		
		if (!::MoveFileA(oldPath, newPath))	// don't use MoveFileEx because it's not implemented on win95
		{
			err = ::GetLastError();	// must get before dispose so not overwritten
			UMemory::Dispose((TPtr)newPath);
			FailWinError(err);
		}
		
		UMemory::Dispose((TPtr)oldPath);
		((SFileSysRef *)inRef)->path = newPath;
		((SFileSysRef *)inRef)->pathSize = newPathSize;
	}
}

// swaps the data for each file (but name and location stay the same)
void UFileSys::SwapData(TFSRefObj* inRefA, TFSRefObj* inRefB)
{
	#pragma unused(inRefA, inRefB)
	Fail(errorType_Misc, error_Unimplemented);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// fails if already open
void UFileSys::Open(TFSRefObj* inRef, Uint32 inPerm)
{
	DWORD access, share;
	HANDLE h;

	Require(inRef);
	if (((SFileSysRef *)inRef)->h) Fail(errorType_Misc, error_Protocol);
	
	_FSAccessToWinAccess(inPerm, access, share);

	h = ::CreateFileA(((SFileSysRef *)inRef)->path, access, share, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) FailLastWinError();

	((SFileSysRef *)inRef)->h = h;
}

// does nothing if already closed
// also stops any flatten/unflatten
void UFileSys::Close(TFSRefObj* inRef)
{
	// note that should not throw exceptions in a cleanup routine such as this
	if (inRef)
	{
		if (((SFileSysRef *)inRef)->h)
		{
			::CloseHandle(((SFileSysRef *)inRef)->h);
			((SFileSysRef *)inRef)->h = nil;
		}
		
		if (((SFileSysRef *)inRef)->flatData)
		{
			UMemory::Dispose(((SFileSysRef *)inRef)->flatData);
			((SFileSysRef *)inRef)->flatData = nil;
		}
		
		if (((SFileSysRef *)inRef)->unflatData)
		{
			UMemory::Dispose(((SFileSysRef *)inRef)->unflatData);
			((SFileSysRef *)inRef)->unflatData = nil;
		}
	}
}

Uint32 UFileSys::Read(TFSRefObj* inRef, Uint32 inOffset, void *outData, Uint32 inMaxSize, Uint32 /* inOptions */)
{
	HANDLE h;
	
	Require(inRef);
	h = ((SFileSysRef *)inRef)->h;
	if (h == NULL) Fail(errorType_Misc, error_Protocol);

	if (::SetFilePointer(h, inOffset, NULL, FILE_BEGIN) == 0xFFFFFFFF)
		FailLastWinError();
	
	if (!::ReadFile(h, outData, inMaxSize, &inMaxSize, NULL))
		FailLastWinError();
	
	return inMaxSize;
}

Uint32 UFileSys::Write(TFSRefObj* inRef, Uint32 inOffset, const void *inData, Uint32 inDataSize, Uint32 /* inOptions */)
{
	HANDLE h;
	
	Require(inRef);
	h = ((SFileSysRef *)inRef)->h;
	if (h == NULL) Fail(errorType_Misc, error_Protocol);

	if (::SetFilePointer(h, inOffset, NULL, FILE_BEGIN) == 0xFFFFFFFF)
		FailLastWinError();
	
	if (!::WriteFile(((SFileSysRef *)inRef)->h, inData, inDataSize, &inDataSize, NULL))
		FailLastWinError();

	return inDataSize;
}

Uint32 UFileSys::GetSize(TFSRefObj* inRef, Uint32 /* inOptions */)
{
	Uint32 s;
	
	Require(inRef);
	
	if (((SFileSysRef *)inRef)->h == NULL)
	{
		HANDLE h = ::CreateFileA(((SFileSysRef *)inRef)->path, GENERIC_READ, FILE_SHARE_READ + FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (h == INVALID_HANDLE_VALUE) FailLastWinError();
		
		s = ::GetFileSize(h, NULL);
		if (s == 0xFFFFFFFF)
		{
			s = ::GetLastError();
			::CloseHandle(h);
			FailWinError(s);
		}
		
		::CloseHandle(h);
	}
	else
	{
		s = ::GetFileSize(((SFileSysRef *)inRef)->h, NULL);
		if (s == 0xFFFFFFFF) FailLastWinError();
	}
	
	return s;
}

void UFileSys::SetSize(TFSRefObj* inRef, Uint32 inSize)
{
	HANDLE h;
	
	Require(inRef);
	h = ((SFileSysRef *)inRef)->h;
	if (h == NULL) Fail(errorType_Misc, error_Protocol);

	if (::SetFilePointer(h, inSize, NULL, FILE_BEGIN) == 0xFFFFFFFF)
		FailLastWinError();
	
	if (!::SetEndOfFile(h))
		FailLastWinError();
}

void UFileSys::Flush(TFSRefObj* inRef)
{
	if (inRef && ((SFileSysRef *)inRef)->h)
		::FlushFileBuffers(((SFileSysRef *)inRef)->h);
}

// returns nil if no data
THdl UFileSys::ReadToHdl(TFSRefObj* inRef, Uint32 inOffset, Uint32& ioSize, Uint32 /* inOptions */)
{
	Require(inRef);

	Uint32 dataSize;
	Uint32 s = ioSize;
	Uint32 ns;
	HANDLE refNum = ((SFileSysRef *)inRef)->h;
	
	if (refNum == NULL) Fail(errorType_Misc, error_Protocol);
	
	dataSize = ::GetFileSize(refNum, NULL);
	if (dataSize == 0xFFFFFFFF) FailLastWinError();
	
	if (inOffset >= dataSize)
	{
		ioSize = 0;
		return nil;
	}
	
	if (inOffset + s > dataSize)
		s = dataSize - inOffset;
	
	if (s == 0)
	{
		ioSize = 0;
		return nil;
	}
	
	THdl h = UMemory::NewHandle(s);
	
	try
	{
		{
			void *p;
			StHandleLocker lock(h, p);

			if (::SetFilePointer(refNum, inOffset, NULL, FILE_BEGIN) == 0xFFFFFFFF)
				FailLastWinError();
			
			if (!::ReadFile(refNum, p, s, &ns, NULL))
				FailLastWinError();
		}
		
		if (ns < s)
		{
			if (ns == 0)
			{
				UMemory::Dispose(h);
				ioSize = 0;
				return nil;
			}
			UMemory::SetSize(h, ns);
		}
	}
	catch(...)
	{
		UMemory::Dispose(h);
		throw;
	}
	
	ioSize = ns;
	return h;
}


bool UFileSys::Copy(TFSRefObj* inSource, TFSRefObj* ioDest, Uint32 inDestTypeCode, Uint32 inDestCreatorCode)
{
	if (!inSource || !inSource->Exists() || !ioDest)
		return false;
	
	StFileOpener sourceFileOpener(inSource, perm_Read);

	if (!ioDest->Exists())
		ioDest->CreateFile(inDestTypeCode, inDestCreatorCode);

	StFileOpener destFileOpener(ioDest, perm_ReadWrite);
	
	Uint32 nMaxBufferSize = 524288;	// 512K
	Uint32 nSourceSize = inSource->GetSize();
	
	Uint32 nBufferSize;
	if (nSourceSize > nMaxBufferSize)
		nBufferSize = nMaxBufferSize;
	else
		nBufferSize = nSourceSize;
	
	void *pBuffer = UMemory::New(nBufferSize);
	if (!pBuffer)	
		return false; 				
				
	Uint32 nOffset = 0;
	while (nOffset < nSourceSize)
	{
		Uint32 nReadSize;
		if (nOffset + nBufferSize > nSourceSize)
			nReadSize = nSourceSize - nOffset;
		else
			nReadSize = nBufferSize;
		
		inSource->Read(nOffset, pBuffer, nReadSize);
		ioDest->Write(nOffset, pBuffer, nReadSize);
		nOffset += nReadSize;
	}
				
	UMemory::Dispose((TPtr)pBuffer);
	ioDest->SetSize(nSourceSize);
	
	return true;
}


/* -------------------------------------------------------------------------- */
#pragma mark -

Uint16 UFileSys::GetType(TFSRefObj* inRef)
{
	Require(inRef);
	
	DWORD attrib = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
	if (attrib == 0xFFFFFFFF)	// if error
		return _FSParentExists(((SFileSysRef *)inRef)->path) ? fsItemType_NonExistant : fsItemType_NonExistantParent;
	
	if (attrib & FILE_ATTRIBUTE_DIRECTORY)
		return fsItemType_Folder;
	else if (_FSIsLinkFile(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize))
	{
		// we have a link
		Int8 resolvedPath[MAX_PATH];
		WIN32_FIND_DATA resolvedInfo;
		if (_FSResolveLink(NULL, ((SFileSysRef *)inRef)->path, resolvedPath, &resolvedInfo) == 0)
			return (resolvedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? fsItemType_FolderAlias : fsItemType_FileAlias;
		else	// couldn't resolve link
			return fsItemType_UnattachedAlias;
	}
	
	return fsItemType_File;
}

void UFileSys::SetComment(TFSRefObj* /* inRef */, const void */* inText */, Uint32 /* inTextSize */, Uint32 /* inEncoding */)
{
	// do nothing
}

Uint32 UFileSys::GetComment(TFSRefObj* /* inRef */, void */* outText */, Uint32 /* inMaxSize */, Uint32 *outEncoding)
{
	// do nothing
	if (outEncoding) *outEncoding = 0;
	return 0;
}

void UFileSys::SetTypeAndCreatorCode(TFSRefObj* /* inRef */, Uint32 /* inTypeCode */, Uint32 /* inCreatorCode */)
{
	// do nothing
}

void UFileSys::GetTypeAndCreatorCode(TFSRefObj* inRef, Uint32& outTypeCode, Uint32& outCreatorCode)
{
	Uint8 ext[16];

	Require(inRef);
	
	_FSGetPathExtension(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, ext);
	_FSWinExtToMacTypeCode(ext, outTypeCode, outCreatorCode);
}

Uint32 UFileSys::GetTypeCode(TFSRefObj* inRef)
{
	Uint8 ext[16];
	Uint32 typeCode, creatorCode;

	Require(inRef);
	
	_FSGetPathExtension(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, ext);
	_FSWinExtToMacTypeCode(ext, typeCode, creatorCode);
	
	return typeCode;
}

Uint32 UFileSys::GetCreatorCode(TFSRefObj* inRef)
{
	Uint8 ext[16];
	Uint32 typeCode, creatorCode;

	Require(inRef);
	
	_FSGetPathExtension(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, ext);
	_FSWinExtToMacTypeCode(ext, typeCode, creatorCode);
	
	return creatorCode;
}

void UFileSys::GetDateStamp(TFSRefObj* inRef, SDateTimeStamp *outModified, SDateTimeStamp *outCreated)
{
	Require(inRef);
	
	if (((SFileSysRef *)inRef)->h == NULL)
	{
		WIN32_FIND_DATA info;
		
		HANDLE h = ::FindFirstFile(((SFileSysRef *)inRef)->path, &info);
		
		if (h == INVALID_HANDLE_VALUE) Fail(errorType_FileSys, fsError_NoSuchItem);
		
		::FindClose(h);
		
		if (outCreated) _FSWinTimeToStamp(info.ftCreationTime, *outCreated);
		if (outModified) _FSWinTimeToStamp(info.ftLastWriteTime, *outModified);
		
		// the following method sucks because you have to open and close the file, and it doesn't work for directories
		/*
		h = ::CreateFileA(((SFileSysRef *)inRef)->path, GENERIC_READ, FILE_SHARE_READ + FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (h == INVALID_HANDLE_VALUE) FailLastWinError();
		
		if (!::GetFileTime(h, &createTime, NULL, &modTime))
		{
			DWORD err = ::GetLastError();
			::CloseHandle(h);
			FailWinError(err);
		}

		::CloseHandle(h);
		*/
	}
	else
	{
		FILETIME createTime, modTime;

		if (!::GetFileTime(((SFileSysRef *)inRef)->h, &createTime, NULL, &modTime))
			FailLastWinError();

		if (outCreated) _FSWinTimeToStamp(createTime, *outCreated);
		if (outModified) _FSWinTimeToStamp(modTime, *outModified);
	}
	
}

bool UFileSys::IsHidden(TFSRefObj* inRef)
{
	DWORD attrib;
	
	Require(inRef);
	
	attrib = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
	if (attrib == 0xFFFFFFFF) return false;		// error
	
	return (attrib & FILE_ATTRIBUTE_HIDDEN) != 0;
}

/* not a good func when you think of it in terms of aliases
bool UFileSys::IsChildOf(TFSRefObj* inRef, TFSRefObj* inFolder)
{
	Require(inRef);
	Require(inFolder);
	
	Uint32 foldLen = strlen(((SFileSysRef *)inFolder)->path);
	
	if (foldLen > strlen(((SFileSysRef *)inRef)->path))
		return false;
		
	return UMemory::Equal(((SFileSysRef *)inFolder)->path, ((SFileSysRef *)inRef)->path, foldLen);
}
*/
bool UFileSys::Equals(TFSRefObj* inRef, TFSRefObj* inCompare)
{
	Require(inRef);
	Require(inCompare);
	
	Uint32 foldLen = strlen(((SFileSysRef *)inCompare)->path);
	
	if (foldLen != strlen(((SFileSysRef *)inRef)->path))
		return false;
		
	return UMemory::Equal(((SFileSysRef *)inCompare)->path, ((SFileSysRef *)inRef)->path, foldLen);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

HWND _GetModalParentWindow();
bool _StandardDialogBox(Uint8 inActive = 0);

TFSRefObj* UFileSys::UserSelectFile(TFSRefObj* inInitianFolder, const Uint32 */* inTypeList */, Uint32 /* inOptions */)
{
	if (_StandardDialogBox()) // if a dialog box is already opened
		return nil;

	OPENFILENAME info;
	Int8 filePath[4096];
	filePath[0] = 0;
	
	// set initial folder
	info.lpstrInitialDir = NULL;
	if (inInitianFolder)
		info.lpstrInitialDir = ((SFileSysRef *)inInitianFolder)->path;

	info.lStructSize = sizeof(OPENFILENAME);
	info.hwndOwner = _GetModalParentWindow();
	info.hInstance = NULL;
	info.lpstrFilter = NULL;
	info.lpstrCustomFilter = NULL;
	info.nMaxCustFilter = 0;
	info.nFilterIndex = 0;
	info.lpstrFile = filePath;
	info.nMaxFile = sizeof(filePath);
	info.lpstrFileTitle = NULL;
	info.nMaxFileTitle = 0;
	info.lpstrTitle = NULL;
	info.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_LONGNAMES | OFN_HIDEREADONLY;
	info.nFileOffset = 0;
	info.nFileExtension = 0;
	info.lpstrDefExt = NULL;
	info.lCustData = 0;
	info.lpfnHook = NULL;
	info.lpTemplateName = NULL;
	
	_StandardDialogBox(1);
	if (!::GetOpenFileName(&info))
	{
		_StandardDialogBox(2);
		return nil;
	}
		
	_StandardDialogBox(2);

	Uint8 *p = (Uint8 *)filePath;
	Uint32 s = 0;
	while (*p++) ++s;

	SFileSysRef *ref;
	p = (Uint8 *)UMemory::New(filePath, s+1);	// +1 is for the null
	try
	{
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		
		ref->path = (Int8 *)p;
		ref->pathSize = s;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)p);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

TFSRefObj* UFileSys::UserSelectFolder(TFSRefObj* inInitianFolder, Uint32 /* inOptions */)
{
	if (_StandardDialogBox()) // if a dialog box is already opened
		return nil;

	LPMALLOC pMalloc;
	if (SHGetMalloc(&pMalloc) == E_FAIL) 
		pMalloc = NULL;

	BROWSEINFO info;
	Int8 folderName[MAX_PATH];
	Int8 folderPath[MAX_PATH];
	
	ITEMIDLIST *pIdRoot = NULL;
	if (inInitianFolder)
	{
	    IShellFolder *pShellFolder;
    	if (::SHGetDesktopFolder(&pShellFolder) == NOERROR)
	    {
			WCHAR widePath[MAX_PATH];
			::MultiByteToWideChar(CP_ACP, 0, ((SFileSysRef *)inInitianFolder)->path, -1, widePath, sizeof(widePath)/sizeof(WCHAR));
	
			pShellFolder->ParseDisplayName(NULL, NULL, widePath, NULL, &pIdRoot, NULL);
			pShellFolder->Release();
    	}
    }
   
    info.hwndOwner = _GetModalParentWindow(); 
   	info.pidlRoot = pIdRoot; 
    info.pszDisplayName = folderName; 
    info.lpszTitle = NULL; 
    info.ulFlags = BIF_RETURNONLYFSDIRS; 
    info.lpfn = NULL; 
    info.lParam = 0; 
    info.iImage = 0; 

	_StandardDialogBox(1);
	ITEMIDLIST *pIdList = ::SHBrowseForFolder(&info);
	_StandardDialogBox(2);
	
	if (pMalloc && pIdRoot)
		pMalloc->Free(pIdRoot);
	
	if (!pIdList || !::SHGetPathFromIDList(pIdList, folderPath))
	{
		if (pMalloc && pIdList) 
			pMalloc->Free(pIdList);

		return nil;
	}

	if (pMalloc) 
		pMalloc->Free(pIdList);
	
	Uint8 *p = (Uint8 *)folderPath;
	Uint32 s = 0;
	while (*p++) ++s;

	SFileSysRef *ref;
	p = (Uint8 *)UMemory::New(folderPath, s+1);	// +1 is for the null
	try
	{
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		
		ref->path = (Int8 *)p;
		ref->pathSize = s;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)p);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

TFSRefObj* UFileSys::UserSaveFile(TFSRefObj* inInitianFolder, const Uint8 *inDefaultName, const Uint8 *inPrompt, Uint32 /* inNameEncoding */, Uint32 /* inPromptEncoding */, Uint32 /* inOptions */)
{
	if (_StandardDialogBox()) // if a dialog box is already opened
		return nil;

	OPENFILENAME info;
	Int8 filePath[4096];
	Int8 title[256];
	Int8 defExt[16];
	
	if (inDefaultName)
	{
		::CopyMemory(filePath, inDefaultName+1, inDefaultName[0]);
		filePath[UFileSys::ValidateFileName(filePath, inDefaultName[0])] = 0;
	}
	else
		filePath[0] = 0;
	
	if (inPrompt)
	{
		::CopyMemory(title, inPrompt+1, inPrompt[0]);
		title[inPrompt[0]] = 0;
		info.lpstrTitle = title;
	}
	else
		info.lpstrTitle = NULL;
	
	// get default extension
	info.lpstrDefExt = NULL;
	if (inDefaultName)
	{
		Uint8 *p = UMemory::SearchByteBackwards('.', inDefaultName+1, inDefaultName[0]);
		if (p)
		{
			p++;
			Uint32 s = (inDefaultName + inDefaultName[0] + 1) - p;
			if (s > sizeof(defExt)-1) s = sizeof(defExt)-1;
			
			if (s)
			{
				::CopyMemory(defExt, p, s);
				defExt[s] = 0;
				info.lpstrDefExt = defExt;
			}
		}
	}
	
	// set initial folder
	info.lpstrInitialDir = NULL;
	if (inInitianFolder)
		info.lpstrInitialDir = ((SFileSysRef *)inInitianFolder)->path;

	info.lStructSize = sizeof(OPENFILENAME);
	info.hwndOwner = _GetModalParentWindow();
	info.hInstance = NULL;
	info.lpstrFilter = NULL;
	info.lpstrCustomFilter = NULL;
	info.nMaxCustFilter = 0;
	info.nFilterIndex = 0;
	info.lpstrFile = filePath;
	info.nMaxFile = sizeof(filePath);
	info.lpstrFileTitle = NULL;
	info.nMaxFileTitle = 0;
	info.Flags = OFN_LONGNAMES | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	info.nFileOffset = 0;
	info.nFileExtension = 0;
	info.lCustData = 0;
	info.lpfnHook = NULL;
	info.lpTemplateName = NULL;
	
	_StandardDialogBox(1);
	if (!::GetSaveFileName(&info))
	{
		_StandardDialogBox(2);
		return nil;
	}
	
	_StandardDialogBox(2);

	Uint8 *p = (Uint8 *)filePath;
	Uint32 s = 0;
	while (*p++) ++s;

	SFileSysRef *ref;
	p = (Uint8 *)UMemory::New(filePath, s+1);	// +1 is for the null
	try
	{
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
		
		ref->path = (Int8 *)p;
		ref->pathSize = s;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)p);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

/*
 * Returns nil if no items in folder.  Format of handle is:
 *
 *		Uint32 count;
 *		SFSListItem item[count];	// each item begins on four-byte boundary
 */
THdl UFileSys::GetListing(TFSRefObj* inRef, Uint32 inOptions)
{
	HANDLE hfind = NULL;
	Int8 *findPath = nil;
	THdl h = nil;
	Uint32 s, count, findPathSize;
	Uint8 *p;
	WIN32_FIND_DATA findData;
	WIN32_FIND_DATA resolvedFindData;
	Uint8 ext[16];
	Uint8 buf[1024];
	SFSListItem& item = *(SFSListItem *)buf;
	Int8 resolvedPath[MAX_PATH];
	
	Require(inRef);
	
	try
	{
		s = ((SFileSysRef *)inRef)->pathSize;
		findPath = (Int8 *)UMemory::New(s + MAX_PATH + 1);	// the +MAX_PATH is because we use the space when resolving aliases (see further down)
		::CopyMemory(findPath, ((SFileSysRef *)inRef)->path, s);
		if (s && findPath[s-1] == '\\')
		{
			findPath[s] = '*';
			findPath[s+1] = 0;
			s--;
		}
		else
		{
			findPath[s] = '\\';
			findPath[s+1] = '*';
			findPath[s+2] = 0;
		}
		findPathSize = s;

		hfind = ::FindFirstFile(findPath, &findData);
		//Le probleme du bug Path Name est entre ici
		//if (hfind == INVALID_HANDLE_VALUE) FailLastWinError();
		if (hfind == INVALID_HANDLE_VALUE) goto done;
		//et ici
		//DebugBreak("test");
		// ignore "." directory
		if (findData.cFileName[0] == '.' && findData.cFileName[1] == 0)
		{
			if (!::FindNextFile(hfind, &findData)) goto done;
		}
		
		// ignore ".." directory
		if (findData.cFileName[0] == '.' && findData.cFileName[1] == '.' && findData.cFileName[2] == 0)
		{
			if (!::FindNextFile(hfind, &findData)) goto done;
		}

		h = UMemory::NewHandle(sizeof(Uint32));
		count = 0;
		item.flags = item.rsvd = 0;

		do {
			// determine length of file name
			p = (Uint8 *)findData.cFileName;
			s = 0;
			while (*p++) ++s;
			if (s > 255) s = 255;

			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	// if a directory
			{
				item.typeCode = TB((Uint32)'fldr');
				item.creatorCode = 0;
				try
				{
					item.size = _FSCountDirItems(findPath, findPathSize, findData.cFileName, s);
				}
				catch(...)
				{
					item.size = 0;
					// don't throw
				}
				
				_FSWinTimeToStamp(findData.ftCreationTime, item.createdDate);
				_FSWinTimeToStamp(findData.ftLastWriteTime, item.modifiedDate);
			}
			else	// it's a file
			{
				if (_FSIsLinkFile(findData.cFileName, s))	// if an alias/link
				{
					::CopyMemory(findPath + findPathSize + 1, findData.cFileName, s+1);
					
					if (!(inOptions & kDontResolveAliases) && _FSResolveLink(NULL, findPath, resolvedPath, &resolvedFindData) == 0)	// if can resolve the link
					{
						// we resolved the link, get info on the original item
						if (resolvedFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)	// if a directory
						{
							item.typeCode = TB((Uint32)'fldr');
							item.creatorCode = 0;
							try
							{
								item.size = _FSCountDirItems(resolvedPath, strlen(resolvedPath), nil, 0);
							}
							catch(...)
							{
								item.size = 0;
								// don't throw
							}
						}
						else
						{
							_FSGetPathExtension(resolvedFindData.cFileName, strlen(resolvedFindData.cFileName), ext);
							_FSWinExtToMacTypeCode(ext, item.typeCode, item.creatorCode);
							item.size = resolvedFindData.nFileSizeLow;
						}
					
						_FSWinTimeToStamp(resolvedFindData.ftCreationTime, item.createdDate);
						_FSWinTimeToStamp(resolvedFindData.ftLastWriteTime, item.modifiedDate);
					}
					else
					{
						// list as unattached
						item.typeCode = TB((Uint32)'alis');
						item.creatorCode = 0;
						item.size = 0;
						_FSWinTimeToStamp(findData.ftCreationTime, item.createdDate);
						_FSWinTimeToStamp(findData.ftLastWriteTime, item.modifiedDate);
					}
				}
				else	// normal file
				{
					_FSGetPathExtension(findData.cFileName, s, ext);
					_FSWinExtToMacTypeCode(ext, item.typeCode, item.creatorCode);
					item.size = findData.nFileSizeLow;
					_FSWinTimeToStamp(findData.ftCreationTime, item.createdDate);
					_FSWinTimeToStamp(findData.ftLastWriteTime, item.modifiedDate);
				}
			}
			
			::CopyMemory(item.name+1, findData.cFileName, s);
			item.name[0] = s;
			
			s = RoundUp4(sizeof(SFSListItem) + item.name[0] + 1);
			UMemory::Append(h, buf, s);
			
			count++;
		} while (::FindNextFile(hfind, &findData));

		UMemory::WriteLong(h, 0, count);
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)findPath);
		UMemory::Dispose(h);
		if (hfind) ::FindClose(hfind);
		throw;
	}
	
done:
	UMemory::Dispose((TPtr)findPath);
	::FindClose(hfind);
	
	return h;
}

// outName must point to 256 bytes (or be nil)
bool UFileSys::GetListNext(THdl inListData, Uint32& ioOffset, Uint8 *outName, Uint32 *outTypeCode, Uint32 *outCreatorCode, Uint32 *outSize, SDateTimeStamp *outCreatedDate, SDateTimeStamp *outModifiedDate, Uint32 *outFlags)
{
	Uint8 *p;
	Uint32 maxSize;
	SFSListItem *item;
	
	if (inListData == nil) return false;
	StHandleLocker lock(inListData, (void*&)p);
	maxSize = UMemory::GetSize(inListData);
	
	if (ioOffset == 0) ioOffset = 4;		// skip count
	if ((ioOffset + sizeof(SFSListItem)) > maxSize) return false;
	
	item = (SFSListItem *)(p + ioOffset);
	
	if (outName) ::CopyMemory(outName, item->name, item->name[0]+1);
	
	if (outTypeCode)		*outTypeCode = item->typeCode;
	if (outCreatorCode)		*outCreatorCode = item->creatorCode;
	if (outSize)			*outSize = item->size;
	if (outCreatedDate)		*outCreatedDate = item->createdDate;
	if (outModifiedDate)	*outModifiedDate = item->modifiedDate;
	if (outFlags)			*outFlags = item->flags;
	
	ioOffset += (sizeof(SFSListItem) + item->name[0] + 4) & 0xFFFFFFFC;
	
	return true;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// returns true if now points to a valid file that is not an alias
// outOrigType is set to what type of FS item the spec was BEFORE it was resolved
bool UFileSys::ResolveAlias(TFSRefObj* inRef, Uint16 *outOrigType)
{
	DWORD attrib;
	
	Require(inRef);
		
	attrib = ::GetFileAttributes(((SFileSysRef *)inRef)->path);
	if (attrib == 0xFFFFFFFF)	// if error
	{
		if (outOrigType)
			*outOrigType = _FSParentExists(((SFileSysRef *)inRef)->path) ? fsItemType_NonExistant : fsItemType_NonExistantParent;
		return false;
	}
	
	if (attrib & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (outOrigType) *outOrigType = fsItemType_Folder;
		return true;
	}
	else	// it's a file
	{
		if (_FSIsLinkFile(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize))
		{
			// we have a link
			Int8 resolvedPath[MAX_PATH];
			WIN32_FIND_DATA resolvedInfo;
			if (_FSResolveLink(NULL, ((SFileSysRef *)inRef)->path, resolvedPath, &resolvedInfo) == 0)
			{
				if (outOrigType) *outOrigType = (resolvedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? fsItemType_FolderAlias : fsItemType_FileAlias;
				
				// update inRef to point to the target of the link
				Uint32 resolvedPathSize = strlen(resolvedPath);
				((SFileSysRef *)inRef)->path = (Int8 *)UMemory::New(resolvedPath, resolvedPathSize+1);
				((SFileSysRef *)inRef)->pathSize = resolvedPathSize;
			}
			else
			{
				// couldn't resolve link
				if (outOrigType) *outOrigType = fsItemType_UnattachedAlias;
				return false;
			}
		}
		else
		{
			// not a link
			if (outOrigType) *outOrigType = fsItemType_File;
		}
	}
	
	return true;
}

Uint32 UFileSys::GetApplicationPath(void *outAppPath, Uint32 inMaxPathSize)
{
	Require(outAppPath);

	Uint32 nPathSize = ::GetModuleFileName(NULL, (Int8 *)outAppPath, inMaxPathSize);
	
	Uint8 *pFileName;
	Uint32 nNameSize;
	_FSFindPathTargetName(outAppPath, nPathSize, pFileName, nNameSize);
	nPathSize -= nNameSize;	// lose the file name
	
	return nPathSize;
}

Uint32 UFileSys::GetApplicationURL(UInt8 *url, Uint32 urlZ)
{
	Require(url);
	
	UInt32 size = 8;
	{   // prepend file:///
		if (urlZ < size)
			return 0;
		for( UInt8 *b = (UInt8*)"file:///"; *b!=0; ++b)
			*url++ = *b;
	}

	Uint32 z = ::GetModuleFileName(NULL, (Int8 *)url, urlZ-size);
	{   // lose the app filename
		for( UInt8 *e=url+z-1; z>0 && *e != '\\'; --e, --z){}
		if (z > 0) --z; // skip the \ at end of path 
	}

	{ // convert dos to url chars	
	    if (z >= 2 && url[1] == ':')
	    	url[1] = '|';
		for( UInt8 *b = url, *e = b+z; b!=e; ++b)
			if (*b=='\\') *b='/';
		size += z;
	}
	
	return size;
}

Uint32 UFileSys::GetTempPath(void *outTempPath, Uint32 inMaxPathSize)
{
	Require(outTempPath);
	
	return ::GetTempPath(inMaxPathSize, (Int8 *)outTempPath);
}

bool UFileSys::IsDiskLocked(TFSRefObj* /*inRef*/)
{
	// assume not locked
	return false;
}

Uint64 UFileSys::GetDiskFreeSpace(TFSRefObj* inRef)
{
	Require(inRef && ((SFileSysRef *)inRef)->pathSize >= 3);

	Uint64 nFreeSpace = 0;
	Int8 *pPath = ((SFileSysRef *)inRef)->path;
	
	// determine if GetDiskFreeSpaceEx is available
	typedef BOOL (CALLBACK* LPPROCDLL_GetDiskFreeSpaceEx)(LPCTSTR, PULARGE_INTEGER, PULARGE_INTEGER, PULARGE_INTEGER);
	LPPROCDLL_GetDiskFreeSpaceEx lpProc_GetDiskFreeSpaceEx = (LPPROCDLL_GetDiskFreeSpaceEx)::GetProcAddress(::GetModuleHandle("KERNEL32.DLL"), "GetDiskFreeSpaceExA");
	
	if (lpProc_GetDiskFreeSpaceEx)	// use GetDiskFreeSpaceEx
	{
		bool bIsFolder;
		TFSRefObj* pFolder = inRef;
			
		// check if exists and is folder
		while (pFolder && (!UFileSys::Exists(pFolder, &bIsFolder) || !bIsFolder))
		{
			TFSRefObj* pTempFolder = pFolder;
			pFolder  = UFileSys::GetFolder(pTempFolder);

			if (pTempFolder != inRef)
				UFileSys::Dispose(pTempFolder);
		}

		if (pFolder)
			pPath = ((SFileSysRef *)pFolder)->path;

   		Uint64 nTotalBytes, nFreeBytes;
   		if (!lpProc_GetDiskFreeSpaceEx(pPath, (PULARGE_INTEGER)&nFreeSpace, (PULARGE_INTEGER)&nTotalBytes, (PULARGE_INTEGER)&nFreeBytes))
   		{
			if (pFolder != inRef)
				UFileSys::Dispose(pFolder);

   			FailLastWinError();
 		}
 			
		if (pFolder != inRef)
			UFileSys::Dispose(pFolder);
	}
	else 							// use GetDiskFreeSpace
	{
		Int8 csRoot[32];
		csRoot[0] = pPath[0];
		csRoot[1] = pPath[1];
		csRoot[2] = pPath[2];
		csRoot[3] = 0;
	
		DWORD nSectorsPerCluster, nBytesPerSector, nNumberOfFreeClusters, nTotalNumberOfClusters;
		if (!::GetDiskFreeSpaceA(csRoot, &nSectorsPerCluster, &nBytesPerSector, &nNumberOfFreeClusters, &nTotalNumberOfClusters))
			FailLastWinError();
	
		nFreeSpace = (Uint64)nNumberOfFreeClusters * ((Uint64)nBytesPerSector * (Uint64)nSectorsPerCluster);
	}

	return nFreeSpace;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

SFlattenRef *UFileSys::FlattenRef(TFSRefObj* inRef)
{
	Require(inRef);

	SFlattenRef *pFlattenRef = (SFlattenRef *)UMemory::New(sizeof(SFlattenRef) + ((SFileSysRef *)inRef)->pathSize);

	pFlattenRef->sig = TB((Uint32)'FSrf');
	pFlattenRef->vers = TB((Uint16)1);
	pFlattenRef->OS = TB((Uint32)'WIND');
	
	pFlattenRef->dataSize = TB(((SFileSysRef *)inRef)->pathSize);
	UMemory::Copy(pFlattenRef->data, ((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize);

	return pFlattenRef;
}

TFSRefObj* UFileSys::UnflattenRef(SFlattenRef *inFlattenRef)
{
	Require(inFlattenRef);

	if (inFlattenRef->sig != TB((Uint32)'FSrf')) Fail(errorType_Misc, error_FormatUnknown);
	if (inFlattenRef->vers != TB((Uint16)1)) Fail(errorType_Misc, error_VersionUnknown);
	if (inFlattenRef->OS != TB((Uint32)'WIND')) Fail(errorType_Misc, error_FormatUnknown);
		
	return _PathToRef(inFlattenRef->data, TB(inFlattenRef->dataSize));
}

/* -------------------------------------------------------------------------- */
#pragma mark -

/*
The file package format is a archive style, COMPACT format for transfering files
between platforms, over network connections and for archiving.

It looks like this:

	Uint32 format;				// always 'FILP'
	Uint16 version;				// currently 1
	Uint8 rsvd[16];				// should all be 0
	Uint16 forkCount;			// number of forks following
	struct {
		Uint32 forkType;		// type of fork, eg, 'DATA', 'INFO', 'MACR'
		Uint32 compType;		// compression type (0=none)
		Uint32 rsvd;			// should be 0
		Uint32 dataSize;		// number of bytes in following string
		Uint8 data[dataSize];	// data for this fork
	} forks[forkCount];
*/

#pragma options align=packed
struct SFlatFileHeader {
	Uint32 format;
	Uint16 version;
	Uint8 rsvd[16];
	Uint16 forkCount;
};

struct SFlatFileForkHeader {
	Uint32 forkType;
	Uint32 compType;
	Uint32 rsvd;
	Uint32 dataSize;
};

struct SFlatFileInfoFork {
	Uint32 platform;			// operating system, eg, 'AMAC', 'MWIN'
	Uint32 typeSig;				// file type signature
	Uint32 creatorSig;			// file creator signature
	Uint32 flags;
	Uint32 platFlags;			// platform specific flags
	Uint8 rsvd[32];
	SDateTimeStamp createDate;
	SDateTimeStamp modifyDate;
	Uint16 nameScript;
	Uint16 nameSize;
	Uint8 nameData[];
	// Uint16 commentSize;
	// Uint8 commentData[];
};

struct SFlatFileResumeEntry {
	Uint32 fork;
	Uint32 dataSize;
	Uint32 rsvdA, rsvdB;
};

struct SFlatFileResumeData {
	Uint32 format;				// always 'RFLT'
	Uint16 version;				// currently 1
	Uint8 rsvd[34];
	Uint16 count;
	SFlatFileResumeEntry forkInfo[];
};
#pragma options align=reset

// returns size of outInfo
static Uint32 _FSCreateFlatFileInfo(TFSRefObj* inRef, SFlatFileInfoFork& outInfo)
{
	Uint8 *p;
	Uint32 s;
	
	::FillMemory(&outInfo, sizeof(SFlatFileInfoFork), 0);
	
	outInfo.platform = TB((Uint32)0x4D57494E);	// 'MWIN';

	Uint8 ext[16];
	_FSGetPathExtension(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, ext);
	_FSWinExtToMacTypeCode(ext, outInfo.typeSig, outInfo.creatorSig);

	_FSFindPathTargetName(((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize, p, s);
	if (s > 63) s = 63;
	
	::CopyMemory(outInfo.nameData, p, s);
	outInfo.nameSize = TB((Uint16)s);
	
	p = outInfo.nameData + s;
	*(Uint16 *)p = 0;	// zero-length comment
		
	return sizeof(SFlatFileInfoFork) + s + sizeof(Uint16);
}

static void _FSSetFlatFileInfo(TFSRefObj* /*inRef*/, const SFlatFileInfoFork& /*inInfo*/, Uint32 /*inSize*/)
{
	// nothing to do
	// in the future, check inInfo's type/creator and map this to a three char extension if this file does not yet have one
}

struct SFileFlatten {
	Uint32 stage;
	Uint32 pos;
	Uint32 dataResumeSize;
	SFlatFileHeader hdr;
	SFlatFileForkHeader dataHdr;
	SFlatFileForkHeader infoHdr;
	SFlatFileInfoFork info;
	Uint8 infoNameBuf[128];
};

void UFileSys::StartFlatten(TFSRefObj* inRef)
{
	Require(inRef);
	if (((SFileSysRef *)inRef)->h) Fail(errorType_Misc, error_Protocol);

	HANDLE h = 0;
	SFileFlatten *finfo = (SFileFlatten *)UMemory::NewClear(sizeof(SFileFlatten));
	
	try
	{
		finfo->infoHdr.dataSize = TB(_FSCreateFlatFileInfo(inRef, finfo->info));
		
		finfo->infoHdr.forkType = TB((Uint32)0x494E464F);	// 'INFO';
		finfo->dataHdr.forkType = TB((Uint32)0x44415441);	// 'DATA';
		
		finfo->hdr.format = TB((Uint32)0x46494C50);			// 'FILP';
		finfo->hdr.version = TB((Uint16)1);
		finfo->hdr.forkCount = TB((Uint16)2);
		
		h = ::CreateFileA(((SFileSysRef *)inRef)->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (h == INVALID_HANDLE_VALUE) FailLastWinError();
		
		finfo->dataHdr.dataSize = TB(::GetFileSize(h, NULL));
		if (finfo->dataHdr.dataSize == 0xFFFFFFFF) FailLastWinError();
	}
	catch(...)
	{
		if (h != 0 && h != INVALID_HANDLE_VALUE) ::CloseHandle(h);
		UMemory::Dispose((TPtr)finfo);
		throw;
	}
	
	((SFileSysRef *)inRef)->flatData = (TPtr)finfo;
	((SFileSysRef *)inRef)->h = h;
}

void UFileSys::ResumeFlatten(TFSRefObj* inRef, const void *inResumeData, Uint32 inDataSize)
{
	Require(inRef && inDataSize >= sizeof(SFlatFileResumeData));

	SFlatFileResumeData *rd = (SFlatFileResumeData *)inResumeData;

	if (rd->format != TB((Uint32)0x52464C54)) Fail(errorType_Misc, error_FormatUnknown);
	if (rd->version != TB((Uint16)1)) Fail(errorType_Misc, error_VersionUnknown);

	StartFlatten(inRef);
	SFileFlatten *finfo = (SFileFlatten *)((SFileSysRef *)inRef)->flatData;
	
	Uint32 n = FB(rd->count);
	SFlatFileResumeEntry *entry = rd->forkInfo;
	
	while (n--)
	{
		if (entry->fork == TB((Uint32)0x44415441))	// 'DATA'
		{
			finfo->dataResumeSize = FB(entry->dataSize);
			if (finfo->dataResumeSize >= FB(finfo->dataHdr.dataSize))
				finfo->dataHdr.dataSize = 0;
			else
				finfo->dataHdr.dataSize = TB( (Uint32)(FB(finfo->dataHdr.dataSize) - finfo->dataResumeSize) );
		}
		entry++;
	}
}

void UFileSys::StopFlatten(TFSRefObj* inRef)
{
	if (inRef && ((SFileSysRef *)inRef)->flatData)
		Close(inRef);
}

Uint32 UFileSys::GetFlattenSize(TFSRefObj* inRef)
{
	Require(inRef);
	if (((SFileSysRef *)inRef)->flatData == nil) Fail(errorType_Misc, error_Protocol);
	
	SFileFlatten *finfo = (SFileFlatten *)((SFileSysRef *)inRef)->flatData;
	
	return sizeof(SFlatFileHeader) + (sizeof(SFlatFileForkHeader) * 2)
		+ FB(finfo->infoHdr.dataSize) + FB(finfo->dataHdr.dataSize);
}

Uint32 UFileSys::ProcessFlatten(TFSRefObj* inRef, void *outData, Uint32 inMaxSize)
{
	void *p;
	Uint32 totalSize, s, resumeSize;
	Uint32 bytesWritten = 0;
	
	Require(inRef);
	if (((SFileSysRef *)inRef)->flatData == nil) Fail(errorType_Misc, error_Protocol);
	SFileFlatten *ref = (SFileFlatten *)((SFileSysRef *)inRef)->flatData;

	/*
	 * Determine what and how we're going to write to the output buffer
	 */
determineAction:
	switch (ref->stage)
	{
		case 0:
			p = &ref->hdr;
			totalSize = sizeof(SFlatFileHeader);
			goto writeBuffer;
		
		case 1:
			p = &ref->infoHdr;
			totalSize = sizeof(SFlatFileForkHeader);
			goto writeBuffer;
		
		case 2:
			p = &ref->info;
			totalSize = FB(ref->infoHdr.dataSize);
			goto writeBuffer;
		
		case 3:
			p = &ref->dataHdr;
			totalSize = sizeof(SFlatFileForkHeader);
			goto writeBuffer;
		
		case 4:
			totalSize = FB(ref->dataHdr.dataSize);
			resumeSize = ref->dataResumeSize;
			goto writeFile;
			
		default:
			return bytesWritten;
	}
	
	/*
	 * Write the data from the file of <totalSize> bytes
	 */
writeFile:
	s = totalSize - ref->pos;
	
	if (s > inMaxSize) s = inMaxSize;
	
	if (::SetFilePointer(((SFileSysRef *)inRef)->h, resumeSize + ref->pos, NULL, FILE_BEGIN) == 0xFFFFFFFF)
		FailLastWinError();
	if (!::ReadFile(((SFileSysRef *)inRef)->h, outData, s, &s, NULL))
		FailLastWinError();
	
	ref->pos += s;
	
	if (ref->pos >= totalSize)
	{
		ref->stage++;
		ref->pos = 0;
	}

	goto continueOutput;

	/*
	 * Write the data at <p> of <totalSize> bytes
	 */
writeBuffer:
	s = totalSize - ref->pos;
	
	if (s > inMaxSize) s = inMaxSize;
	
	::CopyMemory(outData, BPTR(p) + ref->pos, s);
	
	ref->pos += s;
	
	if (ref->pos >= totalSize)
	{
		ref->stage++;
		ref->pos = 0;
	}

	goto continueOutput;
	
	/*
	 * If there's still space in the buffer, keep writing
	 */
continueOutput:
	bytesWritten += s;
	
	if (s < inMaxSize)
	{
		outData = BPTR(outData) + s;
		inMaxSize -= s;
		goto determineAction;
	}
	
	return bytesWritten;
}

bool UFileSys::IsFlattening(TFSRefObj* inRef)
{
	if (inRef) return ((SFileSysRef *)inRef)->flatData != nil;
	return false;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

struct SFileUnflatten {
	Uint32 stage;
	Uint32 pos;
	Uint32 dataResumeSize;
	Uint32 infoSize;
	SFlatFileHeader hdr;
	SFlatFileForkHeader forkHdr;
	SFlatFileInfoFork info;
	Uint8 infoNameBuf[520];
	bool gotInfoData;
};

void UFileSys::StartUnflatten(TFSRefObj* inRef)
{
	Require(inRef);
	if (((SFileSysRef *)inRef)->h) Fail(errorType_Misc, error_Protocol);

	SFileUnflatten *finfo = (SFileUnflatten *)UMemory::NewClear(sizeof(SFileUnflatten));
	
	HANDLE h = ::CreateFileA(((SFileSysRef *)inRef)->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		DWORD err = GetLastError();
		UMemory::Dispose((TPtr)finfo);
		FailWinError(err);
	}
	
	((SFileSysRef *)inRef)->unflatData = (TPtr)finfo;
	((SFileSysRef *)inRef)->h = h;
}

THdl UFileSys::ResumeUnflatten(TFSRefObj* inRef)
{
	THdl resumeDataHdl = nil;
	HANDLE h = 0;
	Uint32 s;
	
	Require(inRef);
	if (((SFileSysRef *)inRef)->h) 
		Fail(errorType_Misc, error_Protocol);
	
	SFileUnflatten *finfo = (SFileUnflatten *)
		UMemory::NewClear(sizeof(SFileUnflatten));
	
	try
	{
		h = ::CreateFileA(((SFileSysRef *)inRef)->path, 
						  GENERIC_READ | GENERIC_WRITE, 
						  FILE_SHARE_READ, NULL, 
						  OPEN_EXISTING, 0, NULL);
		if (h == INVALID_HANDLE_VALUE) 
			FailLastWinError();
		
		resumeDataHdl = UMemory::NewHandleClear(
			sizeof(SFlatFileResumeData) + sizeof(SFlatFileResumeEntry));
		
		SFlatFileResumeData *rd;
		StHandleLocker lock(resumeDataHdl, (void*&)rd);
		
		rd->format = TB((Uint32)0x52464C54);	// 'RFLT';
		rd->version = TB((Uint16)1);
		rd->count = TB((Uint16)2);
		
		s = ::GetFileSize(h, NULL);
		if (s == 0xFFFFFFFF) 
			FailLastWinError();

		rd->forkInfo[0].fork = TB((Uint32)0x44415441);	// 'DATA';
		rd->forkInfo[0].dataSize = TB(s);
		finfo->dataResumeSize = s;
	}
	catch(...)
	{
		if (h != 0 && h != INVALID_HANDLE_VALUE) 
			::CloseHandle(h);
		UMemory::Dispose(resumeDataHdl);
		UMemory::Dispose((TPtr)finfo);
		throw;
	}

	((SFileSysRef *)inRef)->unflatData = (TPtr)finfo;
	((SFileSysRef *)inRef)->h = h;
	
	return resumeDataHdl;
}

void UFileSys::StopUnflatten(TFSRefObj* inRef)
{
	if (inRef && ((SFileSysRef *)inRef)->unflatData)
		Close(inRef);
}

bool UFileSys::ProcessUnflatten(TFSRefObj* inRef, const void *inData, Uint32 inDataSize)
{
	Uint32 totalSize, s;
	void *p;
	
	Require(inRef);
	if (((SFileSysRef *)inRef)->unflatData == nil) Fail(errorType_Misc, error_Protocol);
	SFileUnflatten *finfo = (SFileUnflatten *)((SFileSysRef *)inRef)->unflatData;

	// this is not very good - we should really have a better way of determining when we're done
	if (inDataSize == 0)
	{
		if (finfo->gotInfoData)
		{
			_FSSetFlatFileInfo(inRef, finfo->info, finfo->infoSize);
		}
		return false;
	}
	
	/*
	 * Determine how we're going to process the supplied data
	 */
determineAction:
	switch (finfo->stage)
	{
		case 0:
			p = &finfo->hdr;
			totalSize = sizeof(SFlatFileHeader);
			goto readBuffer;
		
		case 1:
			if (finfo->hdr.format != TB((Uint32)0x46494C50)) Fail(errorType_Misc, error_FormatUnknown);
			if (finfo->hdr.version != TB((Uint16)1)) Fail(errorType_Misc, error_VersionUnknown);
			goto nextStage;
		
		case 2:
			p = &finfo->forkHdr;
			totalSize = sizeof(SFlatFileForkHeader);
			goto readBuffer;

		case 3:
			finfo->forkHdr.dataSize = FB(finfo->forkHdr.dataSize);
			finfo->forkHdr.forkType = FB(finfo->forkHdr.forkType);
			if (finfo->forkHdr.compType != 0)
			{
				DebugBreak("UFileSys::ProcessUnflatten - unknown compression type");
				Fail(errorType_Misc, error_FormatUnknown);
			}
			goto nextStage;

		case 4:
			totalSize = finfo->forkHdr.dataSize;
			switch (finfo->forkHdr.forkType)
			{
				case 0x494E464F:	// 'INFO'
					if (totalSize > sizeof(SFlatFileInfoFork) + sizeof(finfo->infoNameBuf))
						totalSize = sizeof(SFlatFileInfoFork) + sizeof(finfo->infoNameBuf);
					finfo->gotInfoData = true;
					finfo->infoSize = totalSize;
					p = &finfo->info;
					goto readBuffer;
				
				case 0x44415441:	// 'DATA'
					goto readFile;
					
				default:
					goto skipData;
			}

		case 5:
			if (inDataSize != 0)
			{
				finfo->stage = 2;			// process another fork
				goto determineAction;
			}
			
		default:
			if (finfo->gotInfoData)
			{
				_FSSetFlatFileInfo(inRef, finfo->info, finfo->infoSize);
			}
			return false;
	}

	
	/*
	 * Read <totalSize> bytes and write to buffer <p>
	 */
readBuffer:
	s = totalSize - finfo->pos;
	
	if (s > inDataSize) s = inDataSize;
	
	::CopyMemory(BPTR(p) + finfo->pos, inData, s);
	
	finfo->pos += s;
	
	if (finfo->pos >= totalSize)
	{
		finfo->stage++;
		finfo->pos = 0;
	}
	
	goto continueProcess;
	
	
	/*
	 * Read <totalSize> bytes and write to file
	 */
readFile:
	s = totalSize - finfo->pos;
	
	if (s > inDataSize) s = inDataSize;
	
	if (::SetFilePointer(((SFileSysRef *)inRef)->h, finfo->dataResumeSize + finfo->pos, NULL, FILE_BEGIN) == 0xFFFFFFFF)
		FailLastWinError();
	if (!::WriteFile(((SFileSysRef *)inRef)->h, inData, s, &s, NULL))
		FailLastWinError();
	
	finfo->pos += s;
	
	if (finfo->pos >= totalSize)
	{
		finfo->stage++;
		finfo->pos = 0;
	}

	goto continueProcess;
	
	
	/*
	 * Ignore and move past <totalSize> bytes from the input stream
	 */
skipData:
	s = totalSize - finfo->pos;
	
	if (s > inDataSize) s = inDataSize;
	
	finfo->pos += s;
	
	if (finfo->pos >= totalSize)
	{
		finfo->stage++;
		finfo->pos = 0;
	}
	
	goto continueProcess;
	
	
	/*
	 * Jump to the next stage
	 */
nextStage:
	s = 0;
	finfo->stage++;
	finfo->pos = 0;
	goto continueProcess;
	
	
	/*
	 * If there's still unread data, keep reading
	 */
continueProcess:
	if (s < inDataSize)
	{
		inData = BPTR(inData) + s;
		inDataSize -= s;
		goto determineAction;
	}
	
	return true;
}

bool UFileSys::IsUnflattening(TFSRefObj* inRef)
{
	if (inRef) return ((SFileSysRef *)inRef)->unflatData != nil;
	return false;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

static void _FSFindPathTargetName(const void *inPath, Uint32 inPathSize, Uint8*& outName, Uint32& outNameSize)
{
	Uint8 *p = (((Uint8 *)inPath) + inPathSize) - 1;
	Uint32 n = inPathSize;
	
	while (n--)
	{
		if (*p-- == '\\')
		{
			outName = p + 2;
			outNameSize = inPathSize - (n+1);
			return;
		}
	}
	
	outName = (Uint8 *)inPath;
	outNameSize = inPathSize;
}

static void _FSFindPathParentName(const void *inPath, Uint32 inPathSize, Uint8*& outName, Uint32& outNameSize)
{
	Uint8 *p = (((Uint8 *)inPath) + inPathSize) - 1;
	Uint32 n = inPathSize;
	
	while (n--)
	{
		if (*p-- == '\\')
		{
			Uint8 *lastSlash = p+1;
			
			while (n--)
			{
				if (*p-- == '\\')
				{
					p += 2;
					outName = p;
					outNameSize = lastSlash - p;
					return;
				}
			}
			
			outName = (Uint8 *)inPath;
			outNameSize = lastSlash - (Uint8 *)inPath;
			return;
		}
	}
	
	outName = nil;
	outNameSize = 0;
}

// outExt is a pstring of the extension, up to 15 chars
static void _FSGetPathExtension(const void *inPath, Uint32 inPathSize, Uint8 *outExt)
{
	Uint8 *p, *ep;
	Uint32 s;
	
	_FSFindPathTargetName(inPath, inPathSize, p, s);
	ep = p + s;
	
	p = UMemory::SearchByteBackwards('.', p, s);
	
	if (p == nil)
	{
		outExt[0] = 0;
	}
	else
	{
		p++;
		
		s = ep - p;
		if (s > 15) s = 15;
		
		outExt[0] = s;
		UMemory::Copy(outExt+1, p, s);
	}
}

static bool _FSIsLinkFile(const void *inPath, Uint32 inPathSize)
{
	if (inPathSize > 4)
	{
		Uint8 *p = (BPTR(inPath) + inPathSize) - 4;
		return (p[0] == '.' && tolower(p[1]) == 'l' && tolower(p[2]) == 'n' && tolower(p[3]) == 'k');
	}
	
	return false;
}

static void _FSWinExtToMacTypeCode(const Uint8 *inExt, Uint32& outTypeCode, Uint32& outCreatorCode)
{
	if (inExt[0] && inExt[0] <= 3)
	{
		Uint32 n;
		
		enum {
	#if CONVERT_INTS
			byte0	= 3,
			byte1	= 2,
			byte2	= 1,
			byte3	= 0
	#else
			byte0	= 0,
			byte1	= 1,
			byte2	= 2,
			byte3	= 3
	#endif
		};
		
		if (inExt[0] == 1)
		{
			((Uint8 *)&n)[byte0] = '.';
			((Uint8 *)&n)[byte1] = toupper(inExt[1]);
			((Uint8 *)&n)[byte2] = ' ';
			((Uint8 *)&n)[byte3] = ' ';
		}
		else if (inExt[0] == 2)
		{
			((Uint8 *)&n)[byte0] = '.';
			((Uint8 *)&n)[byte1] = toupper(inExt[1]);
			((Uint8 *)&n)[byte2] = toupper(inExt[2]);
			((Uint8 *)&n)[byte3] = ' ';
		}
		else
		{
			((Uint8 *)&n)[byte0] = '.';
			((Uint8 *)&n)[byte1] = toupper(inExt[1]);
			((Uint8 *)&n)[byte2] = toupper(inExt[2]);
			((Uint8 *)&n)[byte3] = toupper(inExt[3]);
		}
		
		switch (n)
		{
// misc
			case '.TXT':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.RTF':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.URL':	outTypeCode = 'URL ';	outCreatorCode = 'url ';	break;
			case '.HTM':	outTypeCode = 'URL ';	outCreatorCode = 'url ';	break;
			case '.DLL':	outTypeCode = 'DLL ';	outCreatorCode = 'dll ';	break;
			
			case '.LOG':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.INI':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.CFG':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.INF':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.DOC':	outTypeCode = 'TEXT';	outCreatorCode = 'ttxt';	break;
			case '.PHP':	outTypeCode = 'URL ';	outCreatorCode = 'url ';	break;
			case '.SIT':	outTypeCode = 'SITD';	outCreatorCode = 'SIT!';	break;
			case '.DAT':	outTypeCode = 'BINA';	outCreatorCode = 'hDmp';	break;
			case '.EXE':	outTypeCode = 'DEXE';	outCreatorCode = 'CWIE';	break;
			case '.BAT':	outTypeCode = 'DEXE';	outCreatorCode = 'CWIE';	break;
			case '.ISO':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.DMG':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.IMG':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.NRG':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.MDS':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.CUE':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.BWT':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.CDI':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.B5T':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.CCD':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			case '.PDI':	outTypeCode = 'rohd';	outCreatorCode = 'ISO ';	break;
			
			case '.PDF':	outTypeCode = 'ttro';	outCreatorCode = 'PDF ';	break;
			
			case '.ZIP':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.ARC':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.ARJ':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.B64':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.BZ2':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.CAB':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.ICE':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.PK3':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.7Z ':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.GZ ':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.JAR':	outTypeCode = 'JAR ';	outCreatorCode = 'JAR ';	break;
			case '.WAR':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			
			case '.RAR':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			case '.ACE':	outTypeCode = 'ZIP ';	outCreatorCode = 'ZIP ';	break;
			//case '.DOC':	outTypeCode = 'W6BN';	outCreatorCode = 'MSWD';	break;
			case '.CPP':	outTypeCode = 'TEXT';	outCreatorCode = 'CWIE';	break;
			case '.C  ':	outTypeCode = 'TEXT';	outCreatorCode = 'CWIE';	break;
			case '.CP ':	outTypeCode = 'TEXT';	outCreatorCode = 'CWIE';	break;
			case '.H  ':	outTypeCode = 'TEXT';	outCreatorCode = 'CWIE';	break;
			case '.HPP':	outTypeCode = 'TEXT';	outCreatorCode = 'CWIE';	break;
			case '.HPF':	outTypeCode = 'HTft';	outCreatorCode = 'HTLC';	break;
			case '.HBM':	outTypeCode = 'HTbm';	outCreatorCode = 'HTLC';	break;
			case '.HNZ':	outTypeCode = 'HLNZ';	outCreatorCode = 'HTLC';	break;
// images
			case '.GIF':	outTypeCode = 'GIFf';	outCreatorCode = 'GKON';	break;
			case '.JPG':	outTypeCode = 'JPEG';	outCreatorCode = 'GKON';	break;
			case '.ICO':	outTypeCode = 'JPEG';	outCreatorCode = 'GKON';	break;
			
			case '.DIB':
			case '.BMP':	outTypeCode = 'BMP ';	outCreatorCode = 'GKON';	break;
			case '.PCT':	outTypeCode = 'PICT';	outCreatorCode = 'GKON';	break;
			case '.FPX':	outTypeCode = 'FPix';	outCreatorCode = 'TVOD';	break;
			case '.MAC':
			case '.PNT':	outTypeCode = 'PNTG';	outCreatorCode = 'TVOD';	break;
			case '.PSD':	outTypeCode = '8BPS';	outCreatorCode = 'TVOD';	break;
			case '.PNG':	outTypeCode = 'PNGf';	outCreatorCode = 'TVOD';	break;
			case '.SGI':	outTypeCode = 'SGI ';	outCreatorCode = 'TVOD';	break;
			case '.TGA':	outTypeCode = 'TPIC';	outCreatorCode = 'TVOD';	break;
			case '.TIF':	outTypeCode = 'TIFF';	outCreatorCode = 'GKON';	break;	
// video
			case '.QT ':
			case '.MOV':	outTypeCode = 'MooV';	outCreatorCode = 'TVOD';	break;
			case '.MPA':
			case '.MP2':
			case '.MPG':	outTypeCode = 'MPEG';	outCreatorCode = 'TVOD';	break;
			case '.AVI':	outTypeCode = 'VfW ';	outCreatorCode = 'TVOD';	break;
			case '.VOB':	outTypeCode = 'VfW ';	outCreatorCode = 'TVOD';	break;
			case '.MKV':	outTypeCode = 'VfW ';	outCreatorCode = 'TVOD';	break;
			case '.WMV':	outTypeCode = 'VfW ';	outCreatorCode = 'TVOD';	break;
			case '.OGM':	outTypeCode = 'VfW ';	outCreatorCode = 'TVOD';	break;
			
			case '.DV ':
			case '.DIF':	outTypeCode = 'dvc!';	outCreatorCode = 'TVOD';	break;
// 3D
			case '.SWF':	outTypeCode = 'SWFL';	outCreatorCode = 'SWF2';	break;
			case '.FLC':
			case '.FLI':	outTypeCode = 'FLI ';	outCreatorCode = 'TVOD';	break;
			case '.QD3':
			case '.3DM':	outTypeCode = '3DMF';	outCreatorCode = 'TVOD';	break;
// audio	
			case '.MP3':	outTypeCode = 'MP3 ';	outCreatorCode = 'MAmp';	break;
			case '.OGG':	outTypeCode = 'MP3 ';	outCreatorCode = 'OGG ';	break;
			case '.M4A':	outTypeCode = 'MP3 ';	outCreatorCode = 'MP4 ';	break;
			case '.CDA':	outTypeCode = 'MP3 ';	outCreatorCode = 'MP4 ';	break;
			case '.M3U':	outTypeCode = 'MP3 ';	outCreatorCode = 'MP4 ';	break;
			
			case '.RAM':
			case '.RM ':
			case '.RA ':
			case '.RAE':	outTypeCode = 'RAE ';	outCreatorCode = 'REAL';	break;
			case '.AIF':	outTypeCode = 'AIFF';	outCreatorCode = 'TVOD';	break;
			case '.AU ':
			case '.SND':
			case '.ULW':	outTypeCode = 'ULAW';	outCreatorCode = 'TVOD';	break;
			case '.WAV':	outTypeCode = 'WAVE';	outCreatorCode = 'TVOD';	break;
			case '.SD2':	outTypeCode = 'Sd2f';	outCreatorCode = 'TVOD';	break;
			case '.MOD':	outTypeCode = 'Sd2f';	outCreatorCode = 'TVOD';	break;
			case '.WMA':	outTypeCode = 'DRM!';	outCreatorCode = 'DRM!';	break;
			case '.VQF':	outTypeCode = 'YAMA';	outCreatorCode = 'YAMA';	break;
			
			case '.SMF':
			case '.KAR':
			case '.MID':	outTypeCode = 'MiDi';	outCreatorCode = 'TVOD';	break;

			default:
				outTypeCode = 'BINA';
				outCreatorCode = 'dosa';
				break;
		}
	}
	else
	{
		outTypeCode = 'BINA';
		outCreatorCode = 'dosa';
	}
	
	outTypeCode = TB(outTypeCode);
	outCreatorCode = TB(outCreatorCode);
}

// remove illegal characters from file name
static void _FSStripName(void *ioName, Uint32 inSize)
{
	Uint8 *p = (Uint8 *)ioName;
	
	// first char can't be period
	if (inSize && *p == '.') *p = '-';
	
	// strip slashes
	while (inSize--)
	{
		if (*p == '\\') *p = '-';
		p++;
	}
}

static void _FSAccessToWinAccess(Uint32 inPerm, Uint32& outAccess, Uint32& outShare)
{
	switch (inPerm & 0x0F)
	{
		case perm_Rd:
			outAccess = GENERIC_READ;
			break;
		case perm_Wr:
			outAccess = GENERIC_WRITE;
			break;
		case perm_RdWr:
			outAccess = GENERIC_READ | GENERIC_WRITE;
			break;
		default:
			outAccess = 0;
			break;
	}
	
	switch (inPerm & 0xF0)
	{
		case perm_NoneDenyRd:
			outShare = FILE_SHARE_WRITE;
			break;
		case perm_NoneDenyWr:
			outShare = FILE_SHARE_READ;
			break;
		case perm_NoneDenyRdWr:
			outShare = 0;
			break;
		default:
			outShare = FILE_SHARE_READ | FILE_SHARE_WRITE;
			break;
	}
}

static void _FSWinTimeToStamp(const FILETIME& inWinTime, SDateTimeStamp& outStamp)
{
	SYSTEMTIME st;
	
	if (::FileTimeToSystemTime(&inWinTime, &st))
	{
		static const Uint32 monthCumSecs[] = { 0, 2678400, 5097600, 7776000, 10368000, 13046400, 15638400, 18316800, 20995200, 23587200, 26265600, 28857600 };
		
		if (st.wMonth > 2 && UDateTime::IsLeapYear(calendar_Gregorian, st.wYear))
			st.wDay++;
		
		outStamp.year = st.wYear;
		outStamp.msecs = st.wMilliseconds;
		outStamp.seconds = monthCumSecs[st.wMonth-1] + ((st.wDay-1) * (24*60*60)) + (st.wHour * (60*60)) + (st.wMinute * 60) + st.wSecond;
	}
	else
	{
		outStamp.year = 1997;
		outStamp.msecs = 0;
		outStamp.seconds = 0;
	}
}

static bool _FSParentExists(const Int8 *inPath)
{
	Uint8 *p;
	Uint32 s;
	DWORD attrib;
	Uint8 saveByte;
	
	p = (Uint8 *)inPath;
	s = 0;
	while (*p++) ++s;
	
	_FSFindPathTargetName(inPath, s, p, s);
	
	saveByte = *p;
	*p = 0;
	attrib = ::GetFileAttributes(inPath);
	*p = saveByte;
	
	return (attrib != 0xFFFFFFFF) && (attrib & FILE_ATTRIBUTE_DIRECTORY);
}

// outData must point to 256 bytes
static Uint32 _FSConvertPathString(const Uint8 *inPath, void *outData)
{
	if (inPath == nil || inPath[0] == 0) return 0;
	
	Uint8 *sp = (Uint8 *)inPath + 1;
	Uint8 *dp = (Uint8 *)outData;
	Uint32 n = inPath[0];
	Uint8 c;
	
	// remove any preceding separator
	if (*sp == '\r')
	{
		sp++;
		n--;
		if (n == 0) return 0;
	}
	
	// remove any trailing separator
	if (sp[n-1] == '\r')
	{
		n--;
		if (n == 0) return 0;
	}
	
	while (n--)
	{
		c = *sp++;
		
		if (c == '\\')
			*dp++ = '-';
		else if (c == '\r')
			*dp++ = '\\';
		else
			*dp++ = c;
	}
	
	return dp - (Uint8 *)outData;
}

// converts path data to DOS\really\sucks format
// number of bytes written to outData will not exceed inPathSize
// returns number of bytes written
// output path has no preceding or trailing separators
static Uint32 _FSConvertPathData(const void *inPath, Uint32 inPathSize, void *outData)
{
	Uint32 count, s;
	Uint8 *p, *ep, *op;

	if (inPathSize < 6) return 0;
	
	count = FB( *(Uint16 *)inPath );
	p = ((Uint8 *)inPath) + sizeof(Uint16);
	ep = ((Uint8 *)inPath) + inPathSize;
	op = (Uint8 *)outData;
	
	while (count--)
	{
		p += sizeof(Uint16);
		if (p >= ep) break;		// check for corruption
		
		s = *p++;
		if (s == 0 || p + s > ep) break;	// check for corruption
		
		// disallow going up a directory (for security reasons)
		if (s == 2 && p[0] == '.' && p[1] == '.')
		{
			p += 2;
			continue;
		}
		
		while (s--)
		{
			Uint8 c = *p++;
			*op++ = (c == '\\') ? '-' : c;
		}

		*op++ = '\\';
	}
	
	s = op - (Uint8 *)outData;
	if (s) s--;		// don't include trailing separator
	return s;
}

// returns 0 if not found, on input, ioPathStr should be starting dir
static Uint32 _FSPathDataToActualDir(const void *inPathData, Uint32 inPathDataSize, void *ioPathStr, Uint32 inPathStrSize, Uint32 inMaxSize)
{
	Uint32 count, s;
	Uint8 *p, *ep, *op, *oep;
	WIN32_FIND_DATA wfd;
	
	// sanity checks
	ASSERT(inMaxSize >= MAX_PATH);
	if (inPathStrSize > inMaxSize) return 0;
	
	// get count of names in path-data, and if zero, we have nothing to do
	count = (inPathDataSize < 6) ? 0 : FB( *(Uint16 *)inPathData );
	if (count == 0) return inPathStrSize;				// path-string unmodified
	
	// gets ptrs to work with
	p = ((Uint8 *)inPathData) + sizeof(Uint16);
	ep = ((Uint8 *)inPathData) + inPathDataSize;
	op = ((Uint8 *)ioPathStr) + inPathStrSize;
	oep = ((Uint8 *)ioPathStr) + inMaxSize;
	
	// add separator
	if (op >= oep) return 0;
	if (inPathStrSize && op[-1] != '\\') *op++ = '\\';
	
	// loop thru the names in the pata-data
	while (count--)
	{
		// skip encoding and check for corruption
		p += sizeof(Uint16);
		if (p >= ep) break;
		
		// grab length of name and check for corruption
		s = *p++;
		if (s == 0 || p + s > ep) break;
		
	#if 0
		
		// disallow going up a directory (for security reasons)
		if (s == 2 && p[0] == '.' && p[1] == '.')
		{
			p += 2;
			continue;
		}
	#endif
	
		// check for .. anywhere in the path
		Uint8 *q = p;
		Uint32 len = s - 1;
		while (len--)
		{
			if (*q++ == '.')
			{
				if (*q == '.')
				{
					p += s;
					continue;
				}
			}
		}

		
		// check if enough space in buffer for current name + a slash and a null, and if not, abort, return not found
		if ((op + s + 2) > oep) return 0;
		
		// copy from name from path-data to path-string, stripping out slashes at the same time
		while (s--)
		{
			Uint8 c = *p++;
			*op++ = (c == '\\') ? '-' : c;
		}

		// if the current name is a link file, resolve the path
		if (_FSIsLinkFile(ioPathStr, op - BPTR(ioPathStr)))
		{
			*op = 0;		// null terminate path-string

			// resolve link file, if can't resolve or resolved item is not a directory, abort and return not found
			if (_FSResolveLink(NULL, (Int8 *)ioPathStr, (Int8 *)ioPathStr, &wfd) || !(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				return 0;	// not found
			
			// the path-string has completely changed, recalculate write position
			op = BPTR(ioPathStr) + strlen((Int8 *)ioPathStr);
			
			// make sure a double slash doesn't get written
			if (*(Int8 *)ioPathStr && op[-1] == '\\') op--;
		}

		// items in path-string get separated by a backslash
		*op++ = '\\';
	}
	
	// calc size of path-string and don't include any trailing backslash
	s = op - BPTR(ioPathStr);
	if (s && BPTR(ioPathStr)[s-1] == '\\') s--;

	// check if target exists and is directory
	BPTR(ioPathStr)[s] = 0;									// null terminate
	DWORD attrib = ::GetFileAttributes((Int8 *)ioPathStr);
	if (attrib == 0xFFFFFFFF || !(attrib & FILE_ATTRIBUTE_DIRECTORY)) return 0;

	// all done! return size of path-string
	return s;
}

// returns true if you should return nil
static bool _FSDoNewOptions(const Int8 *inPath, Int8 *outResolvedPath, Uint32 inOptions, Uint16 *outType)
{
	// ******** this function totally sucks, need to redo as per notes at top of UFileSys(M).cp

	WIN32_FIND_DATA resolvedInfo;
	DWORD attrib;
	Uint16 type;
	
	*outResolvedPath = 0;
	
	if (inOptions || outType)
	{
		// determine FS type
		attrib = ::GetFileAttributes(inPath);
		if (attrib == 0xFFFFFFFF)	// if error
		{
			if (inOptions & fsOption_ResolveAliases)
			{
				if (_FSResolveLinksInPath(NULL, inPath, outResolvedPath, &resolvedInfo) == 0)
				{
					if (_FSIsLinkFile(inPath, strlen(inPath)))
						type = (resolvedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? fsItemType_Folder : fsItemType_File;
					else
						type = fsItemType_File;
					
					goto setOutType;
				
				}
			
			}
			
			type = _FSParentExists(inPath) ? fsItemType_NonExistant : fsItemType_NonExistantParent;
		}
		else if (attrib & FILE_ATTRIBUTE_DIRECTORY)
		{
			type = fsItemType_Folder;
		}
		else	// it's a file
		{
			if (inOptions & fsOption_ResolveAliases)
			{
				if (_FSIsLinkFile(inPath, strlen(inPath)))
				{
					// we have a link
					if (_FSResolveLink(NULL, inPath, outResolvedPath, &resolvedInfo) == 0)
						type = (resolvedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? fsItemType_Folder : fsItemType_File;
					else	// couldn't resolve link
					{
						type = fsItemType_UnattachedAlias;
						*outResolvedPath = 0;
						
						if (inOptions & fsOption_NilIfNonExistant)
							return true;
						else
							Fail(errorType_FileSys, fsError_CannotResolveAlias);
					}
				}
				else
					type = fsItemType_File;
			}
			else
			{
				if (_FSIsLinkFile(inPath, strlen(inPath)))
				{
					// we have a link
					WIN32_FIND_DATA resolvedInfo;
					if (_FSResolveLink(NULL, inPath, outResolvedPath, &resolvedInfo) == 0)
						type = (resolvedInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? fsItemType_FolderAlias : fsItemType_FileAlias;
					else	// couldn't resolve link
						type = fsItemType_UnattachedAlias;
					
					*outResolvedPath = 0;	// because fsOption_ResolveAliases is off
				}
				else
					type = fsItemType_File;
			}
		}
		
setOutType:
		// set outType even if return nil
		if (outType) *outType = type;

		if (type == fsItemType_NonExistantParent)
		{
			if (inOptions & fsOption_NilIfNonExistantParent)
				return true;
			else
				goto nonExistant;
		}
		else if (type == fsItemType_NonExistant)
		{
nonExistant:
			if (inOptions & fsOption_NilIfNonExistant)
				return true;
			else if (inOptions & fsOption_FailIfNonExistant)
			{
				if (inOptions & (fsOption_FailIfNotFile | fsOption_NilIfNotFile))			// if either option is on
					Fail(errorType_FileSys, fsError_NoSuchFile);
				else if (inOptions & (fsOption_FailIfNotFolder | fsOption_NilIfNotFolder))	// if either option is on
					Fail(errorType_FileSys, fsError_NoSuchFolder);
				else
					Fail(errorType_FileSys, fsError_NoSuchItem);
			}
		}
		else
		{
			if (inOptions & fsOption_NilIfExists)
				return true;
			else if (inOptions & fsOption_FailIfExists)
				Fail(errorType_FileSys, fsError_ItemAlreadyExists);

			if (inOptions & fsOption_NilIfNotFile)
			{
				if (type != fsItemType_File)
					return true;
			}
			else if (inOptions & fsOption_FailIfNotFile)
			{
				if (type != fsItemType_File)
					Fail(errorType_FileSys, fsError_FileExpected);
			}
			else if (inOptions & fsOption_NilIfNotFolder)
			{
				if (type != fsItemType_Folder)
					return true;
			}
			else if (inOptions & fsOption_FailIfNotFolder)
			{
				if (type != fsItemType_Folder)
					Fail(errorType_FileSys, fsError_FolderExpected);
			}
		}
	}
	
	return false;
}

// inPath shouldn't have trailing backslash
static Uint32 _FSCountDirItems(const void *inPath, Uint32 inPathSize, const void *inDirName, Uint32 inDirNameSize)
{
	HANDLE hfind = NULL;
	Int8 *findPath = nil;
	Uint32 count = 0;
	Int8 *p;
	WIN32_FIND_DATA findData;
	
	ASSERT(inPath && inPathSize);
	
	try
	{
		findPath = (Int8 *)UMemory::New(inPathSize + inDirNameSize + 6);
		::CopyMemory(findPath, inPath, inPathSize);

		p = findPath + inPathSize;
		if (inDirNameSize == 0)
		{
			*p++ = '\\';
			*p++ = '*';
			*p++ = 0;
		}
		else
		{
			*p++ = '\\';
			::CopyMemory(p, inDirName, inDirNameSize);
			p += inDirNameSize;
			*p++ = '\\';
			*p++ = '*';
			*p++ = 0;
		}
		
		hfind = ::FindFirstFile(findPath, &findData);
		if (hfind == INVALID_HANDLE_VALUE) FailLastWinError();
		
		// ignore "." directory
		if (findData.cFileName[0] == '.' && findData.cFileName[1] == 0)
		{
			if (!::FindNextFile(hfind, &findData)) goto done;
		}
		
		// ignore ".." directory
		if (findData.cFileName[0] == '.' && findData.cFileName[1] == '.' && findData.cFileName[2] == 0)
		{
			if (!::FindNextFile(hfind, &findData)) goto done;
		}
		
		do {
			count++;
		} while (::FindNextFile(hfind, &findData));
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)findPath);
		if (hfind) ::FindClose(hfind);
		throw;
	}
	
done:
	UMemory::Dispose((TPtr)findPath);
	::FindClose(hfind);
	
	return count;
}

// GetLinkInfo() fills the filename and path buffer
// with relevant information
// hWnd         - calling app's window handle.
//
// lpszLinkName - name of the link file passed into the function.
//
// lpszPath     - the buffer that will receive the filepath name.
//
static HRESULT _FSGetLinkInfo(HWND hWnd, LPCTSTR lpszLinkName, LPSTR lpszPath, LPSTR lpszDescription)
{	
   HRESULT hres;
   IShellLink *psl;
   WIN32_FIND_DATA wfd;

// Assume Failure to start with:
   *lpszPath = 0;
   *lpszDescription = 0;

// Call CoCreateInstance to obtain the IShellLink
// Interface pointer. This call fails if
// CoInitialize is not called, so it is assumed that
// CoInitialize has been called.

   hres = ::CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);
   if ( SUCCEEDED( hres ) )
   {
      IPersistFile *ppf;

// The IShellLink Interface supports the IPersistFile
// interface. Get an interface pointer to it.
      hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
      if ( SUCCEEDED( hres ) )
      {
         wchar_t wsz[MAX_PATH];

//Convert the given link name string to wide character string.
         MultiByteToWideChar( CP_ACP, 0,
                  lpszLinkName,
                  -1, wsz, MAX_PATH );
//Load the file.
         hres = ppf->Load(wsz, STGM_READ);
         
         if ( SUCCEEDED( hres ) )
         {
// Resolve the link by calling the Resolve() interface function.
            hres = psl->Resolve(hWnd,  SLR_ANY_MATCH | SLR_NO_UI);
            if ( SUCCEEDED( hres ) )
            {
               hres = psl->GetPath(lpszPath, MAX_PATH, (WIN32_FIND_DATA*)&wfd, SLGP_UNCPRIORITY);
        if(!SUCCEEDED(hres))
      return FALSE;

               hres = psl->GetDescription(lpszDescription, MAX_PATH);

        if(!SUCCEEDED(hres))
      return FALSE;

            }
         }
         ppf->Release();
      }
      psl->Release();
   }
   return hres;
}

/*
The CreateLink function in the following example creates a shortcut. The 
parameters include a pointer to the name of the file to link to, a pointer 
to the name of the shortcut that you are creating, and a pointer to the 
description of the link. The description consists of the string, "Shortcut 
to filename," where filename is the name of the file to link to. 

 CreateLink - uses the shell's IShellLink and IPersistFile interfaces 
   to create and store a shortcut to the specified object. 
 Returns the result of calling the member functions of the interfaces. 
 lpszPathObj - address of a buffer containing the path of the object 
 lpszPathLink - address of a buffer containing the path where the 
   shell link is to be stored 
 lpszDesc - address of a buffer containing the description of the 
   shell link 
*/
static HRESULT _FSCreateLink(LPCSTR lpszPathObj, LPSTR lpszPathLink, LPSTR lpszDesc)
{ 
	HRESULT hres; 
    IShellLink* psl; 
 
    // Get a pointer to the IShellLink interface. 
    hres = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);
    
    if (SUCCEEDED(hres)) { 
        IPersistFile* ppf; 
 
        // Set the path to the shortcut target, and add the 
        // description. 
        psl->SetPath(lpszPathObj); 
		if (lpszDesc) psl->SetDescription(lpszDesc); 
 
       // Query IShellLink for the IPersistFile interface for saving the 
       // shortcut in persistent storage. 
		hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf); 
 
		if (SUCCEEDED(hres))
		{ 
            wchar_t wsz[MAX_PATH]; 
 
            // Ensure that the string is ANSI. 
            MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, 
                wsz, MAX_PATH); 
 
            // Save the link by calling IPersistFile::Save. 
            hres = ppf->Save(wsz, TRUE); 
            ppf->Release(); 
		} 
        psl->Release(); 
    } 
    return hres; 
}

/*
The application-defined _FSResolveLink function in the following example resolves a shortcut.
Its parameters include a window handle, a pointer to the path of the shortcut, and the
address of a buffer that receives the new path to the object. The window handle identifies 
the parent window for any message boxes that the shell may need to display. For example, 
the shell can display a message box if the link is on unshared media, if network problems 
occur, if the user needs to insert a floppy disk, and so on.
lpszLinkFile can equal lpszPath

TG - doesn't handle shortcuts to shortcuts, but if it did, we could get into an endless loop...
*/
/*static*/ HRESULT _FSResolveLink(HWND hwnd, LPCSTR lpszLinkFile, LPSTR lpszPath, WIN32_FIND_DATA *outWFD, int levels)
{
	if(levels == 0)
		return S_FALSE;
		
	HRESULT hres;
	IShellLink* psl;
	char szGotPath[MAX_PATH];
	//char szDescription[MAX_PATH];
	WIN32_FIND_DATA wfd;
	
	// get a pointer to the IShellLink interface
	hres = ::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl); 
    
	if (SUCCEEDED(hres))
	{
		IPersistFile* ppf; 

		// get a pointer to the IPersistFile interface. 
		hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf); 

		if (SUCCEEDED(hres))
		{
			wchar_t wsz[MAX_PATH]; 

			// ensure that the string is Unicode
			MultiByteToWideChar(CP_ACP, 0, lpszLinkFile, -1, wsz, MAX_PATH); 

            // load the shortcut
			hres = ppf->Load(wsz, STGM_READ); 
			if (SUCCEEDED(hres))
			{
                // resolve the link
                hres = psl->Resolve(hwnd, SLR_ANY_MATCH | (_FSAliasUI ? 0 : SLR_NO_UI));
                
				if (SUCCEEDED(hres))
				{
					// get the path to the link target
					hres = psl->GetPath(szGotPath, MAX_PATH, outWFD ? outWFD : &wfd, SLGP_UNCPRIORITY); 

					if (SUCCEEDED(hres))
 					{
						// get the description of the target
						//hres = psl->GetDescription(szDescription, MAX_PATH); 
						//if (SUCCEEDED(hres)) 
							if (lpszPath) lstrcpy(lpszPath, szGotPath);
					}
                }
			}
			
			// release the pointer to the IPersistFile interface
			ppf->Release(); 
		}
		
		// release the pointer to the IShellLink interface
		psl->Release(); 
	}
	
	if (!SUCCEEDED(hres) && lpszPath)
		*lpszPath = 0;
	else
	{
		// check if this was a .lnk to a .lnk
		Uint32 s = strlen(lpszPath);
		if(s > 4 && lpszPath[s-3] == '.' &&
			(lpszPath[s-2] == 'l' || lpszPath[s-2] == 'L') &&
			(lpszPath[s-1] == 'n' || lpszPath[s-1] == 'N') &&
			(lpszPath[s] == 'k' || lpszPath[s] == 'K'))
			hres = _FSResolveLink(hwnd, lpszPath, lpszPath, outWFD, levels);	
	}
    return hres; 
}


// same as above but for a path
static HRESULT _FSResolveLinksInPath(HWND hwnd, LPCSTR lpszInPath, LPSTR lpszOutPath, WIN32_FIND_DATA *outWFD)
{

	Uint32 tokOff = 0;
	Uint32 tokSize = 0;
	Uint32 dataSize = lstrlen(lpszInPath);
	
	char lnkPath[MAX_PATH];
	lnkPath[0] = 0;
	
	Uint32 s = 0;
	
	while(UMemory::Token(lpszInPath, dataSize, "\\", tokOff, tokSize))
	{
		s += UMemory::Copy(lnkPath + s, lpszInPath + tokOff, tokSize);
		
		lnkPath[s] = 0;
		
		if (::GetFileAttributes(lnkPath) == 0xFFFFFFFF)	// file does not exist
		{
			if(s > 4 && lnkPath[s-3] == '.' &&
				(lnkPath[s-2] == 'l' || lnkPath[s-2] == 'L') &&
				(lnkPath[s-1] == 'n' || lnkPath[s-1] == 'N') &&
				(lnkPath[s] == 'k' || lnkPath[s] == 'K'))
				return STG_E_FILENOTFOUND;	// it's already an .lnk and does not exist
			
			lnkPath[s++] = '.';
			lnkPath[s++] = 'l';
			lnkPath[s++] = 'n';
			lnkPath[s++] = 'k';
			lnkPath[s] = 0;
			

			
			if (::GetFileAttributes(lnkPath) == 0xFFFFFFFF)	// file still does not exist
				return STG_E_FILENOTFOUND;
			else
			{
				//lookup this .lnk - if we have more tokens left, don't bother copying outWFD
				HRESULT res = _FSResolveLink(hwnd, lnkPath, lnkPath, (dataSize > tokOff + tokSize) ? nil : outWFD);
				if (res != S_OK)
				{
					return res;
				}
			}				
		}
		
		s = strlen(lnkPath);
		
		if(dataSize > tokOff + tokSize)
			lnkPath[s++] = lpszInPath[tokOff + tokSize++];	// add the last slash
		else
			break;
	}

	if (lpszOutPath) lstrcpy(lpszOutPath, lnkPath);
	
	return S_OK;
}

Uint32 _GetMaxChildPathSize(TFSRefObj* inRef)
{
	THdl hList = nil;
	Uint8 psName[256];
	Uint32 nMaxPathSize = 0;
	Uint32 nOffset = 0, nTypeCode;
	
	try
	{
		hList = inRef->GetListing(kDontResolveAliases);

		while (UFileSys::GetListNext(hList, nOffset, psName, &nTypeCode))
		{
			Uint32 nPathSize = 1 + psName[0];	// 1 is for the path delimiter (\);
			if (nTypeCode == TB((Uint32)'fldr'))
			{
				TFSRefObj* pRef = UFileSys::New(inRef, nil, psName);
				scopekill(TFSRefObj, pRef);

				nPathSize += _GetMaxChildPathSize(pRef);
			}
				
			if (nPathSize > nMaxPathSize)
				nMaxPathSize = nPathSize;
		}		
	}
	catch(...)
	{
		// don't throw
	}
	
	UMemory::Dispose(hList);
	return nMaxPathSize;
}

TFSRefObj* _FSNewRefFromWinPath(const void *inPath, Uint32 inPathSize)
{
	SFileSysRef *ref;
	Int8 *path;
	
	path = (Int8 *)UMemory::New(inPathSize+1);	// +1 is for the null
	try
	{
		::CopyMemory(path, inPath, inPathSize);
		path[inPathSize] = 0;					// null terminate
		
		ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
	
		ref->path = path;
		ref->pathSize = inPathSize;
	}
	catch(...)
	{
		UMemory::Dispose((TPtr)path);
		throw;
	}
	
	return (TFSRefObj*)ref;
}

TFSRefObj* _PathToRef(const void *inPath, Uint32 inPathSize)
{
	if (!inPath || !inPathSize)
		return nil;
	
	SFileSysRef *ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
	ref->path = (Int8 *)UMemory::New(inPathSize + 1);
	
	UMemory::Copy(ref->path, inPath, inPathSize);
	*(ref->path + inPathSize) = 0;
	ref->pathSize = inPathSize;
	
	return (TFSRefObj*)ref;
}

TFSRefObj* _FSGetModuleRef()
{
	Int8 path[4096];
	Uint16 pathSize = ::GetModuleFileName(NULL, path, sizeof(path));
	
	SFileSysRef *ref = (SFileSysRef *)UMemory::NewClear(sizeof(SFileSysRef));
	ref->path = (Int8 *)UMemory::New(path, pathSize + 1);
	ref->pathSize = pathSize;
	
	return (TFSRefObj*)ref;
}

Uint32 _GetWinPath(TFSRefObj* inRef, void *outAppPath, Uint32 inMaxPathSize)
{
	Require(inRef && outAppPath);

	return UMemory::Copy(outAppPath, ((SFileSysRef *)inRef)->path, ((SFileSysRef *)inRef)->pathSize > inMaxPathSize ? inMaxPathSize : ((SFileSysRef *)inRef)->pathSize);
}

void _GetQuickTimeFSSpec(TFSRefObj* inRef, FSSpec& outSpec)
{
	Require(inRef);
	
	::NativePathNameToFSSpec(((SFileSysRef *)inRef)->path, &outSpec, 0);
}

#endif /* WIN32 */
