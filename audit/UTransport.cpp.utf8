/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */
/*
-- Errors --

100		An unknown transport related error has occurred.
101		Network address is not valid.
102		Connection unexpectedly closed.
103		Connection timed out waiting for remote host.
104		Remote host refused connection.
105		Remote host is not responding.
106		Remote host is unreachable (no route to host).
107		Transport error:  invalid configuration.
108		Transport error:  pending action incompatible with requested action.
109		TCP/IP ability is required but not available.  Please install the necessary software.
110		Transport support is required but not available.  Please install the necessary software.
111		Transport error:  timed out waiting for data from remote host.
112		Cannot bind to the specified address because it is already in use.
*/

#include "UTransport.h"
#include "UHttpTransport.h"


struct STransport {
	union{
		TRegularTransport tptRegular;
		THttpTransport tptHttp;
	} unTpt;
	bool  bRegular;
};

#define TPT		((STransport *)inTpt)

/* -------------------------------------------------------------------------- */

bool UTransport::IsAvailable()
{
	return URegularTransport::IsAvailable();
}

bool UTransport::HasTCP()
{
	return URegularTransport::HasTCP();
}

void UTransport::Init()
{
	URegularTransport::Init();
	UHttpTransport::Init();
}

/* -------------------------------------------------------------------------- */
#pragma mark -

TTransport UTransport::New(Uint32 inProtocol, bool inRegular, Uint32 inOptions)
{
	STransport *tpt = (STransport *)UMemory::NewClear(sizeof(STransport));

	tpt->bRegular = inRegular;
	
	if (tpt->bRegular)
		tpt->unTpt.tptRegular = URegularTransport::New(inProtocol, inOptions);
	else
		tpt->unTpt.tptHttp = UHttpTransport::New(inProtocol, inOptions);
		
	return (TTransport)tpt;
}

void UTransport::Dispose(TTransport inTpt)
{
	if (inTpt)
	{
		if (TPT->bRegular)
			URegularTransport::Dispose(TPT->unTpt.tptRegular);
		else
			UHttpTransport::Dispose(TPT->unTpt.tptHttp);
			
		UMemory::Dispose((TPtr)inTpt);
	}
}

void UTransport::SetDataMonitor(TTransport inTpt, TTransportMonitorProc inProc)
{	
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::SetDataMonitor(TPT->unTpt.tptRegular, inProc);											
	else
		UHttpTransport::SetDataMonitor(TPT->unTpt.tptHttp, inProc);
}

bool UTransport::IsRegular(TTransport inTpt)
{
	Require(inTpt);
	
	return TPT->bRegular;
}
		
void UTransport::SetMessageHandler(TTransport inTpt, TMessageProc inProc, void *inContext)
{
	Require(inTpt);
	
	if (TPT->bRegular) 
		URegularTransport::SetMessageHandler(TPT->unTpt.tptRegular, inProc, inContext, inTpt);
	else
		UHttpTransport::SetMessageHandler(TPT->unTpt.tptHttp, inProc, inContext, inTpt);
}

void UTransport::PostMessage(TTransport inTpt, Uint32 inMsg, const void *inData, Uint32 inDataSize, Int16 inPriority)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::PostMessage(TPT->unTpt.tptRegular, inMsg, inData, inDataSize, inPriority);
	else
		UHttpTransport::PostMessage(TPT->unTpt.tptHttp, inMsg, inData, inDataSize, inPriority);
}

void UTransport::ReplaceMessage(TTransport inTpt, Uint32 inMsg, const void *inData, Uint32 inDataSize, Int16 inPriority)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::ReplaceMessage(TPT->unTpt.tptRegular, inMsg, inData, inDataSize, inPriority);
	else
		UHttpTransport::ReplaceMessage(TPT->unTpt.tptHttp, inMsg, inData, inDataSize, inPriority);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

SIPAddress UTransport::GetDefaultIPAddress()
{
	return URegularTransport::GetDefaultIPAddress();
}

void UTransport::SetIPAddress(TTransport inTpt, SIPAddress inIPAddress)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::SetIPAddress(TPT->unTpt.tptRegular, inIPAddress);
	else
		UHttpTransport::SetIPAddress(TPT->unTpt.tptHttp, inIPAddress);
}

SIPAddress UTransport::GetIPAddress(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetIPAddress(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::GetIPAddress(TPT->unTpt.tptHttp);
}

Uint32 UTransport::GetRemoteAddressText(TTransport inTpt, void *outAddr, Uint32 inMaxSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetRemoteAddressText(TPT->unTpt.tptRegular, outAddr, inMaxSize);
	else
		return UHttpTransport::GetRemoteAddressText(TPT->unTpt.tptHttp, outAddr, inMaxSize);
}

Uint32 UTransport::GetLocalAddressText(TTransport inTpt, void *outAddr, Uint32 inMaxSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetLocalAddressText(TPT->unTpt.tptRegular, outAddr, inMaxSize);
	else
		return UHttpTransport::GetLocalAddressText(TPT->unTpt.tptHttp, outAddr, inMaxSize);
}

Uint32 UTransport::CleanUpAddressText(Uint16 /* inType */, const void *inAddr, Uint32 inAddrSize, void *outAddr, Uint32 inMaxSize)
{
	Uint8 *p;
	Uint32 n;
	
	// remove leading spaces
	while (inAddrSize)
	{
		if (UText::IsSpace(*(Uint8 *)inAddr))
		{
			inAddr = BPTR(inAddr) + 1;
			inAddrSize--;
		}
		else break;
	}
	
	// count number of non-space chars
	p = (Uint8 *)inAddr;
	n = inAddrSize;
	while (n--)
	{
		if (UText::IsSpace(*p++))
		{
			p--;
			break;
		}
	}
	inAddrSize = p - (Uint8 *)inAddr;
	
	// copy out and replace unprintables
	if (inAddrSize > inMaxSize) inAddrSize = inMaxSize;
	p = (Uint8 *)inAddr;
	n = inAddrSize;
	while (n--)
	{
		if (UText::IsGraph(*p))
			*((Uint8 *)outAddr)++ = *p++;
		else
		{
			*((Uint8 *)outAddr)++ = 'x';
			p++;
		}
	}
	
	// all done!
	return inAddrSize;
}

// returns 0 if no port specified
Uint32 UTransport::GetPortFromAddressText(Uint16 /* inType */, const void *inAddr, Uint32 inAddrSize)
{
	if (inAddrSize == 0) return 0;
	
	Uint8 *p = (Uint8 *)inAddr + inAddrSize;
	Uint32 n = inAddrSize;
	for (n++; --n && *--p != ':';) {}
	if (p == (Uint8 *)inAddr) return 0;		// no colon
	
	Uint32 num = 0;
	p++;
	n = (BPTR(inAddr) + inAddrSize) - p;
	
	while (n--)
	{
		num *= 10;
		num += *p++ - '0';
	}
	
	return num;
}

Uint32 UTransport::GetLocalAddress(TTransport inTpt, void *outAddr, Uint32 inMaxSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetLocalAddress(TPT->unTpt.tptRegular, outAddr, inMaxSize);
	else
		return UHttpTransport::GetLocalAddress(TPT->unTpt.tptHttp, outAddr, inMaxSize);
}

Uint32 UTransport::GetProxyServerAddress(Uint16 inType, void *outProxyAddr, Uint32 inMaxSize, const void *inBypassAddr, Uint32 inBypassAddrSize)
{
	return URegularTransport::GetProxyServerAddress(inType, outProxyAddr, inMaxSize, inBypassAddr, inBypassAddrSize);
}

Uint32 UTransport::IPAddressToText(SIPAddress inIPAddress, void *outAddr, Uint32 inMaxSize)
{
	if (!outAddr || !inMaxSize)
		return 0;
	
	return UText::Format(outAddr, inMaxSize, "%d.%d.%d.%d", inIPAddress.un_IP.stB_IP.nB_IP1, inIPAddress.un_IP.stB_IP.nB_IP2, inIPAddress.un_IP.stB_IP.nB_IP3, inIPAddress.un_IP.stB_IP.nB_IP4);
}

bool UTransport::TextToIPAddress(Uint8 *inAddr, SIPAddress &outIPAddress)
{
	outIPAddress.un_IP.stDW_IP.nDW_IP = 0;

	if (!inAddr || inAddr[0] < 7)
		return false;

	Uint8 nSize = 0;
	Uint8 *pAddr = inAddr;
	
	SIPAddress stIPAddress;
	Uint8 *pIPAddress = (Uint8 *)&stIPAddress;

	Uint8 nCount = 0;
	while (nCount++ < 4)
	{
		pAddr += nSize + 1;
		if (pAddr >= inAddr + inAddr[0] + 1)
			return false;

		if (nCount < 4)
		{
			if(*(pAddr + 1) == '.')
				nSize = 1;
			else if(*(pAddr + 2) == '.')
				nSize = 2;
			else if(*(pAddr + 3) == '.')
				nSize = 3;
			else
				return false;
		}
		else
		{
			nSize = inAddr + inAddr[0] + 1 - pAddr;
			if (nSize > 3)
				nSize = 3;
		}
	
		while (nSize > 1 && *pAddr == '0')
		{
			pAddr++;
			nSize--;
		}
		
		Uint32 nTestSize = nSize;
		Uint8 *pTestAddr = pAddr;
		
		while (nTestSize > 0)
		{
			if (*pTestAddr < '0' || *pTestAddr > '9')
				return false;
		
			pTestAddr++;
			nTestSize--;
		}
		
		Int16 nAddr = UText::TextToInteger(pAddr, nSize);
		if (nAddr < 0 || nAddr > 255)
			return false;
	
		*pIPAddress++ = nAddr;		
	}
		
	outIPAddress = stIPAddress;
	return true;
}

bool UTransport::GetEthernetAddress(SEthernetAddress& outEthernetAddr)
{
	static bool bIsInitted = false;
	static bool bValidAddr = false;
	static SEthernetAddress stEthernetAddr;
	
	if (!bIsInitted)
	{
		bIsInitted = true;
		bValidAddr = URegularTransport::GetEthernetAddress(stEthernetAddr);
	}
	
	if (bValidAddr)
		outEthernetAddr = stEthernetAddr;
	else
		outEthernetAddr.SetNull();
		
	return bValidAddr;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

bool UTransport::IsConnected(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::IsConnected(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::IsConnected(TPT->unTpt.tptHttp);
}

void UTransport::StartConnectThruFirewall(TTransport inTpt, const void *inAddr, Uint32 inAddrSize, const void *inFirewallAddr, Uint32 inFirewallAddrSize, Uint32 inMaxSecs, Uint32 inOptions)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::StartConnectThruFirewall(TPT->unTpt.tptRegular, inAddr, inAddrSize, inFirewallAddr, inFirewallAddrSize, inMaxSecs, inOptions);
	else
		UHttpTransport::StartConnectThruFirewall(TPT->unTpt.tptHttp, inAddr, inAddrSize, inFirewallAddr, inFirewallAddrSize, inMaxSecs, inOptions);
}

void UTransport::StartConnect(TTransport inTpt, const void *inAddr, Uint32 inAddrSize, Uint32 inMaxSecs)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::StartConnect(TPT->unTpt.tptRegular, inAddr, inAddrSize, inMaxSecs);
	else
		UHttpTransport::StartConnect(TPT->unTpt.tptHttp, inAddr, inAddrSize, inMaxSecs);
}

Uint16 UTransport::GetConnectStatus(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetConnectStatus(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::GetConnectStatus(TPT->unTpt.tptHttp);
}

void UTransport::Disconnect(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::Disconnect(TPT->unTpt.tptRegular);
	else
		UHttpTransport::Disconnect(TPT->unTpt.tptHttp);
}

void UTransport::StartDisconnect(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::StartDisconnect(TPT->unTpt.tptRegular);
	else
		UHttpTransport::StartDisconnect(TPT->unTpt.tptHttp);
}

bool UTransport::IsDisconnecting(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::IsDisconnecting(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::IsDisconnecting(TPT->unTpt.tptHttp);
}

bool UTransport::IsConnecting(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::IsConnecting(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::IsConnecting(TPT->unTpt.tptHttp);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransport::Listen(TTransport inTpt, const void *inAddr, Uint32 inAddrSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::Listen(TPT->unTpt.tptRegular, inAddr, inAddrSize);
	else
		UHttpTransport::Listen(TPT->unTpt.tptHttp, inAddr, inAddrSize);
}

TTransport UTransport::Accept(TTransport inTpt, void *outAddr, Uint32 *ioAddrSize)
{
	Require(inTpt);

	STransport *tpt = nil;

	if (TPT->bRegular) 
	{
		TRegularTransport tptRegular = URegularTransport::Accept(TPT->unTpt.tptRegular, outAddr, ioAddrSize);	
		if (!tptRegular)
			return nil;
		
		tpt = (STransport *)UMemory::NewClear(sizeof(STransport));

		tpt->bRegular = true;
		tpt->unTpt.tptRegular = tptRegular;		
	}
	else
	{
		THttpTransport tptHttp = UHttpTransport::Accept(TPT->unTpt.tptHttp, outAddr, ioAddrSize);
		if (!tptHttp)
			return nil;
		
		tpt = (STransport *)UMemory::NewClear(sizeof(STransport));

		tpt->bRegular = false;
		tpt->unTpt.tptHttp = tptHttp;
	}

	return (TTransport)tpt;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void *UTransport::NewBuffer(Uint32 inSize)
{
	return URegularTransport::NewBuffer(inSize);
}

void UTransport::DisposeBuffer(void *inBuf)
{
	URegularTransport::DisposeBuffer(inBuf);
}

Uint32 UTransport::GetBufferSize(void *inBuf)
{
	return 	URegularTransport::GetBufferSize(inBuf);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransport::SendBuffer(TTransport inTpt, void *inBuf)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::SendBuffer(TPT->unTpt.tptRegular, inBuf);
	else
		UHttpTransport::SendBuffer(TPT->unTpt.tptHttp, inBuf);
}

void UTransport::Send(TTransport inTpt, const void *inData, Uint32 inDataSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::Send(TPT->unTpt.tptRegular, inData, inDataSize);
	else
		UHttpTransport::Send(TPT->unTpt.tptHttp, inData, inDataSize);
}

bool UTransport::ReceiveBuffer(TTransport inTpt, void*& outBuf, Uint32& outSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::ReceiveBuffer(TPT->unTpt.tptRegular, outBuf, outSize);
	else
		return UHttpTransport::ReceiveBuffer(TPT->unTpt.tptHttp, outBuf, outSize);
}

Uint32 UTransport::Receive(TTransport inTpt, void *outData, Uint32 inMaxSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::Receive(TPT->unTpt.tptRegular, outData, inMaxSize);
	else
		return UHttpTransport::Receive(TPT->unTpt.tptHttp, outData, inMaxSize);
}

Uint32 UTransport::GetReceiveSize(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetReceiveSize(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::GetReceiveSize(TPT->unTpt.tptHttp);
}

Uint32 UTransport::GetUnsentSize(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::GetUnsentSize(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::GetUnsentSize(TPT->unTpt.tptHttp);
}

void UTransport::NotifyReadyToReceive(TTransport inTpt, Uint32 inReceivedSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::NotifyReadyToReceive(TPT->unTpt.tptRegular, inReceivedSize);
	else
		UHttpTransport::NotifyReadyToReceive(TPT->unTpt.tptHttp, inReceivedSize);
}

void UTransport::NotifyReadyToSend(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::NotifyReadyToSend(TPT->unTpt.tptRegular);
	else
		UHttpTransport::NotifyReadyToSend(TPT->unTpt.tptHttp);
}

bool UTransport::IsReadyToReceive(TTransport inTpt, Uint32 inReceivedSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::IsReadyToReceive(TPT->unTpt.tptRegular, inReceivedSize);
	else
		return UHttpTransport::IsReadyToReceive(TPT->unTpt.tptHttp, inReceivedSize);
}

bool UTransport::IsReadyToSend(TTransport inTpt)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::IsReadyToSend(TPT->unTpt.tptRegular);
	else
		return UHttpTransport::IsReadyToSend(TPT->unTpt.tptHttp);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransport::SendUnit(TTransport inTpt, 
						  const void *inAddr, Uint32 inAddrSize, 
						  const void *inData, Uint32 inDataSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		URegularTransport::SendUnit(TPT->unTpt.tptRegular, inAddr, inAddrSize, inData, inDataSize);
	else
		UHttpTransport::SendUnit(TPT->unTpt.tptHttp, inAddr, inAddrSize, inData, inDataSize);
}

bool UTransport::ReceiveUnit(TTransport inTpt, 
							 void *outAddr, Uint32& ioAddrSize, 
							 void *outData, Uint32& ioDataSize)
{
	Require(inTpt);

	if (TPT->bRegular) 
		return URegularTransport::ReceiveUnit(TPT->unTpt.tptRegular, outAddr, ioAddrSize, outData, ioDataSize);
	else
		return UHttpTransport::ReceiveUnit(TPT->unTpt.tptHttp, outAddr, ioAddrSize, outData, ioDataSize);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

bool UTransport::IsRegisteredProtocol(const Uint8 inProtocol[])
{
	return URegularTransport::IsRegisteredProtocol(inProtocol);
}

bool UTransport::IsRegisteredForProtocol(const Uint8 inProtocol[])
{
	return URegularTransport::IsRegisteredForProtocol(inProtocol);
}

bool UTransport::RegisterProtocol(const Uint8 inProtocol[])
{
	return URegularTransport::RegisterProtocol(inProtocol);
}

bool UTransport::LaunchURL(const void *inText, Uint32 inTextSize, Uint32 *ioSelStart, Uint32 *ioSelEnd)
{
	return URegularTransport::LaunchURL(inText, inTextSize, ioSelStart, ioSelEnd);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// returns false if invalid URL
bool _GetURLAddress(const void *inText, Uint32 inTextSize, Uint32& outStart, Uint32& outSize)
{
	//	protocol://user:password@server/files/...
	//  protocol:server/news/...

	Uint8 *p, *ep, *xp;
		
	p = (Uint8 *)UMemory::SearchByte(':', inText, inTextSize);
	if (p == nil) return false;
	ep = BPTR(inText) + inTextSize;
	p++;
	
	if (ep - p < 2) return false;
	if (*p == '\\') return false;

	if (*p == '/')
	{
		p++;
		if (*p == '/') p++;
	}
	
	outStart = p - BPTR(inText);
	
	xp = (Uint8 *)UMemory::SearchByte('/', p, ep - p);
	
	if (xp == nil)
		outSize = ep - p;
	else
		outSize = xp - p;

	return true;
}

bool _GetLoginPassURLAddress(const void *inText, Uint32 inTextSize, Uint32& outLoginStart, Uint32& outLoginSize, Uint32& outPassStart, Uint32& outPassSize, Uint32& outAddressStart, Uint32& outAddressSize, Uint32& outExtraStart, Uint32& outExtraSize)
{
	//	protocol://user:password@server/files/...
	//  protocol:server/news/...
	
	outLoginStart = outLoginSize = outPassStart = outPassSize = outAddressStart = outAddressSize = outExtraStart = outExtraSize = 0;

	Uint8 *pAddStart = (Uint8 *)UMemory::SearchByte(':', inText, inTextSize);
	if (pAddStart == nil) return false;
	Uint8 *pAddEnd = BPTR(inText) + inTextSize;
	pAddStart++;
	
	if (pAddEnd - pAddStart < 2) return false;
	if (*pAddStart == '\\') return false;

	if (*pAddStart == '/')
	{
		pAddStart++;
		if (*pAddStart == '/') pAddStart++;
	}
		
	Uint8 *pExtraInfo = (Uint8 *)UMemory::SearchByte('/', pAddStart, pAddEnd - pAddStart);
	
	if (pExtraInfo)
	{		
		pExtraInfo++;
		outExtraStart = pExtraInfo - BPTR(inText);
		outExtraSize = pAddEnd - pExtraInfo;
		pAddEnd = pExtraInfo - 1;
	}

	Uint8 *pServerStart = (Uint8 *)UMemory::SearchByte('@', pAddStart, pAddEnd - pAddStart);

	if (pServerStart)
	{
		pServerStart++;
		outAddressStart = pServerStart - BPTR(inText);
		outAddressSize = pAddEnd - pServerStart;
		pAddEnd = pServerStart - 1;
		
		Uint8 *pPassStart = (Uint8 *)UMemory::SearchByte(':', pAddStart, pAddEnd - pAddStart);

		if (pPassStart)
		{
			pPassStart++;
			outPassStart = pPassStart - BPTR(inText);
			outPassSize = pAddEnd - pPassStart;
			pAddEnd = pPassStart - 1;
		}
		
		outLoginStart = pAddStart - BPTR(inText);
		outLoginSize = pAddEnd - pAddStart;
	}
	else
	{
		outAddressStart = pAddStart - BPTR(inText);
		outAddressSize = pAddEnd - pAddStart;
	}

	return true;
}

