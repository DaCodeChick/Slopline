/*
 * TrackerServ.cp
 * (c)1999 Hotline Communications
 */

#include "TrackerServ.h"

const Uint8 kTrackerServVers[] = "\pv1.3";

#if USES_FILE_EXTENSIONS
	const Uint8 *kRezDataFileName = 		"\phltracker.dat";
	const Uint8 *kMyErrorLogFileName = 		"\pError Log.txt";
#else
	const Uint8 *kRezDataFileName = 		"\pHotlineTrack Data";
	const Uint8 *kMyErrorLogFileName = 		"\pError Log";
#endif


/* ————————————————————————————————————————————————————————————————————————— */

CMyApplication *gApp;

void main()
{
	UOperatingSystem::Init();
	
	try
	{
		UTransport::Init();
		
		if (!UTransport::HasTCP())
			Fail(errorType_Transport, transError_NeedTCP);
		
#if !MACINTOSH
		URez::AddProgramFileToSearchChain(kRezDataFileName);
#endif

		gApp = new CMyApplication;
		gApp->StartUp();
		gApp->Run();
		delete gApp;
	}
	catch(SError& err)
	{
		UError::Alert(err);
	}
}


/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -

CMyTracker::CMyTracker(TFSRefObj* inTrackerFolder)
{
	mLogWin = nil;
	mStatsWin = nil;
	mServersWin = nil;
	mTrackerFolder = inTrackerFolder;
	
	mOpts = mTrackerFolder ? 0 : myOpt_ActiveTracker | myOpt_LogTrackerStart | myOpt_LogTrackerStop;
	mIPAddress.un_IP.stDW_IP.nDW_IP = 0;
	mRegTpt = mListenTpt = mListenHttpTpt = nil;
	
	mServerList = nil;
	mServerCount = 0;

	mMaxServersPerIP = 0;
	mListingCounter = mUserCount = 0;

	Uint16 winVis = 0;
	SRect winRect[3];
	winRect[0].SetEmpty();
	winRect[1].SetEmpty();
	winRect[2].SetEmpty();
	
	if (!ReadPrefs(winRect, &winVis) && mTrackerFolder)
		return;

	// make logs window
	mLogWin = new CMyTextWin;
	if (winRect[0].IsEmpty())
		mLogWin->SetAutoBounds(windowPos_TopLeft, windowPosOn_WinScreen);
	else
	{
		mLogWin->SetBounds(winRect[0]);
		mLogWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
	}
	if (winVis & myWinVis_Log) mLogWin->Show();

	// make stats window
	mStatsWin = new CMyStatsWin;
	if (winRect[1].IsEmpty())
		mStatsWin->SetAutoBounds(windowPos_TopLeft, windowPosOn_WinScreen);
	else
	{
		mStatsWin->SetLocation(winRect[1].TopLeft());
		mStatsWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
	}	
	if (winVis & myWinVis_Stats) mStatsWin->Show();

	// make servers window
	mServersWin = new CMyTextWin;
	if (winRect[2].IsEmpty())
		mServersWin->SetAutoBounds(windowPos_TopLeft, windowPosOn_WinScreen);
	else
	{
		mServersWin->SetBounds(winRect[2]);
		mServersWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
	}
	if (winVis & myWinVis_Servers) mServersWin->Show();

	UpdateTitles();
	LogText("\pHotline Tracker", kTrackerServVers);
	
	if (IsActive())
		StartTracker();	
}

CMyTracker::~CMyTracker()
{
	WritePrefs();
	
	ClearClientList();
	UTransport::Dispose(mRegTpt);
	UTransport::Dispose(mListenTpt);
	UTransport::Dispose(mListenHttpTpt);

	ClearServerList();
	ClearServerPerIPList();

	ClearLoginList();
	ClearPermBanList();
	
	delete mLogWin;
	delete mStatsWin;
	delete mServersWin;
}

void CMyTracker::StartTracker()
{
	StopTracker(false);
	
	SInternetAddress addr = { kInternetAddressType, 0, 0, 0, 0, 0 };

	try
	{
		// make register transport
		addr.port = 5499;
		mRegTpt = UTransport::New(transport_UDPIP);
		mRegTpt->SetIPAddress(mIPAddress);
		mRegTpt->Listen(&addr, sizeof(addr));
		mRegTpt->SetMessageHandler(MessageHandler, this);

		// make client listener
		addr.port = 5498;
		mListenTpt = UTransport::New(transport_TCPIP);
		mListenTpt->SetIPAddress(mIPAddress);
		mListenTpt->Listen(&addr, sizeof(addr));
		mListenTpt->SetMessageHandler(MessageHandler, this);

		addr.port = 5497;
		mListenHttpTpt = UTransport::New(transport_TCPIP, false);
		mListenHttpTpt->SetIPAddress(mIPAddress);
		mListenHttpTpt->Listen(&addr, sizeof(addr));
		mListenHttpTpt->SetMessageHandler(MessageHandler, this);
	} 
	catch(...)
	{
		UpdateTitles();
		UpdateStatsWin();

		if (mIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			Uint8 bufMsg[256];
			bufMsg[0] = UText::Format(bufMsg + 1, sizeof(bufMsg) - 1, "Cannot bind to the address %d.%d.%d.%d because it is incorrect or it is already in use.", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
			DisplayMessage("\pTracker error", bufMsg, icon_Stop, 1);
		}
		
		return;
	}
	
	mServerList = UMemory::NewHandle(1024);
	ReadLoginList();
	ReadPermBanList();

	mIPAddress = mRegTpt->GetIPAddress();
	UpdateTitles();
	UpdateStatsWin();
	
	mOpts |= myOpt_ActiveTracker;
	
	if (mOpts & myOpt_LogTrackerStart)
		LogStartStop(true);
}

void CMyTracker::StopTracker(bool inLogStop)
{
	mOpts &= ~myOpt_ActiveTracker;

	if (mRegTpt)
	{
		UTransport::Dispose(mRegTpt);
		mRegTpt = nil;
	}
		
	if (mListenTpt)
	{
		UTransport::Dispose(mListenTpt);
		mListenTpt = nil;
	}

	if (mListenHttpTpt)
	{
		UTransport::Dispose(mListenHttpTpt);
		mListenHttpTpt = nil;
	}
		
	mUserCount = mListingCounter = 0;

	ClearServerList();
	ClearServerPerIPList();
	
	ClearLoginList();
	ClearPermBanList();

	UpdateStatsWin();
	
	if ((mOpts & myOpt_LogTrackerStop) && inLogStop)
		LogStartStop(false);
}

void CMyTracker::DeleteTracker()
{
	if (mTrackerFolder)
		mTrackerFolder->DeleteFolderContents();
}

void CMyTracker::MessageHandler(void *inContext, void *inObject, Uint32 inMsg, const void *inData, Uint32 inDataSize)
{
	#pragma unused(inData, inDataSize)
	
	switch (inMsg)
	{
		case msg_DataArrived:
		case msg_DataTimeOut:
		case msg_ReadyToSend:
		case msg_ConnectionClosed:
		case msg_ConnectionRequest:
		case msg_ConnectionEstablished:
		case msg_ConnectionRefused:
			CMyTracker *pTracker = (CMyTracker *)inContext;
		
			if ((TTransport)inObject == pTracker->mRegTpt)
				pTracker->ProcessRegister();
			else
				pTracker->ProcessClients();
	}
}

bool CMyTracker::IsActive()
{
	if (mOpts & myOpt_ActiveTracker)
		return true;
		
	return false;
}

SIPAddress CMyTracker::GetIPAddress()
{
	return mIPAddress;	
}

Uint32 CMyTracker::GetServerCount()
{
	return mServerCount;
}

Uint32 CMyTracker::GetUserCount()
{
	return mUserCount;
}

Uint32 CMyTracker::GetListingCounter()
{
	return mListingCounter;
}

bool CMyTracker::EditTracker(bool inNewTracker)
{
	CPtrList<SMyLoginInfo> stLoginList;
	ReadLoginInfo(&stLoginList);

	CPtrList<SMyPermBanInfo> stPermBanList;
	ReadPermBanInfo(&stPermBanList);
	
	CMyOptionsWin *win = new CMyOptionsWin();
	win->SetInfo(mIPAddress, mMaxServersPerIP, mOpts, &stLoginList, &stPermBanList);
	
	// clear login list
	Uint32 i = 0;
	SMyLoginInfo *pLoginInfo;
	while (stLoginList.GetNext(pLoginInfo, i))
		UMemory::Dispose((TPtr)pLoginInfo);

	stLoginList.Clear();

	// clear ban list
	i = 0;
	SMyPermBanInfo *pPermBanInfo;
	while (stPermBanList.GetNext(pPermBanInfo, i))
		UMemory::Dispose((TPtr)pPermBanInfo);

	stPermBanList.Clear();
	
	// show window
	win->Show();
	
	// go into modal loop to process modal options window
	Uint32 id;
	for(;;)
	{
		win->ProcessModal();
		id = win->GetHitID();
		
		if (id == 1)	// save
		{
			SIPAddress stIPAddress = win->GetIPAddress();
			if (!stIPAddress.un_IP.stDW_IP.nDW_IP)
			{
				DisplayMessage("\pTracker error", "\pInvalid IP address.", icon_Stop, 1);
				continue;
			}

			if (inNewTracker && gApp->IsInTrackerList(stIPAddress))
			{
				DisplayMessage("\pTracker error", "\pThis IP address already exists in tracker list.", icon_Stop, 1);
				continue;
			}
			
			Uint32 nOpts = mOpts;
			Uint32 nMaxServersPerIP = mMaxServersPerIP;
			
			win->GetInfo(nil, &mMaxServersPerIP, &mOpts, &stLoginList, &stPermBanList);
			delete win;
			
			if (nOpts & myOpt_ActiveTracker)
				mOpts |= myOpt_ActiveTracker;
						
			bool bStartTracker = false;
			if (stIPAddress.un_IP.stDW_IP.nDW_IP != mIPAddress.un_IP.stDW_IP.nDW_IP)
			{
				bStartTracker = true;
				mIPAddress.un_IP.stDW_IP.nDW_IP = stIPAddress.un_IP.stDW_IP.nDW_IP;
			}
			
			WritePrefs();
			WriteLoginInfo(&stLoginList);
			WritePermBanInfo(&stPermBanList);
			
			if (bStartTracker)
				StartTracker();
			
			if (nMaxServersPerIP != mMaxServersPerIP)
				BuildServerPerIPList();
			
			return true;
		}
		else if (id == 2)	// cancel
		{
			delete win;
			return false;
		}
		else if(id == 3)
			win->AddLogin();
		else if(id ==4)
			win->DeleteLogin();
		else if(id == 5)
			win->AddPermBan();
		else if(id ==6)
			win->DeletePermBan();
	}
}

void CMyTracker::ShowLogWin()
{
	if (mLogWin->IsVisible())
		mLogWin->BringToFront();
	else
		mLogWin->Show();

#if WIN32
	if (UWindow::IsCollapsed(*mLogWin))
 		UWindow::Expand(*mLogWin);
#endif
		
	mLogWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
}

void CMyTracker::ShowStatsWin()
{
	if (mStatsWin->IsVisible())
		mStatsWin->BringToFront();
	else
		mStatsWin->Show();

#if WIN32
	if (UWindow::IsCollapsed(*mStatsWin))
 		UWindow::Expand(*mStatsWin);
#endif
		
	mStatsWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
}

void CMyTracker::ShowServersWin()
{
	UpdateServersWin();

	if (mServersWin->IsVisible())
		mServersWin->BringToFront();
	else
		mServersWin->Show();

#if WIN32
	if (UWindow::IsCollapsed(*mServersWin))
 		UWindow::Expand(*mServersWin);
#endif
		
	mServersWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
}

void CMyTracker::BringToFrontWin()
{
	if (mLogWin->IsVisible())
	{
		mLogWin->BringToFront();

	#if WIN32
		if (UWindow::IsCollapsed(*mLogWin))
 			UWindow::Expand(*mLogWin);
	#endif
	}

	if (mStatsWin->IsVisible())
	{
		mStatsWin->BringToFront();

	#if WIN32
		if (UWindow::IsCollapsed(*mStatsWin))
 			UWindow::Expand(*mStatsWin);
	#endif
	}

	if (mServersWin->IsVisible())
	{
		mServersWin->BringToFront();

	#if WIN32
		if (UWindow::IsCollapsed(*mServersWin))
 			UWindow::Expand(*mServersWin);
	#endif
	}
}

void CMyTracker::RemoveOldServers()
{
	if (!IsActive() || !mServerList || !mServerCount)
		return;
	
	Uint32 curTime;
	SMyServerInfo **pp, **spp, **epp, *info;
	//Uint8 buf[8192];
	//Uint8 *p, *ep;
	//Uint32 count, s;

	//p = buf;
	//ep = buf + sizeof(buf);
	//count = 0;
	
	//*((Uint16 *)p)++ = 2;	// add type
	//*((Uint32 *)p)++ = 0;	// space for size and count

	curTime = UDateTime::GetSeconds();
	
	spp = (SMyServerInfo **)UMemory::Lock(mServerList);
	epp = spp - 1;
	pp = epp + mServerCount;

	while (pp != epp)
	{
		info = *pp;
		
		if (curTime - info->timeStamp > 1860)	// 31 minutes
		{
			//if not enough space in buf, stop (we'll get the rest in the next sweep)
			//if (info->data[0] + 7 > ep - p)
			//	break;
			
			//count++;
			//*((Uint32 *)p)++ = info->address;
			//*((Uint16 *)p)++ = info->port;
			//p += UMemory::Copy(p, info->data, info->data[0]+1);
			
			mUserCount -= info->userCount;
			RemoveServerPerIP(info->address);
			
			UMemory::Dispose((TPtr)info);
			UMemory::Copy(pp, pp+1, BPTR(spp + mServerCount) - BPTR(pp+1));
			mServerCount--;
		}
		
		pp--;
	}
	
	UMemory::Unlock(mServerList);
	
	//*(Uint16 *)(buf+2) = p - buf - 4;	// size of following data
	//*(Uint16 *)(buf+4) = count;		// number of servers
	//s = p - buf;
	
	UpdateStatsWin();
}

bool CMyTracker::RemoveServer(Uint32 inIndex)
{
	if (!mServerCount || inIndex > mServerCount - 1) 
		return false;
		
	SMyServerInfo **spp = (SMyServerInfo **)UMemory::Lock(mServerList);
	SMyServerInfo **pp = spp + inIndex;
	SMyServerInfo *info = *pp;
			
	mUserCount -= info->userCount;
	RemoveServerPerIP(info->address);
			
	UMemory::Dispose((TPtr)info);
	UMemory::Copy(pp, pp + 1, BPTR(spp + mServerCount) - BPTR(pp + 1));
	mServerCount--;
	
	UMemory::Unlock(mServerList);
		
	UpdateStatsWin();
	return true;
}

void CMyTracker::ClearServerList()
{
	if (!mServerList)
	{
		mServerCount = 0;
		return;
	}

	SMyServerInfo **pp = (SMyServerInfo **)UMemory::Lock(mServerList);
	Uint32 nCount = mServerCount;

	while (nCount--)
	{
		UMemory::Dispose((TPtr)(*pp));
		pp++;
	}
		
	UMemory::Unlock(mServerList);
	UMemory::Dispose(mServerList);

	mServerList = nil;
	mServerCount = 0;
}

void CMyTracker::ClearClientList()
{
	Uint32 i = 0;
	SMyClient *pClient;
	
	while (mClientList.GetNext(pClient, i))
	{
		delete pClient->tpt;
		UMemory::Dispose((TPtr)pClient);
	}
	
	mClientList.Clear();
}

void CMyTracker::ResetTrackerServ()
{		
	// wipe client list
	ClearClientList();
	
	UpdateStatsWin();
	LogReset();
}

void CMyTracker::ProcessRegister()
{
	if (!IsActive())
		return;

	Uint8 buf[1024];
	Uint8 addr[128];
	Uint32 addrSize, dataSize;
	Uint16 type;
	Uint8 *p, *ep;
	
	for(;;)
	{
		addrSize = sizeof(addr);
		dataSize = sizeof(buf);

		if (!mRegTpt->ReceiveUnit(addr, addrSize, buf, dataSize))
			break;
		
		if (dataSize >= sizeof(Uint16))
		{
			type = FB( *(Uint16 *)buf );
			p = buf;
			ep = p + dataSize;
			
			// validate and dispatch message
			if (type == 1)		// add
			{
				// Uint16 type; Uint16 port; Uint16 userCount; Uint16 flags; Uint32 passID; pstring name; pstring desc; pstring password;
				// flags = 1 means don't show in list (name only)
				
				if (dataSize > 14)
				{
					CUnflatten unflat(buf, dataSize);
					
					unflat.SkipWord();
					Uint16 port = unflat.ReadWord();
					Uint16 userCount = unflat.ReadWord();
					Uint16 flags = unflat.ReadWord();
					Uint32 passID = unflat.ReadLong();
					Uint8 *name = unflat.ReadPString();
					Uint8 *desc = unflat.ReadPString();
					Uint8 *password = unflat.ReadPString();
					
					((SInternetAddress *)addr)->port = port;
					RegisterServer(*(SInternetAddress *)addr, userCount, flags, passID, name, desc, password);
				}
			}
			else if (type == 2)	// remove
			{
				// not done and not really necessary, so just ignore this message type
			}
		}
	}
}

void CMyTracker::ProcessClients()
{
	if (!IsActive())
		return;

	SMyClient *client;
	TTransport tpt;
	Uint32 i;
	
	// accept connections
	for(;;)
	{
		tpt = nil;
		if (mListenTpt)
			tpt = mListenTpt->Accept();
		
		if (tpt == nil)
		{
			if (mListenHttpTpt)
				tpt = mListenHttpTpt->Accept();
		
			if (tpt == nil) break;
		}
				
		client = nil;
		try
		{
			tpt->SetMessageHandler(MessageHandler, this);
			client = (SMyClient *)UMemory::NewClear(sizeof(SMyClient));
			client->tpt = tpt;
			mClientList.AddItem(client);
		}
		catch(...)
		{
			if (client) delete client;
			delete tpt;
			throw;
		}
					
		// reset error count every time someone connects
		gApp->ResetErrorCount();
	}

	// process each client
	i = 0;
	while (mClientList.GetNext(client, i))
	{
		tpt = client->tpt;
		
		// check if connection has closed
		if (!tpt->IsConnected())
		{
			mClientList.RemoveItem(client);
			delete tpt;
			UMemory::Dispose((TPtr)client);
			i--;
			continue;
		}
		if (tpt->IsDisconnecting()) continue;
		
		// establish transaction protocol if necessary
		if (!client->isEstablished)
		{
			if (tpt->GetReceiveSize() >= 6)
			{
				#pragma options align=packed
				struct {
					Uint32 type;
					Uint16 version;
				} estabData;
				#pragma options align=reset
				
				tpt->Receive(&estabData, sizeof(estabData));
				
				if (estabData.type == TB((Uint32)'HTRK') && estabData.version == TB((Uint16)1))
				{
					tpt->Send(&estabData, sizeof(estabData));
					mListingCounter++;
					SendServerList(tpt);
					UpdateStatsWin();
				}
				else
				{
					tpt->Disconnect();
				}
			}
		}
	}
}

void CMyTracker::RegisterServer(const SInternetAddress& inAddr, Uint16 inUserCount, Uint16 inFlags, Uint32 inPassID, const Uint8 *inName, const Uint8 *inDesc, const Uint8 *inPass)
{
	if (!IsActive() || !inName || !inName[0] || !inDesc || !inPass)
		return;

	Uint32 i, n, crc;
	SMyServerInfo *ip, *newInfo;
	bool updateExisting = false;
	
	// ignore servers with incorrect passwords or banned addresses
	if (IsInvalidPassword(inPass) || IsBannedAddress(inAddr)) 
		return;
	
	if (mOpts & myOpt_LogServerConnect)
		LogServerConnect(inName, inAddr, inPass);
	
	// expand server list handle if necessary
	n = UMemory::GetSize(mServerList);
	if (((mServerCount+2) * sizeof(void*)) > n)
		UMemory::SetSize(mServerList, n + 1024);
	
	// calculate CRC of name
	crc = UMemory::CRC(inName+1, inName[0]);

#if INTERNAL_SERVER_PRIORITY
	// give priority to our internal servers!
	const Uint8 matchHost[3] = {	207, 245, 14	};
	
	if (crc < 10)
		crc = 10;
	
	if (UMemory::Equal(inAddr.host, matchHost, 3))
	{
		if (inAddr.host[3] == 170)
		{
			switch (inAddr.port)
			{
				case 5500:
					crc = 0;
					break;
				
				default:
					crc = 1;
					break;
			}
		}
		else if (inAddr.host[3] >= 162 && inAddr.host[3] <= 200)
			crc = 1;
	}
#endif

	// get ptr to list of servers
	void *hdata;
	StHandleLocker locker(mServerList, hdata);
	SMyServerInfo **serverList = (SMyServerInfo **)hdata;

	// decide whether to update existing server info or add new
	if (FindServerByName(crc, i))		// if found
	{
		// move i to first item with same CRC
		if (i)							// to stop i from going negative
		{
			while (i--)
			{
				if (serverList[i]->nameCRC != crc)
				{
					i++;
					break;
				}
			}
		}
		
		for (n=i; n<mServerCount; n++)
		{
			ip = serverList[n];
			if (ip->nameCRC != crc) break;
			
			if (ip->data[0] == inName[0] && UMemory::Equal(ip->data, inName, inName[0]))	// if same name
			{
				if ((ip->address == *(Uint32 *)inAddr.host && ip->port == inAddr.port) || (ip->passID == inPassID))	// if same address or same password
				{
					updateExisting = true;
					break;
				}
				else
				{
					// ***** should auto choose a diff name, eg append number
					return;
				}
			}
		}
	}
	else if (FindServerByAddress(inAddr, n))
	{
		ip = serverList[n];
		updateExisting = true;
	}
	
	// update or add the server registration
	if (updateExisting)								// if updating existing server info
	{
		Uint32 oldUserCount = ip->userCount;
		Uint32 oldNameCRC = ip->nameCRC;
		
		if (ip->address != *(Uint32 *)inAddr.host)
		{
			if (AddServerPerIP(*(Uint32 *)inAddr.host))
				RemoveServerPerIP(ip->address);
			else	
			{
				// delete servers which are exceeding the mMaxServersPerIP limit
				RemoveServer(n);
				LogRejectServerConnect(inAddr);
				return;
			}
		}

		// build new server-info, and use the existing memory if the size is the same
		if (GetServerInfoSize(ip) == (sizeof(SMyServerInfo) + inName[0] + inDesc[0] + inPass[0] + 3))	// if new and old are same size
			newInfo = NewServerInfo(ip, inAddr, inUserCount, inFlags, inPassID, crc, inName, inDesc, inPass);
		else
		{
			newInfo = NewServerInfo(nil, inAddr, inUserCount, inFlags, inPassID, crc, inName, inDesc, inPass);
			UMemory::Dispose((TPtr)ip);
			serverList[n] = newInfo;
		}
		
		// adjust total user count
		if (inUserCount > oldUserCount)
			mUserCount += inUserCount - oldUserCount;
		else
			mUserCount -= oldUserCount - inUserCount;
		
		// if CRC changed, need to reinsert item to maintain sorting (list is sorted by CRC)
		if (oldNameCRC != crc)
		{
			// n is the current index, i is the new index
			UMemory::Copy(serverList + n, serverList + n + 1, (mServerCount - (n + 1)) * sizeof(void*));
			if (i > n) i--;
			UMemory::Copy(serverList + i + 1, serverList + i, ((mServerCount-1) - i) * sizeof(void *));
			serverList[i] = newInfo;
		}
	}
	else	// adding new server
	{
		// ignore servers which are exceeding the mMaxServersPerIP limit
		if (!AddServerPerIP(*(Uint32 *)inAddr.host))
		{
			LogRejectServerConnect(inAddr);
			return;
		}

		ip = NewServerInfo(nil, inAddr, inUserCount, inFlags, inPassID, crc, inName, inDesc, inPass);
		
		UMemory::Copy(serverList+i+1, serverList+i, (mServerCount - i) * 4);
		serverList[i] = ip;
		
		mServerCount++;
		mUserCount += inUserCount;
	}

	UpdateStatsWin();
}

#if 0
void CMyTracker::UnregisterServer(const SInternetAddress& inAddr, const Uint8 inName[])
{
	// can implement this later.  RemoveOldServers() will do for starters
	
	search for crc
	go up if same crc
	while crc is same
		if same crc and same addr and same port, remove it
}
#endif

// outIndex is 0-based
bool CMyTracker::FindServerByName(Uint32 inCRC, Uint32& outIndex)
{
	if (!IsActive() || mServerCount == 0)
	{
		outIndex = 0;
		return false;
	}

	void *hdata;
	StHandleLocker lock(mServerList, hdata);
	SMyServerInfo* *lookupTab = (SMyServerInfo* *)hdata;
	Uint32 crc;
	Uint32 l, r, i;
	
	crc = lookupTab[0]->nameCRC;
	
	if (inCRC == crc)
	{
		outIndex = 0;
		goto found;
	}
	else if (inCRC < crc)
	{
		outIndex = 0;
		return false;
	}
	
	l = 1;
	r = mServerCount - 1;
	
	if (l > r)
	{
		outIndex = 1;
		return false;
	}
	
	while (l <= r)
	{
		i = (l + r) >> 1;
		
		crc = lookupTab[i]->nameCRC;

		if (inCRC == crc)
		{
			outIndex = i;
			goto found;
		}
		else if (inCRC < crc)
			r = i - 1;
		else
			l = i + 1;
	}
	
	if (inCRC > crc)
		i++;
	
	outIndex = i;
	return false;
	
found:
	return true;
}

// outIndex is 0-based
bool CMyTracker::FindServerByAddress(const SInternetAddress& inAddr, Uint32& outIndex)
{
	if (!IsActive())
	{
		outIndex = 0;
		return false;
	}

	void *hdata;
	StHandleLocker lock(mServerList, hdata);
	SMyServerInfo **pp = (SMyServerInfo **)hdata;
	SMyServerInfo **sp = pp;
	
	Uint32 addr = *(Uint32 *)inAddr.host;
	Uint16 port = inAddr.port;
	
	Uint32 n = mServerCount;
	
	while (n--)
	{
		if ((**pp).address == addr && (**pp).port == port)
		{
			outIndex = pp - sp;
			return true;
		}
		
		pp++;
	}
	
	outIndex = 0;
	return false;
}

// returns true if found
SMyServerInfo *CMyTracker::NameToInfo(const Uint8 *inName)
{
	if (!IsActive())
		return nil;

	Uint32 crc, i;
	SMyServerInfo **serverList;
	SMyServerInfo *ip;
	
	crc = UMemory::CRC(inName+1, inName[0]);

	serverList = (SMyServerInfo **)UMemory::Lock(mServerList);

	if (FindServerByName(crc, i))
	{
		// move i to first item with same CRC
		while (i--)
		{
			if (serverList[i]->nameCRC != crc)
			{
				i++;
				break;
			}
		}
		
		for (; i<mServerCount; i++)
		{
			ip = serverList[i];
			if (ip->nameCRC != crc) break;
			
			if (ip->data[0] == inName[0] && UMemory::Equal(ip->data, inName, inName[0]))	// if same name
			{
				UMemory::Unlock(mServerList);
				return ip;
			}
		}
	}
	
	UMemory::Unlock(mServerList);
	return nil;
}

void CMyTracker::SendServerList(TTransport inTpt)
{
	if (!IsActive())
		return;

	if (mOpts & myOpt_LogClientConnect)
	{
		Uint8 psIPAddress[256];
		psIPAddress[0] = inTpt->GetRemoteAddressText(psIPAddress + 1, sizeof(psIPAddress) - 1);
		LogText(psIPAddress, "\pClient Connection");
	}

	Uint8 buf[8192];
	Uint8 *p, *ep;
	SMyServerInfo **pp;
	SMyServerInfo *info;
	Uint32 n, count;
	Uint8 *name, *desc;

	p = buf;
	ep = buf + sizeof(buf);
	n = mServerCount;
	count = 0;
	
	*((Uint16 *)p)++ = TB((Uint16)1);	// add type
	*((Uint16 *)p)++ = 0;				// space for size
	*((Uint32 *)p)++ = 0;				// space for counts

	pp = (SMyServerInfo **)UMemory::Lock(mServerList);
	
	try
	{
		while (n--)
		{
			info = *pp;
						
			name = info->data;
			desc = name + name[0] + 1;
			
			if ((name[0] + desc[0] + 12) > (ep - p))	// if not enough space
			{
				// send what's in the buf
				*(Uint16 *)(buf+2) = TB((Uint16)(p - buf - 4));	// size of following data
				*(Uint16 *)(buf+4) = TB((Uint16)mServerCount);	// number of servers in total
				*(Uint16 *)(buf+6) = TB((Uint16)count);			// number of servers in this message
				inTpt->Send(buf, p - buf);
				
				// reset buf
				count = 0;
				p = buf;
				*((Uint16 *)p)++ = TB((Uint16)1);	// add type
				*((Uint16 *)p)++ = 0;				// space for size
				*((Uint32 *)p)++ = 0;				// space for counts
			}
			
			// add server to buf
			count++;
			*((Uint32 *)p)++ = info->address;
			*((Uint16 *)p)++ = TB((Uint16)info->port);
			*((Uint16 *)p)++ = TB((Uint16)info->userCount);
			*((Uint16 *)p)++ = TB((Uint16)info->flags);
			p += UMemory::Copy(p, name, name[0]+1);
			p += UMemory::Copy(p, desc, desc[0]+1);
			
			pp++;
		}
		
		// send what's in the buf
		*(Uint16 *)(buf+2) = TB((Uint16)(p - buf - 4));	// size of following data
		*(Uint16 *)(buf+4) = TB((Uint16)mServerCount);	// number of servers in total
		*(Uint16 *)(buf+6) = TB((Uint16)count);			// number of servers in this message
		inTpt->Send(buf, p - buf);
	}
	catch(...)
	{
		UMemory::Unlock(mServerList);
		throw;
	}
	
	UMemory::Unlock(mServerList);
}

void CMyTracker::SendLookup(TTransport inTpt, const Uint8 *inName)
{
	SMyServerInfo *info;
	Uint8 buf[1024];
	Uint8 *p = buf;
	Uint8 *ep = buf + sizeof(buf);
	Uint8 *name, *desc;

	info = NameToInfo(inName);
	
	if (info)
	{
		name = info->data;
		desc = name + name[0] + 1;

		*((Uint16 *)p)++ = TB((Uint16)4);				// type 4 means we found it
		*((Uint16 *)p)++ = 0;							// space for the size
		
		*((Uint32 *)p)++ = info->address;
		*((Uint16 *)p)++ = TB((Uint16)info->port);
		*((Uint16 *)p)++ = TB((Uint16)info->userCount);
		*((Uint16 *)p)++ = TB((Uint16)info->flags);
		p += UMemory::Copy(p, name, name[0]+1);
		p += UMemory::Copy(p, desc, desc[0]+1);
		
		*(Uint16 *)(buf+2) = TB((Uint16)(p - buf - 4));	// size of following data
	}
	else
	{
		*((Uint16 *)p)++ = TB((Uint16)5);				// type 5 means not found
		*((Uint16 *)p)++ = 0;	// no data
	}
	
	inTpt->Send(buf, p - buf);
}

void CMyTracker::BuildServerPerIPList()
{
	ClearServerPerIPList();
	if (!mMaxServersPerIP || !mServerList || !mServerCount)
		return;
 
	SMyServerInfo **pp, **spp, **epp, *info;
	spp = (SMyServerInfo **)UMemory::Lock(mServerList);
	epp = spp - 1;
	pp = epp + mServerCount;

	while (pp != epp)
	{
		info = *pp;
		
		if (!AddServerPerIP(info->address))
		{			
			mUserCount -= info->userCount;
			
			UMemory::Dispose((TPtr)info);
			UMemory::Copy(pp, pp + 1, BPTR(spp + mServerCount) - BPTR(pp + 1));
			mServerCount--;
		}
		
		pp--;
	}
	
	UMemory::Unlock(mServerList);
		
	UpdateStatsWin();
}

bool CMyTracker::FindServerByAddress(Uint32 inAddr, Uint32& outIndex)
{
	if (!mServerPerIPList.GetItemCount())
	{
		outIndex = 0;
		return false;
	}

	Uint32 nAddr = mServerPerIPList.GetItem(1)->address;
	if (inAddr == nAddr)
	{
		outIndex = 1;
		return true;
	}
	else if (inAddr < nAddr)
	{
		outIndex = 0;
		return false;
	}
	
	Uint32 nFirst = 2;
	Uint32 nLast = mServerPerIPList.GetItemCount();
	if (nFirst > nLast)
	{
		outIndex = 2;
		return false;
	}
	
	Uint32 nIndex;
	while (nFirst <= nLast)
	{
		nIndex = (nFirst + nLast)/2;
		nAddr = mServerPerIPList.GetItem(nIndex)->address;
	
		if (inAddr == nAddr)
		{
			outIndex = nIndex;
			return true;
		}
		else if (inAddr < nAddr)
			nLast = nIndex - 1;
		else
			nFirst = nIndex + 1;
	}
	
	if (inAddr > nAddr)
		nIndex++;
	
	outIndex = nIndex;
	return false;	
}

bool CMyTracker::AddServerPerIP(Uint32 inAddr)
{
	if (!mMaxServersPerIP)	// don't need to add to list
		return true;
	
	Uint32 nIndex;
	if (FindServerByAddress(inAddr, nIndex))
	{
		SMyServerPerIP *pServerPerIP = mServerPerIPList.GetItem(nIndex);
		if (pServerPerIP)
		{
			if (pServerPerIP->count >= mMaxServersPerIP)
				return false;
		
			pServerPerIP->count++;
		}
	}
	else
	{
		SMyServerPerIP *pServerPerIP = (SMyServerPerIP *)UMemory::New(sizeof(SMyServerPerIP));
		
		pServerPerIP->address = inAddr;
		pServerPerIP->count = 1;
		
		mServerPerIPList.InsertItem(nIndex, pServerPerIP);
	}
	
	return true;
}

bool CMyTracker::RemoveServerPerIP(Uint32 inAddr)
{
	Uint32 nIndex;
	if (!FindServerByAddress(inAddr, nIndex))
		return false;

	SMyServerPerIP *pServerPerIP = mServerPerIPList.GetItem(nIndex);
	if (!pServerPerIP)
		return false;
	
	pServerPerIP->count--;
	if (!pServerPerIP->count)
	{
		mServerPerIPList.RemoveItem(nIndex);
		UMemory::Dispose((TPtr)pServerPerIP);
	}
	
	return true;
}

void CMyTracker::ClearServerPerIPList()
{
	Uint32 i = 0;
	SMyServerPerIP *pServerPerIP;
	
	while (mServerPerIPList.GetNext(pServerPerIP, i))
		UMemory::Dispose((TPtr)pServerPerIP);
	
	mServerPerIPList.Clear();
}

void CMyTracker::UpdateTitles()
{
	Uint8 psTitle[32];
	psTitle[0] = UText::Format(psTitle + 1, sizeof(psTitle) - 1, "Log - %d.%d.%d.%d", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
	mLogWin->SetTitle(psTitle);

	psTitle[0] = UText::Format(psTitle + 1, sizeof(psTitle) - 1, "Stats - %d.%d.%d.%d", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
	mStatsWin->SetTitle(psTitle);	

	psTitle[0] = UText::Format(psTitle + 1, sizeof(psTitle) - 1, "Servers - %d.%d.%d.%d", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
	mServersWin->SetTitle(psTitle);	
}

void CMyTracker::UpdateStatsWin()
{
	gApp->UpdateGlobalStats();

	if (mStatsWin)
	{
		mStatsWin->SetServerCount(mServerCount);
		mStatsWin->SetUserCount(mUserCount);
		mStatsWin->SetListingCounter(mListingCounter);
	}
}

void CMyTracker::UpdateServersWin()
{
	CTextView *pTextView = mServersWin->GetTextView();
	
	if (!pTextView->IsEmpty())
		pTextView->DeleteText(0, max_Uint32);
	
	if (!IsActive())
		return;

	void *hdata;
	StHandleLocker lock(mServerList, hdata);
	SMyServerInfo **pp = (SMyServerInfo **)hdata;
	
	Uint8 psAddr[32];
	Uint8 bufData[256];
	Uint32 nCount = mServerCount;
	
	while (nCount--)
	{
		Uint8 *pAddr = (Uint8 *)(&(**pp).address);
		psAddr[0] = UText::Format(psAddr + 1, sizeof(psAddr) - 1, "%d.%d.%d.%d:%lu", *pAddr, *(pAddr + 1), *(pAddr + 2), *(pAddr + 3), (**pp).port);

		Uint8 *pName = (**pp).data;
		Uint8 *pDesc = pName + pName[0] + 1;
		Uint8 *pPass = pDesc + pDesc[0] + 1;
		
		Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-20.20s %5.5lu  %#-20.20s  %#-26.26s  %#s", psAddr, (**pp).userCount, pName, pDesc, pPass);

		if (pTextView->IsEmpty())
			pTextView->InsertText(max_Uint32, bufData + 1, nSize - 1);
		else
			pTextView->InsertText(max_Uint32, bufData, nSize);
	
		pp++;
	}
	
	pTextView->SetFullHeight();
	mServersWin->ScrollToBottom();	
}

void CMyTracker::UpdateFolderName()
{
	if (!mTrackerFolder)
		return;
	
	Uint8 psOldName[256];
	mTrackerFolder->GetName(psOldName);
	
	Uint8 psNewName[16];
	psNewName[0] = UTransport::IPAddressToText(mIPAddress, psNewName + 1, sizeof(psNewName) - 1);
	
	if (UMemory::Compare(psOldName + 1, psOldName[0], psNewName + 1, psNewName[0]))
		mTrackerFolder->SetName(psNewName);
}

void CMyTracker::Log(const Uint8 *inData, Uint32 inSize)
{
	CTextView *pTextView = mLogWin->GetTextView();
	
	// clear log if getting too big
	THdl hText = pTextView->GetTextHandle();
	if (hText)
	{
		if (UMemory::GetSize(hText) > 16384)
			pTextView->DeleteText(0, max_Uint32);
	}

	const Uint8 *pData = inData;
	Uint32 nSize = inSize;
	
	if (pTextView->IsEmpty())
	{
		pData++;
		nSize--;
	}

	// append to log
	pTextView->InsertText(max_Uint32, pData, nSize);
	pTextView->SetFullHeight();
	mLogWin->ScrollToBottom();
		
	if ((mOpts & myOpt_LogToFile) && mTrackerFolder)
	{
		// also log to log file
		try
		{
			StFileSysRef logFile(mTrackerFolder, nil, "\pServerReg Log.txt");
			if (!logFile->Exists())
				logFile->CreateFile('TEXT', 'ttxt');
			
			pData = inData;
			nSize = inSize;
			
			if (!logFile->GetSize())
			{
				pData++;
				nSize--;
			}

			StFileOpener open(logFile);
			logFile->Write(logFile->GetSize(), pData, nSize);
		}
		catch(...)
		{
		}
	}
}

void CMyTracker::LogServerConnect(const Uint8 *inName, const SInternetAddress& inAddr, const Uint8 *inPass)
{
	Uint8 psAddr[32];
	Uint8 psDate[64];
	Uint8 bufData[256];

	psAddr[0] = UText::Format(psAddr + 1, sizeof(psAddr) - 1, "%d.%d.%d.%d:%lu", inAddr.host[0], inAddr.host[1], inAddr.host[2], inAddr.host[3], inAddr.port);
	psDate[0] = UDateTime::DateToText(psDate + 1, sizeof(psDate) - 1, kShortDateFullYearText + kTimeWithSecsText);
	Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-22.22s %#-20.20s %#-32.32s %#s", psDate, psAddr, inName, inPass);
	
	Log(bufData, nSize);			
}

void CMyTracker::LogRejectServerConnect(const SInternetAddress& inAddr)
{
	Uint8 psAddr[32];
	Uint8 psDate[64];
	Uint8 bufData[256];

	psAddr[0] = UText::Format(psAddr + 1, sizeof(psAddr) - 1, "%d.%d.%d.%d:%lu", inAddr.host[0], inAddr.host[1], inAddr.host[2], inAddr.host[3], inAddr.port);
	psDate[0] = UDateTime::DateToText(psDate + 1, sizeof(psDate) - 1, kShortDateFullYearText + kTimeWithSecsText);
	Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-22.22s %#-20.20s %#s", psDate, psAddr, "\pServer Rejected");
	
	Log(bufData, nSize);			
}

void CMyTracker::LogStartStop(bool inStartStop)
{
	Uint8 psAddr[16];
	Uint8 psDate[64];
	Uint8 bufData[256];

	Uint8 *psStartStop;
	if (inStartStop)
		psStartStop = "\pHotline Tracker started";
	else
		psStartStop = "\pHotline Tracker stopped";

	psDate[0] = UDateTime::DateToText(psDate + 1, sizeof(psDate) - 1, kShortDateFullYearText + kTimeWithSecsText);
	psAddr[0] = UText::Format(psAddr + 1, sizeof(psAddr) - 1, "%d.%d.%d.%d", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
	Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-22.22s %#-20.20s %#s", psDate, psAddr, psStartStop);

	Log(bufData, nSize);
}

void CMyTracker::LogReset()
{
	Uint8 psAddr[16];
	Uint8 psDate[64];
	Uint8 bufData[256];

	psDate[0] = UDateTime::DateToText(psDate + 1, sizeof(psDate) - 1, kShortDateFullYearText + kTimeWithSecsText);
	psAddr[0] = UText::Format(psAddr + 1, sizeof(psAddr) - 1, "%d.%d.%d.%d", mIPAddress.un_IP.stB_IP.nB_IP1, mIPAddress.un_IP.stB_IP.nB_IP2, mIPAddress.un_IP.stB_IP.nB_IP3, mIPAddress.un_IP.stB_IP.nB_IP4);
	Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-22.22s %#-20.20s %#s", psDate, psAddr, "\pTRACKER RESET - too many errors");

	Log(bufData, nSize);
}

void CMyTracker::LogText(const Uint8 *inContextName, const Uint8 *inText)
{
	Uint8 psDate[64];
	Uint8 bufData[256];

	psDate[0] = UDateTime::DateToText(psDate + 1, sizeof(psDate) - 1, kShortDateFullYearText + kTimeWithSecsText);
	Uint32 nSize = UText::Format(bufData, sizeof(bufData), "\r%#-22.22s %#-20.20s %#s", psDate, inContextName, inText);

	Log(bufData, nSize);
}

SMyServerInfo *CMyTracker::NewServerInfo(SMyServerInfo *ioInfo, const SInternetAddress& inAddr, Uint16 inUserCount, Uint16 inFlags, Uint32 inPassID, Uint32 inNameCRC, const Uint8 *inName, const Uint8 *inDesc, const Uint8 *inPass)
{
	ASSERT(inName && inDesc);
	
	if (ioInfo == nil)
		ioInfo = (SMyServerInfo *)UMemory::New(sizeof(SMyServerInfo) + inName[0] + inDesc[0] + inPass[0] + 3);
		
	ioInfo->timeStamp = UDateTime::GetSeconds();
	ioInfo->address = *(Uint32 *)inAddr.host;
	ioInfo->passID = inPassID;
	ioInfo->port = inAddr.port;
	ioInfo->userCount = inUserCount;
	ioInfo->nameCRC = inNameCRC;
	ioInfo->flags = inFlags;
	
	UMemory::Copy(ioInfo->data, inName, inName[0] + 1);
	UMemory::Copy(ioInfo->data + inName[0] + 1, inDesc, inDesc[0] + 1);
	UMemory::Copy(ioInfo->data + inName[0] + inDesc[0] + 2, inPass, inPass[0] + 1);

	return ioInfo;
}

Uint32 CMyTracker::GetServerInfoSize(const SMyServerInfo *inInfo)
{
	ASSERT(inInfo);
	
	const Uint8 *pData = inInfo->data;
	
	Uint32 nSize = sizeof(SMyServerInfo);
	nSize += pData[0] + 1;
	pData += pData[0] + 1;
	nSize += pData[0] + 1;
	pData += pData[0] + 1;
	nSize += pData[0] + 1;

	return nSize;
}


/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -
#pragma mark •• Prefs File ••

TFSRefObj* CMyTracker::GetPrefsRef()
{
	const Uint8 kMyPrefsFileName[] = "\pPrefs";
	
	if (!mTrackerFolder)
	{
		if (!mIPAddress.un_IP.stDW_IP.nDW_IP)
			return nil;
		
		Uint8 psFolderName[16];
		psFolderName[0] = UTransport::IPAddressToText(mIPAddress, psFolderName + 1, sizeof(psFolderName) - 1);

		mTrackerFolder = UFS::New(kProgramFolder, nil, psFolderName);
		if (!mTrackerFolder->Exists())
			mTrackerFolder->CreateFolder();
	}

	return UFS::New(mTrackerFolder, nil, kMyPrefsFileName);
}

bool CMyTracker::WritePrefs()
{
	if (!mIPAddress.un_IP.stDW_IP.nDW_IP)
		return false;

	UpdateFolderName();

	TFSRefObj* fp = GetPrefsRef();
	if (!fp) return false;
	
	scopekill(TFSRefObj, fp);

	Uint16 winVis = 0;
	SRect wrLog, wrStats, wrServers;
	wrLog.SetEmpty();
	wrStats.SetEmpty();
	wrServers.SetEmpty();
			
	if (mLogWin)
	{
		if (mLogWin->IsVisible()) winVis |= myWinVis_Log;
		mLogWin->GetBounds(wrLog);
	}
	
	if (mStatsWin)
	{
		if (mStatsWin->IsVisible()) winVis |= myWinVis_Stats;
		mStatsWin->GetBounds(wrStats);
	}
	
	if (mServersWin)
	{
		if (mServersWin->IsVisible()) winVis |= myWinVis_Servers;
		mServersWin->GetBounds(wrServers);
	}

	Uint8 buf[1024];
	CFlatten flat(buf);
	
	// build flat prefs file data
	flat.WriteWord(kMyPrefsVersion);
	flat.WriteWord(winVis);
	flat.WriteShortRect(wrLog);
	flat.WriteShortRect(wrStats);
	flat.WriteShortRect(wrServers);
	flat.Reserve(28);
	flat.WriteLong(mMaxServersPerIP);
	flat.WriteLong(mOpts);
	flat.WriteByte(mIPAddress.un_IP.stB_IP.nB_IP1);
	flat.WriteByte(mIPAddress.un_IP.stB_IP.nB_IP2);
	flat.WriteByte(mIPAddress.un_IP.stB_IP.nB_IP3);
	flat.WriteByte(mIPAddress.un_IP.stB_IP.nB_IP4);
	
	// write the prefs file
	fp->CreateFileAndOpen('pref', 'HTLS', kAlwaysOpenFile, perm_ReadWrite);
	try
	{
		Uint32 nSize = flat.GetSize();
		fp->SetSize(nSize);
		fp->Write(0, buf, nSize);
	}
	catch(...)
	{
		fp->Close();
		fp->DeleteFile();
		throw;
	}
	
	return true;
}

bool CMyTracker::ReadPrefs(SRect *outWinRects, Uint16 *outWinVis)
{
	Uint32 nSize;
	Uint8 buf[1024];
	
	// read prefs file data
	try
	{
		TFSRefObj* fp = GetPrefsRef();
		if (!fp) return false;
		scopekill(TFSRefObj, fp);
	
		fp->Open(perm_Read);
		nSize = fp->Read(0, buf, sizeof(buf));
	}
	catch(...)
	{
		return false;
	}
	
	// prepare to extract prefs data
	if (nSize < 50) return false;;
	CUnflatten unflat(buf, nSize);

	Uint16 nVers = unflat.ReadWord();
	if (nVers != kMyPrefsVersion) return false;
	
	// extract prefs data
	*outWinVis = unflat.ReadWord();
	unflat.ReadShortRect(outWinRects[0]);
	unflat.ReadShortRect(outWinRects[1]);
	unflat.ReadShortRect(outWinRects[2]);
	unflat.Skip(28);
	mMaxServersPerIP = unflat.ReadLong();
	mOpts = unflat.ReadLong();
	mIPAddress.un_IP.stB_IP.nB_IP1 = unflat.ReadByte();
	mIPAddress.un_IP.stB_IP.nB_IP2 = unflat.ReadByte();
	mIPAddress.un_IP.stB_IP.nB_IP3 = unflat.ReadByte();
	mIPAddress.un_IP.stB_IP.nB_IP4 = unflat.ReadByte();

	UpdateFolderName();
	return true;
}


/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -
#pragma mark •• Login File ••

TFSRefObj* CMyTracker::GetLoginRef()
{
	if (!mTrackerFolder)
		return nil;
	
	return UFS::New(mTrackerFolder, nil, "\pLogin");
}

void CMyTracker::MakeLoginBackup()
{
	if (!mTrackerFolder)
		return;
	
	TFSRefObj* fpb = UFS::New(mTrackerFolder, nil, "\pLogin.bak");
	scopekill(TFSRefObj, fpb);
	
	if (fpb->Exists())
		fpb->DeleteFile();
			
	TFSRefObj* fpo = GetLoginRef();
	scopekill(TFSRefObj, fpo);
	
	if (fpo->Exists())
		fpo->SetName("\pLogin.bak");
}

void CMyTracker::WriteLoginInfo(CPtrList<SMyLoginInfo> *inLoginList)
{
	MakeLoginBackup();

	Uint32 nLoginCount = inLoginList->GetItemCount();
	if (!nLoginCount)
		return;

	// get FS ref for login file
	TFSRefObj* fp = GetLoginRef();
	if (!fp)
		return;
	
	scopekill(TFSRefObj, fp);
		
	ClearLoginList();
	
	// write login file
	fp->CreateFileAndOpen('logn', 'HTLS', kAlwaysOpenFile, perm_ReadWrite);
	
	try
	{
		Uint16 nLoginVersion = TB((Uint16)kMyLoginVersion);
		Uint32 nOffset = fp->Write(0, &nLoginVersion, 2);

	#if CONVERT_INTS
		nLoginCount = swap_int(nLoginCount);
	#endif

		nOffset += fp->Write(nOffset, &nLoginCount, 4);
		
		Uint32 i = 0;
		SMyLoginInfo *pLoginInfo;
		
		while (inLoginList->GetNext(pLoginInfo, i))
		{
			nOffset += fp->Write(nOffset, pLoginInfo, sizeof(SMyLoginInfo));
			
			if (pLoginInfo->nActive == 1)
			{
				SMyLogin *pLogin = (SMyLogin *)UMemory::NewClear(sizeof(SMyLogin));
				UMemory::Copy(pLogin->psLogin, pLoginInfo->psLogin, pLoginInfo->psLogin[0] + 1);
				UMemory::Copy(pLogin->psPassword, pLoginInfo->psPassword, pLoginInfo->psPassword[0] + 1);
				mLoginList.AddItem(pLogin);
			}
						
			UMemory::Dispose((TPtr)pLoginInfo);
		}
		
		inLoginList->Clear();
		fp->Close();
	}
	catch(...)
	{
		inLoginList->Clear();
		fp->Close();
		fp->DeleteFile();
		throw;
	}
}

void CMyTracker::ReadLoginInfo(CPtrList<SMyLoginInfo> *outLoginList)
{
	TFSRefObj* fp = GetLoginRef();
	if (!fp)
		return;
	
	scopekill(TFSRefObj, fp);
	if (!fp->Exists())
		return;
		
	StFileOpener fop(fp, perm_Read);

	// read login file
	try
	{
		Uint16 nLoginVersion;
		Uint32 nOffset = fp->Read(0, &nLoginVersion, 2);
		
		if (nLoginVersion != TB((Uint16)kMyLoginVersion))
			return;
				
		Uint32 nLoginCount = 0;
		nOffset += fp->Read(nOffset, &nLoginCount, 4);
	
	#if CONVERT_INTS
		nLoginCount = swap_int(nLoginCount);
	#endif
	
		if (nLoginCount <= 0)
		{
			fp->DeleteFile();
			return;
		}
		
		while (nLoginCount--)
		{
			SMyLoginInfo *pLoginInfo = (SMyLoginInfo *)UMemory::NewClear(sizeof(SMyLoginInfo));
			
			if (fp->Read(nOffset, pLoginInfo, sizeof(SMyLoginInfo)) != sizeof(SMyLoginInfo))
			{
				UMemory::Dispose((TPtr)pLoginInfo);
				return;
			}
			
			nOffset += sizeof(SMyLoginInfo);
						
			outLoginList->AddItem(pLoginInfo);
		}
	}
	catch(...)
	{
		fp->DeleteFile();
		throw;
	}	
}

void CMyTracker::ReadLoginList()
{
	TFSRefObj* fp = GetLoginRef();
	if (!fp)
		return;

	scopekill(TFSRefObj, fp);
	if (!fp->Exists())
		return;
		
	StFileOpener fop(fp, perm_Read);

	// read login file
	try
	{
		Uint16 nLoginVersion;
		Uint32 nOffset = fp->Read(0, &nLoginVersion, 2);

		if (nLoginVersion != TB((Uint16)kMyLoginVersion))
			return;
				
		Uint32 nLoginCount = 0;
		nOffset += fp->Read(nOffset, &nLoginCount, 4);

	#if CONVERT_INTS
		nLoginCount = swap_int(nLoginCount);
	#endif

		if (nLoginCount <= 0)
		{
			fp->DeleteFile();
			return;
		}
		
		SMyLoginInfo stLoginInfo;
		while (nLoginCount--)
		{					
			if (fp->Read(nOffset, &stLoginInfo, sizeof(SMyLoginInfo)) != sizeof(SMyLoginInfo))
				return;
			
			nOffset += sizeof(SMyLoginInfo);
									
			if (stLoginInfo.nActive == 1)
			{
				SMyLogin *pLogin = (SMyLogin *)UMemory::NewClear(sizeof(SMyLogin));
				UMemory::Copy(pLogin->psLogin, stLoginInfo.psLogin, stLoginInfo.psLogin[0] + 1);
				UMemory::Copy(pLogin->psPassword, stLoginInfo.psPassword, stLoginInfo.psPassword[0] + 1);
				mLoginList.AddItem(pLogin);
			}
		}
	}
	catch(...)
	{
		fp->DeleteFile();
		throw;
	}	
}

bool CMyTracker::IsInvalidPassword(const Uint8 *inPass)
{
	if (!mLoginList.GetItemCount())
		return false;

	Uint32 i = 0;
	SMyLogin *pLogin;

	while (mLoginList.GetNext(pLogin, i))
	{
		if (!UMemory::Compare(pLogin->psPassword + 1, pLogin->psPassword[0], inPass + 1, inPass[0]))
			return false;
	}

	return true;
}

void CMyTracker::ClearLoginList()
{
	Uint32 i = 0;
	SMyLogin *pLogin;
	
	while (mLoginList.GetNext(pLogin, i))
		UMemory::Dispose((TPtr)pLogin);

	mLoginList.Clear();
}


/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -
#pragma mark •• Ban File ••

TFSRefObj* CMyTracker::GetPermBanRef()
{
	if (!mTrackerFolder)
		return nil;
	
	return UFS::New(mTrackerFolder, nil, "\pBanned");
}

void CMyTracker::MakePermBanBackup()
{
	if (!mTrackerFolder)
		return;
	
	TFSRefObj* fpb = UFS::New(mTrackerFolder, nil, "\pBanned.bak");
	scopekill(TFSRefObj, fpb);
	
	if (fpb->Exists())
		fpb->DeleteFile();
			
	TFSRefObj* fpo = GetPermBanRef();
	scopekill(TFSRefObj, fpo);
	
	if (fpo->Exists())
		fpo->SetName("\pBanned.bak");
}

void CMyTracker::WritePermBanInfo(CPtrList<SMyPermBanInfo> *inPermBanList)
{
	MakePermBanBackup();

	Uint32 nPermBanCount = inPermBanList->GetItemCount();
	if (!nPermBanCount)
		return;

	// get FS ref for ban file
	TFSRefObj* fp = GetPermBanRef();
	if (!fp)
		return;
	
	scopekill(TFSRefObj, fp);
		
	ClearPermBanList();
	
	// write ban file
	fp->CreateFileAndOpen('bann', 'HTLS', kAlwaysOpenFile, perm_ReadWrite);
	
	try
	{
		Uint16 nPermBanVersion = TB((Uint16)kMyPermBanVersion);
		Uint32 nOffset = fp->Write(0, &nPermBanVersion, 2);

	#if CONVERT_INTS
		nPermBanCount = swap_int(nPermBanCount);
	#endif

		nOffset += fp->Write(nOffset, &nPermBanCount, 4);
		
		Uint32 i = 0;
		SMyPermBanInfo *pPermBanInfo;
		
		while (inPermBanList->GetNext(pPermBanInfo, i))
		{
			Uint16 nSize = sizeof(SMyPermBanInfo) - sizeof(pPermBanInfo->psDescr);
			nOffset += fp->Write(nOffset, pPermBanInfo, nSize);
			nOffset += fp->Write(nOffset, (Uint8 *)pPermBanInfo + nSize, pPermBanInfo->psDescr[0] + 1);
			
			if (pPermBanInfo->nActive == 1)
			{
				SMyPermBan *pPermBan = (SMyPermBan *)UMemory::NewClear(sizeof(SMyPermBan));
				pPermBan->nAddr = pPermBanInfo->nAddr;
				mPermBanList.AddItem(pPermBan);
			}
						
			UMemory::Dispose((TPtr)pPermBanInfo);
		}
		
		inPermBanList->Clear();
		fp->Close();
	}
	catch(...)
	{
		inPermBanList->Clear();
		fp->Close();
		fp->DeleteFile();
		throw;
	}
}

void CMyTracker::ReadPermBanInfo(CPtrList<SMyPermBanInfo> *outPermBanList)
{
	TFSRefObj* fp = GetPermBanRef();
	if (!fp)
		return;
	
	scopekill(TFSRefObj, fp);
	if (!fp->Exists())
		return;
		
	StFileOpener fop(fp, perm_Read);

	// read ban file
	try
	{
		Uint16 nPermBanVersion;
		Uint32 nOffset = fp->Read(0, &nPermBanVersion, 2);
		
		if (nPermBanVersion !=	TB((Uint16)kMyPermBanVersion))
			return;
				
		Uint32 nPermBanCount = 0;
		nOffset += fp->Read(nOffset, &nPermBanCount, 4);
	
	#if CONVERT_INTS
		nPermBanCount = swap_int(nPermBanCount);
	#endif
	
		if (nPermBanCount <= 0)
		{
			fp->DeleteFile();
			return;
		}
		
		while (nPermBanCount--)
		{
			SMyPermBanInfo *pPermBanInfo = (SMyPermBanInfo *)UMemory::NewClear(sizeof(SMyPermBanInfo));
			
			Uint16 nSize = sizeof(SMyPermBanInfo) - sizeof(pPermBanInfo->psDescr);
			if (fp->Read(nOffset, pPermBanInfo, nSize) != nSize)
			{
				UMemory::Dispose((TPtr)pPermBanInfo);
				return;
			}
			
			nOffset += nSize;
			
			Uint8 nPsSize;
			if (fp->Read(nOffset, &nPsSize, 1) != 1)
			{
				UMemory::Dispose((TPtr)pPermBanInfo);
				return;
			}
			
			nPsSize += 1;
			if (fp->Read(nOffset, (Uint8 *)pPermBanInfo + nSize, nPsSize) != nPsSize)
			{
				UMemory::Dispose((TPtr)pPermBanInfo);
				return;
			}
			
			nOffset += nPsSize;
			
			outPermBanList->AddItem(pPermBanInfo);
		}
	}
	catch(...)
	{
		fp->DeleteFile();
		throw;
	}	
}

void CMyTracker::ReadPermBanList()
{
	TFSRefObj* fp = GetPermBanRef();
	if (!fp)
		return;

	scopekill(TFSRefObj, fp);
	if (!fp->Exists())
		return;
		
	StFileOpener fop(fp, perm_Read);

	// read ban file
	try
	{
		Uint16 nPermBanVersion;
		Uint32 nOffset = fp->Read(0, &nPermBanVersion, 2);

		if (nPermBanVersion !=	TB((Uint16)kMyPermBanVersion))
			return;
				
		Uint32 nPermBanCount = 0;
		nOffset += fp->Read(nOffset, &nPermBanCount, 4);

	#if CONVERT_INTS
		nPermBanCount = swap_int(nPermBanCount);
	#endif

		if (nPermBanCount <= 0)
		{
			fp->DeleteFile();
			return;
		}
		
		SMyPermBanInfo stPermBanInfo;
		while (nPermBanCount--)
		{					
			Uint16 nSize = sizeof(SMyPermBanInfo) - sizeof(stPermBanInfo.psDescr);
			if (fp->Read(nOffset, &stPermBanInfo, nSize) != nSize)
				return;
			
			nOffset += nSize;
			
			Uint8 nPsSize;
			if(fp->Read(nOffset, &nPsSize, 1) != 1)
				return;
			
			nOffset += nPsSize + 1;
						
			if (stPermBanInfo.nActive == 1)
			{
				SMyPermBan *pPermBan = (SMyPermBan *)UMemory::NewClear(sizeof(SMyPermBan));
				pPermBan->nAddr = stPermBanInfo.nAddr;
				mPermBanList.AddItem(pPermBan);
			}
		}
	}
	catch(...)
	{
		fp->DeleteFile();
		throw;
	}	
}

bool CMyTracker::IsBannedAddress(const SInternetAddress& inAddr)
{
	Uint32 nAddr = *(Uint32 *)inAddr.host;
	
	Uint32 i = 0;
	SMyPermBan *pPermBan;

	while (mPermBanList.GetNext(pPermBan, i))
	{
		if (pPermBan->nAddr == nAddr)
			return true;
	}
	
	return false;
}

void CMyTracker::ClearPermBanList()
{
	Uint32 i = 0;
	SMyPermBan *pPermBan;

	while (mPermBanList.GetNext(pPermBan, i))
		UMemory::Dispose((TPtr)pPermBan);

	mPermBanList.Clear();
}


/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -

CMyApplication::CMyApplication()
{
	const SKeyCmd keyCmds[] = {
		{	viewID_Quit,				'q',		modKey_Shortcut					}
	};

	mToolbarWin = nil;

	mRemoveOldTimer = NewTimer();
	mErrorCount = mResetCount = 0;
	
	InstallKeyCommands(keyCmds, sizeof(keyCmds) / sizeof(SKeyCmd));
}

CMyApplication::~CMyApplication()
{
	WritePrefs();

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
		delete pTracker;

	mTrackerList.Clear();
	
	UTimer::Dispose(mRemoveOldTimer);
}

void CMyApplication::StartUp()
{
	Uint16 winVis = myWinVis_Toolbar;
	SRect winRect[1];
	winRect[0].SetEmpty();
	
	ReadPrefs(winRect, &winVis);

	// make toolbar window
	mToolbarWin = new CMyToolbarWin;
	UWindow::SetMainWindow(*mToolbarWin);
	if (winRect[0].IsEmpty())
	{
		mToolbarWin->SetAutoBounds(windowPos_TopRight, windowPosOn_WinScreen);
	}
	else
	{
		mToolbarWin->SetLocation(winRect[0].TopLeft());
		mToolbarWin->SetAutoBounds(windowPos_Best, windowPosOn_WinScreen);
	}	
	if (!(winVis & myWinVis_Toolbar)) mToolbarWin->ToggleState();
	mToolbarWin->Show();
	
	ReadTrackers();

	// start timer to regularly remove outdated servers from the list
	mRemoveOldTimer->Start(300000, kRepeatingTimer);
}

void CMyApplication::KeyCommand(Uint32 inCmd, const SKeyMsgData& inInfo)
{
	#pragma unused(inInfo)
	
	switch (inCmd)
	{
		case viewID_Quit:
			UserQuit();
			break;
	};
}

void CMyApplication::WindowHit(CWindow *inWindow, const SHitMsgData& inInfo)
{
	switch (inInfo.id)
	{
		case viewID_GlobalStats:
			mToolbarWin->ToggleState();
			break;

		case viewID_NewTracker:
			DoNewTracker();
			break;

		case viewID_EditTracker:
		case viewID_TrackerList:
			DoEditTracker();
			break;
	
		case viewID_DeleteTracker:
			DoDeleteTracker();
			break;

		case viewID_ShowLog:
			DoShowTrackerLog();
			break;

		case viewID_ShowStats:
			DoShowTrackerStats();
			break;

		case viewID_ShowServers:
			DoShowTrackerServers();
			break;

		case viewID_Quit:
			UserQuit();
			break;

		case CWindow::ClassID:
			inWindow->Hide();
			break;
	}
}

void CMyApplication::Timer(TTimer inTimer)
{
	if (inTimer == mRemoveOldTimer)
		RemoveOldServers();
}

void CMyApplication::Error(const SError& inInfo)
{
	mErrorCount++;
	if (mErrorCount > 100)
	{
		DebugBreak("Too many errors! Resetting...");
		try
		{
			ResetTrackerServ();
		}
		catch(...)
		{
			DebugBreak("Reset failed! Terminating...");
			UApplication::Abort();
		}
	}

	StFileSysRef logFile(kProgramFolder, nil, kMyErrorLogFileName);
	UError::Log(logFile, inInfo);
	USound::Play(nil, 130, true);

	// just in case there was unfinished work to do
	UApplication::PostMessage(msg_DataArrived);
}

void CMyApplication::SelectionChanged(SIPAddress inTrackerIP, bool inSelect)
{
	if (!inSelect)
		return;
	
	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == inTrackerIP.un_IP.stDW_IP.nDW_IP)
		{
			pTracker->BringToFrontWin();
			break;
		}
	}
}

void CMyApplication::CheckChanged(Uint32 inIndex, SIPAddress inTrackerIP, bool inActive)
{
	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == inTrackerIP.un_IP.stDW_IP.nDW_IP)
		{
			if (inActive)
			{
				if (!pTracker->IsActive())
					pTracker->StartTracker();
					
				if (!pTracker->IsActive())
					mToolbarWin->UpdateTracker(inIndex, inTrackerIP, false);
			}
			else
			{
				if (pTracker->IsActive())
					pTracker->StopTracker();
			}
			
			break;
		}
	}
}

bool CMyApplication::IsInTrackerList(SIPAddress inTrackerIP)
{
	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == inTrackerIP.un_IP.stDW_IP.nDW_IP)
			return true;
	}

	return false;
}

void CMyApplication::UpdateGlobalStats()
{
	Uint32 i = 0;
	CMyTracker *pTracker;
	
	Uint32 nServerCount = 0;
	Uint32 nUserCount = 0;
	Uint32 nListingCounter = 0;

	while(mTrackerList.GetNext(pTracker, i))
	{
		nServerCount += pTracker->GetServerCount();
		nUserCount += pTracker->GetUserCount();
		nListingCounter += pTracker->GetListingCounter();
	}

	mToolbarWin->SetGlobalStats(nServerCount, nUserCount, nListingCounter);
}

void CMyApplication::ReadTrackers()
{
	Uint32 nOffset = 0;
	Uint32 nTypeCode, nFlags;
	Uint8 psFolderName[256];
	
	TFSRefObj* fpFolder = UFileSys::New(kProgramFolder, nil, nil);
	THdl hList = fpFolder->GetListing();
	UFileSys::Dispose(fpFolder);

	try
	{
		while (UFS::GetListNext(hList, nOffset, psFolderName, &nTypeCode, nil, nil, nil, nil, &nFlags))
		{
			if ((nFlags & 1) == 0 && nTypeCode == TB((Uint32)'fldr'))	// if visible and folder
			{
				fpFolder = UFileSys::New(kProgramFolder, psFolderName, nil);

				CMyTracker *pTracker = new CMyTracker(fpFolder);
				if (!AddTracker(pTracker))
					delete pTracker;
			}
		}
	}
	catch(...)
	{
		UMemory::Dispose(hList);
		throw;
	}

	UMemory::Dispose(hList);

	if (!mTrackerList.GetItemCount())
	{
		CMyTracker *pTracker = new CMyTracker(nil);
		if (!AddTracker(pTracker))
			delete pTracker;
	}
	
	mToolbarWin->SelectFirstTracker();
}

bool CMyApplication::AddTracker(CMyTracker *inTracker)
{
	SIPAddress stIPAddress = inTracker->GetIPAddress();
	if (!stIPAddress.un_IP.stDW_IP.nDW_IP)
		return false;

	mTrackerList.AddItem(inTracker);
	mToolbarWin->AddTracker(stIPAddress, inTracker->IsActive());
	return true;
}

void CMyApplication::DoNewTracker()
{
	CMyTracker *pTracker = new CMyTracker(nil);
	
	if (!pTracker->EditTracker(true))
		delete pTracker;
	else if (!AddTracker(pTracker))
		delete pTracker;
}

void CMyApplication::DoEditTracker()
{
	SIPAddress stIPAddress;
	Uint32 nIndex = mToolbarWin->GetSelectedTrackerIP(stIPAddress);
	if (!nIndex)
		return;

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == stIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			if (pTracker->EditTracker(false))
				mToolbarWin->UpdateTracker(nIndex, pTracker->GetIPAddress(), pTracker->IsActive());

			break;
		}
	}
}

void CMyApplication::DoDeleteTracker()
{
	SIPAddress stIPAddress;
	if (!mToolbarWin->GetSelectedTrackerIP(stIPAddress))
		return;

	SMsgBox mb;
	ClearStruct(mb);

	Uint8 psMessage[256];
	psMessage[0] = UText::Format(psMessage + 1, sizeof(psMessage) - 1, "Are you sure you want to delete the tracker %d.%d.%d.%d?", stIPAddress.un_IP.stB_IP.nB_IP1, stIPAddress.un_IP.stB_IP.nB_IP2, stIPAddress.un_IP.stB_IP.nB_IP3, stIPAddress.un_IP.stB_IP.nB_IP4);

	mb.icon = icon_Caution;
	mb.title = "\pDelete Tracker";
	mb.messageSize = psMessage[0];
	mb.messageData = psMessage + 1;
	mb.button1 = "\pCancel";
	mb.button2 = "\pDelete";
	
	if (MsgBox(mb) != 2)
		return;

	mToolbarWin->DeleteSelectedTracker();

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == stIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			pTracker->DeleteTracker();
			mTrackerList.RemoveItem(pTracker);
			delete pTracker;
			break;
		}
	}
}

void CMyApplication::DoShowTrackerLog()
{
	SIPAddress stIPAddress;
	if (!mToolbarWin->GetSelectedTrackerIP(stIPAddress))
		return;

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == stIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			pTracker->ShowLogWin();
			break;
		}
	}
}

void CMyApplication::DoShowTrackerStats()
{
	SIPAddress stIPAddress;
	if (!mToolbarWin->GetSelectedTrackerIP(stIPAddress))
		return;

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == stIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			pTracker->ShowStatsWin();
			break;
		}
	}
}

void CMyApplication::DoShowTrackerServers()
{
	SIPAddress stIPAddress;
	if (!mToolbarWin->GetSelectedTrackerIP(stIPAddress))
		return;

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
	{
		if (pTracker->GetIPAddress().un_IP.stDW_IP.nDW_IP == stIPAddress.un_IP.stDW_IP.nDW_IP)
		{
			pTracker->ShowServersWin();
			break;
		}
	}
}

void CMyApplication::ResetErrorCount()
{
	mErrorCount = mResetCount = 0;
}

void CMyApplication::RemoveOldServers()
{
	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
		pTracker->RemoveOldServers();
}

void CMyApplication::ResetTrackerServ()
{
	// if there's been too many resets, abort app
	mResetCount++;
	if (mResetCount > 10)
	{
		DebugBreak("Too many resets! Terminating...");
		UApplication::Abort();
	}

	// reset values
	mErrorCount = 0;

	Uint32 i = 0;
	CMyTracker *pTracker;
	
	while(mTrackerList.GetNext(pTracker, i))
		pTracker->ResetTrackerServ();
}

/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -
#pragma mark •• Prefs File ••

TFSRefObj* CMyApplication::GetPrefsRef()
{
	const Uint8 kMyPrefsFileName[] = "\pPrefs";
	
	return UFS::New(kProgramFolder, nil, kMyPrefsFileName);
}

bool CMyApplication::WritePrefs()
{
	TFSRefObj* fp = GetPrefsRef();
	scopekill(TFSRefObj, fp);

	Uint16 winVis = 0;
	SRect wrToolbar;
	wrToolbar.SetEmpty();
			
	if (mToolbarWin)
	{
		if (!mToolbarWin->IsCollapsed()) winVis |= myWinVis_Toolbar;
		mToolbarWin->GetBounds(wrToolbar);
	}
	
	Uint8 buf[512];
	CFlatten flat(buf);
	
	// build flat prefs file data
	flat.WriteWord(kMyPrefsVersion);
	flat.WriteWord(winVis);
	flat.WriteShortRect(wrToolbar);
	flat.Reserve(32);
	
	// write the prefs file
	fp->CreateFileAndOpen('pref', 'HTLS', kAlwaysOpenFile, perm_ReadWrite);
	try
	{
		Uint32 nSize = flat.GetSize();
		fp->SetSize(nSize);
		fp->Write(0, buf, nSize);
	}
	catch(...)
	{
		fp->Close();
		fp->DeleteFile();
		throw;
	}
	
	return true;
}

bool CMyApplication::ReadPrefs(SRect *outWinRects, Uint16 *outWinVis)
{
	Uint32 nSize;
	Uint8 buf[512];
	
	// read prefs file data
	try
	{
		TFSRefObj* fp = GetPrefsRef();
		scopekill(TFSRefObj, fp);
	
		fp->Open(perm_Read);
		nSize = fp->Read(0, buf, sizeof(buf));
	}
	catch(...)
	{
		return false;
	}
	
	// prepare to extract prefs data
	if (nSize < 8) return false;;
	CUnflatten unflat(buf, nSize);

	Uint16 nVers = unflat.ReadWord();
	if (nVers != kMyPrefsVersion) return false;
	
	// extract prefs data
	*outWinVis = unflat.ReadWord();
	unflat.ReadShortRect(outWinRects[0]);
//	unflat.Skip(32);

	return true;
}