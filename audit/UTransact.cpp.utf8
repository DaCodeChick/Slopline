/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#include "UTransact.h"
#include "UTransport.h"
#include "HLCrypt.h"

HLCrypt *gCrypt = nil;

enum {
	kOutTask = 1, kOutReplyTask, kInTask, kInReplyTask
};

#pragma options align=packed
struct STranHdr {
	Uint8 flag;							// reserved, should be 0
	Uint8 isReply;						// is this transaction a reply?
	Uint16 type;						// type of transaction
	Uint32 id;							// arbitrary value used for reply
	Uint32 error;						// error code used for reply
	Uint32 totalSize;					// for splitting large transactions
	Uint32 dataSize;					// number of bytes in following string
	Uint8 data[];						// data for this transaction
};
#pragma options align=reset

struct STranTask {
	STranTask *next;
	THdl dataHdl;			// data to send OR data received
	Uint32 totalSize;		// total size of data for this transaction
	Uint32 transferedSize;	// number of bytes sent/received so far
	Uint32 tranID;			// transaction ID
	Uint32 tranError;		// transaction error
	Uint16 tranType;		// transaction type
	Uint16 taskType;
	Uint16 needsSend : 1;
	Uint16 isBeingReceived : 1;
	Uint16 hasReceivedData : 1;
};

struct STransact {
	TTransport tpt;
	TMessageProc msgProc;
	void *msgProcContext;
	Uint32 tranIDCounter;
	STranTask *firstTask;
	void *userRef;
	Uint32 rcvLeft;
	Uint32 rcvOffset;
	THdl rcvBuffer;
	STranHdr rcvHeader;
	TTimer estabTimer;
	Uint32 defaultTimeout;		// milliseconds
	Uint32 protocolTag, remoteProtocolTag;
	Uint16 versionTag, remoteVersionTag;
	Uint16 isEstablished	: 1;
	Uint16 estabTimedOut	: 1;
	Uint16 waitEstabReply	: 1;
	Uint16 isDead			: 1;
};

struct STransactSession {
	STransact *trn;
	TTimer tm;
	Uint32 timeout;		// milliseconds
	STranTask *sndTask;
	STranTask *rcvTask;
	Uint16 hasTimedOut		: 1;
};

#define TRN		((STransact *)inTrn)
#define TSN		((STransactSession *)inRef)

static Uint32 _TNNewTranID(TTransact inTrn);
static void _TNTimeoutHandler(void *inContext, void *inObject, Uint32 inMsg, const void *inData, Uint32 inDataSize);
static void _TNEstabTimeoutHandler(void *inContext, void *inObject, Uint32 inMsg, const void *inData, Uint32 inDataSize);
static STranTask *_TNNewTask(TTransact inTrn);
static void _TNKillTask(TTransact inTrn, STranTask *inTask);
static STranTask *_TNGetTask(TTransact inTrn, Uint16 inTaskType, Uint32 inTranID);
static STranTask *_TNGetInTask(TTransact inTrn, Uint16 inType, Uint16 inOptions);
static void _TNSendTran(TTransact inTrn, Uint32 inID, bool inReply, Uint16 inType, Uint32 inError, const void *inData, Uint32 inDataSize, Uint32 inTotalSize);
static void _TNProcessIncomingData(TTransact inTrn);
static void _TNMessageHandler(void *inContext, void *inObject, Uint32 inMsg, const void *inData, Uint32 inDataSize);

static Uint32 _gEstabilishTime = 20000;		// 20 seconds
static Uint32 _gDefaultTimeout = 200000;	// 200 seconds
Uint32 _gTransactMaxReceiveSize = 2097152;	// 2 MB

/* -------------------------------------------------------------------------- */

TTransact UTransact::New(Uint32 inProtocol, bool inRegular, Uint32 inOptions)
{
	STransact *trn = (STransact *)UMemory::NewClear(sizeof(STransact));
	trn->tpt = nil;
	
	try
	{
		trn->tpt = UTransport::New(inProtocol, inRegular, inOptions);
		UTransport::SetMessageHandler(trn->tpt, _TNMessageHandler, trn);
		trn->defaultTimeout = _gDefaultTimeout;
	}
	catch(...)
	{
		UTransport::Dispose(trn->tpt);
		UMemory::Dispose((TPtr)trn);
		throw;
	}
	
	return (TTransact)trn;
}

void UTransact::Dispose(TTransact inTrn)
{
	STranTask *p, *np;
	
	if (inTrn)
	{
		if (TRN->msgProc)
			UApplication::FlushMessages(TRN->msgProc, TRN->msgProcContext, inTrn);

		p = TRN->firstTask;
		while (p)
		{
			np = p->next;
			
			UMemory::Dispose(p->dataHdl);
			UMemory::Dispose((TPtr)p);
			
			p = np;
		}
		
		UMemory::Dispose(TRN->rcvBuffer);
		UTimer::Dispose(TRN->estabTimer);
		
		UTransport::Dispose(TRN->tpt);
		UMemory::Dispose((TPtr)inTrn);
	}
}

void UTransact::SetRef(TTransact inTrn, void *inRef)
{
	Require(inTrn);
	TRN->userRef = inRef;
}

void *UTransact::GetRef(TTransact inTrn)
{
	Require(inTrn);
	return TRN->userRef;
}

void UTransact::SetTransport(TTransact inTrn, TTransport inTpt)
{
	Require(inTrn);
	TRN->tpt = inTpt;
}

TTransport UTransact::GetTransport(TTransact inTrn)
{
	Require(inTrn);
	return TRN->tpt;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransact::SetMessageHandler(TTransact inTrn, TMessageProc inProc, void *inContext)
{
	Require(inTrn);
	TRN->msgProc = inProc;
	TRN->msgProcContext = inContext;
}

void UTransact::PostMessage(TTransact inTrn, Uint32 inMsg, const void *inData, Uint32 inDataSize, Int16 inPriority)
{
	Require(inTrn);
	if (TRN->msgProc)
		UApplication::PostMessage(inMsg, inData, inDataSize, inPriority, TRN->msgProc, TRN->msgProcContext, inTrn);
}

void UTransact::ReplaceMessage(TTransact inTrn, Uint32 inMsg, const void *inData, Uint32 inDataSize, Int16 inPriority)
{
	Require(inTrn);
	if (TRN->msgProc)
		UApplication::ReplaceMessage(inMsg, inData, inDataSize, inPriority, TRN->msgProc, TRN->msgProcContext, inTrn);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransact::SetIPAddress(TTransact inTrn, SIPAddress inIPAddress)
{
	Require(inTrn);
	UTransport::SetIPAddress(TRN->tpt, inIPAddress);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

bool UTransact::IsConnected(TTransact inTrn)
{
	Require(inTrn);
	return UTransport::IsConnected(TRN->tpt);
}

void UTransact::StartConnect(TTransact inTrn, const void *inAddr, Uint32 inAddrSize, Uint32 inMaxSecs)
{
	UTransport::StartConnect(TRN->tpt, inAddr, inAddrSize, inMaxSecs);
	TRN->waitEstabReply = false;
	TRN->estabTimedOut = false;
}

Uint16 UTransact::GetConnectStatus(TTransact inTrn)
{
	Require(inTrn);
	TTransport tpt = TRN->tpt;

	if (TRN->isEstablished)
		return kConnectionEstablished;
	
	if (TRN->waitEstabReply)
	{
#pragma options align=packed
		struct {
			Uint32 protocol;
			Uint32 result;
		} rcvData;
#pragma options align=reset
				
		if (!UTransport::IsConnected(tpt))
			Fail(errorType_Transport, transError_ConnectionClosed);
		
		if (UTransport::IsReadyToReceive(tpt, sizeof(rcvData)) && UTransport::Receive(tpt, &rcvData, sizeof(rcvData)) != 0)
		{
			UTimer::Dispose(TRN->estabTimer);
			TRN->estabTimer = nil;


			if (FB(rcvData.protocol) != 0x54525450){	// 'TRTP'
				
				if (FB(rcvData.protocol) != 0x4E49434B){	// 'NICK'
				Fail(errorType_Misc, error_FormatUnknown);
				}
				}
			
			if (rcvData.result != 0)
				Fail(errorType_Misc, error_VersionUnknown);
			
			TRN->isEstablished = true;
			
			return kConnectionEstablished;
		}
		
		if (TRN->estabTimedOut)
		{
			UTimer::Dispose(TRN->estabTimer);
			TRN->estabTimer = nil;
			Fail(errorType_Transport, transError_DataTimedOut);
		}
		
		return kEstablishingConnection;
	}
	else if (UTransport::GetConnectStatus(tpt) == kConnectionEstablished)
	{
#pragma options align=packed
		struct {
			Uint32 protocol;
			Uint32 subProtocol;
			Uint16 version;
			Uint16 subVersion;
		} sndData = { TB((Uint32)0x54525450), TB(TRN->protocolTag), TB((Uint16)1), TB(TRN->versionTag) };
#pragma options align=reset
		
		TRN->estabTimer = UTimer::New(_TNEstabTimeoutHandler, TRN);
		
		UTransport::Send(tpt, &sndData, sizeof(sndData));

		UTimer::Start(TRN->estabTimer, _gEstabilishTime);
		
		TRN->waitEstabReply = true;
		
		return kEstablishingConnection;
	}
	
	return kWaitingForConnection;
}

void UTransact::Disconnect(TTransact inTrn)
{
	if (inTrn)
	{
		UTransport::Disconnect(TRN->tpt);
		TRN->isEstablished = false;
	}
}

void UTransact::StartDisconnect(TTransact inTrn)
{
	Require(inTrn);
	UTransport::StartDisconnect(TRN->tpt);
}

bool UTransact::IsDisconnecting(TTransact inTrn)
{
	Require(inTrn);
	return UTransport::IsDisconnecting(TRN->tpt);
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransact::Listen(TTransact inTrn, const void *inAddr, Uint32 inAddrSize)
{
	Require(inTrn);
	UTransport::Listen(TRN->tpt, inAddr, inAddrSize);
}

// returns NIL if no connection
TTransact UTransact::Accept(TTransact inTrn, void *outAddr, Uint32 *ioAddrSize)
{
	Require(inTrn);
	TTransport tpt;
	STransact *trn = nil;
	
	tpt = UTransport::Accept(TRN->tpt, outAddr, ioAddrSize);
	if (tpt == nil) return nil;
	
	try
	{
		trn = (STransact *)UMemory::NewClear(sizeof(STransact));
		trn->tpt = tpt;
		UTransport::SetMessageHandler(trn->tpt, _TNMessageHandler, trn);
		trn->defaultTimeout = _gDefaultTimeout;
	}
	catch(...)
	{
		UTransport::Dispose(tpt);
		UMemory::Dispose((TPtr)trn);
		throw;
	}
	
	return (TTransact)trn;
}

bool UTransact::IsEstablished(TTransact inTrn)
{
	Require(inTrn);
	return TRN->isEstablished;
}

Uint16 UTransact::ReceiveEstablish(TTransact inTrn, Uint32& outProtocol, Uint16& outVersion)
{
	if (UTransport::GetConnectStatus(TRN->tpt) != kConnectionEstablished)
		return kWaitTransactionClient;
	
	if (TRN->isEstablished)
	{
		outProtocol = TRN->remoteProtocolTag;
		outVersion = TRN->remoteVersionTag;
		return kIsTransactionClient;
	}
	
#pragma options align=packed
	struct {
		Uint32 protocol;
		Uint32 subProtocol;
		Uint16 version;
		Uint16 subVersion;
	} rcvData;
#pragma options align=reset
	
	if (UTransport::IsReadyToReceive(TRN->tpt, sizeof(rcvData)) && UTransport::Receive(TRN->tpt, &rcvData, sizeof(rcvData)) != 0)
	{
	
		if (FB(rcvData.protocol) != 'TRTP'){
			
		if (FB(rcvData.protocol) != 'NICK'){
			return kNotTransactionClient;
			}
		}
		if (FB(rcvData.version) != 1)
		{
			RejectEstablish(inTrn, 1);
			return kIncompatibleTransactionClient;
		}
		
		outProtocol = TRN->remoteProtocolTag = rcvData.subProtocol;
		outVersion = TRN->remoteVersionTag = rcvData.subVersion;
		
		return kIsTransactionClient;
	}
	
	return kWaitTransactionClient;
}

void UTransact::AcceptEstablish(TTransact inTrn)
{
	Require(inTrn);
	if (!TRN->isEstablished)
	{
#pragma options align=packed
		struct {
			Uint32 protocol;
			Uint32 error;
		} sndData = { TB((Uint32)0x54525450), 0 };
#pragma options align=reset
		
		UTransport::Send(TRN->tpt, &sndData, sizeof(sndData));
		TRN->isEstablished = true;
	}
}

void UTransact::RejectEstablish(TTransact inTrn, Uint32 inReason)
{
	Require(inTrn && !TRN->isEstablished);
	
	if (inReason == 0) inReason = 1;
	
#pragma options align=packed
	struct {
		Uint32 protocol;
		Uint32 error;
	} sndData = { TB((Uint32)0x54525450), TB(inReason) };
#pragma options align=reset
	
	UTransport::Send(TRN->tpt, &sndData, sizeof(sndData));
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransact::SetVersion(TTransact inTrn, Uint32 inProtocol, Uint16 inVersion)
{
	Require(inTrn);
	TRN->protocolTag = inProtocol;
	TRN->versionTag = inVersion;
}

void UTransact::GetRemoteVersion(TTransact inTrn, Uint32& outProtocol, Uint16& outVersion)
{
	Require(inTrn);
	outProtocol = TRN->remoteProtocolTag;
	outVersion = TRN->remoteVersionTag;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// send one transaction and optionally receive one reply to it
TTransactSession UTransact::NewSendSession(TTransact inTrn, Uint16 inType)
{
	STransactSession *tsn = (STransactSession *)UMemory::NewClear(sizeof(STransactSession));
	STranTask *p;
	
	try
	{
		tsn->trn = (STransact *)inTrn;
		tsn->timeout = TRN->defaultTimeout;
		
		tsn->tm = UTimer::New(_TNTimeoutHandler, tsn);
		
		tsn->sndTask = p = _TNNewTask(inTrn);
		p->taskType = kOutTask;
		p->tranID = _TNNewTranID(inTrn);
		p->tranType = inType;
		
		tsn->rcvTask = p = _TNNewTask(inTrn);
		p->taskType = kInReplyTask;
		p->tranID = tsn->sndTask->tranID;	// reply will have the same ID as the tran we send
	}
	catch(...)
	{
		UTimer::Dispose(tsn->tm);
		_TNKillTask(inTrn, tsn->sndTask);
		_TNKillTask(inTrn, tsn->rcvTask);
		UMemory::Dispose((TPtr)tsn);
		throw;
	}
	
	return (TTransactSession)tsn;
}

// receive one transaction and optionally send one reply to it
// returns NIL if no transaction to receive
// if inOptions is set to kOnlyCompleteTransact, will only get transactions that have fully transfered
TTransactSession UTransact::NewReceiveSession(TTransact inTrn, Uint16 inType, Uint16 inOptions)
{
	Require(inTrn);
	STranTask *rcvTask, *p;
	STransactSession *tsn;
	
	_TNProcessIncomingData(inTrn);
	
	rcvTask = _TNGetInTask(inTrn, inType, inOptions);
	if (rcvTask == nil) return nil;
	
	tsn = (STransactSession *)UMemory::NewClear(sizeof(STransactSession));
	try
	{
		tsn->trn = (STransact *)inTrn;
		tsn->timeout = TRN->defaultTimeout;
		tsn->rcvTask = rcvTask;
		
		tsn->sndTask = p = _TNNewTask(inTrn);
		p->taskType = kOutReplyTask;
		p->tranID = rcvTask->tranID;	// reply has to have the same ID as the tran we're replying to
	}
	catch(...)
	{
		_TNKillTask(inTrn, tsn->sndTask);
		UMemory::Dispose((TPtr)tsn);
		throw;
	}
	
	// stop another NewReceiveSession() from grabbing the same transaction
	rcvTask->isBeingReceived = true;
	
	return (TTransactSession)tsn;
}

void UTransact::DisposeSession(TTransactSession inRef)
{
	if (inRef)
	{
		_TNKillTask((TTransact)TSN->trn, TSN->sndTask);
		_TNKillTask((TTransact)TSN->trn, TSN->rcvTask);

		UTimer::Dispose(TSN->tm);
		UMemory::Dispose((TPtr)inRef);
	}
}

void UTransact::SetSessionTimeout(TTransactSession inRef, Uint32 inMaxMillisecs)
{
	Require(inRef);
	TSN->timeout = inMaxMillisecs;
}

void UTransact::SetDefaultSessionTimeout(TTransact inTrn, Uint32 inMaxMillisecs)
{
	Require(inTrn);
	TRN->defaultTimeout = inMaxMillisecs;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

void UTransact::SetSessionSendType(TTransactSession inRef, Uint16 inType)
{
	Require(inRef);
	STranTask *p = TSN->sndTask;
	if (p) p->tranType = inType;
}

void UTransact::SetSessionSendError(TTransactSession inRef, Uint32 inError)
{
	Require(inRef);
	STranTask *p = TSN->sndTask;
	if (p) p->tranError = inError;
}

// sets the total size of the data
void UTransact::SetSessionSendSize(TTransactSession inRef, Uint32 inSize)
{
	Require(inRef);
	STranTask *p = TSN->sndTask;
	if (p) p->totalSize = inSize;
}

void UTransact::SendSessionData(TTransactSession inRef, const void *inData, Uint32 inDataSize, Uint16 /* inFlags */)
{
	Require(inRef);
	STranTask *p = TSN->sndTask;
	Require(p);

	if (p->totalSize == 0)
		p->totalSize = inDataSize;

	_TNSendTran((TTransact)TSN->trn, p->tranID, p->taskType == kOutReplyTask, p->tranType, p->tranError, inData, inDataSize, p->totalSize);
	p->transferedSize += inDataSize;

	if (TSN->tm && p->transferedSize >= p->totalSize)		// if finished sending
	{
		// start timer for timeout on reply
		UTimer::Start(TSN->tm, TSN->timeout, kOnceTimer);
	}
}

void UTransact::SendSessionData(TTransactSession inRef, TFieldData inData)
{
	THdl h = inData->GetDataHandle();
	
	void *p;
	StHandleLocker locker(h, p);
	
	SendSessionData(inRef, p, UMemory::GetSize(h));
}

/* -------------------------------------------------------------------------- */
#pragma mark -

bool UTransact::IsSessionReceiveComplete(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	Require(p);
	
	_TNProcessIncomingData((TTransact)TSN->trn);
	
	if (TSN->hasTimedOut)
		Fail(errorType_Transport, transError_DataTimedOut);

	return (p->hasReceivedData && p->transferedSize >= p->totalSize);
}

Uint16 UTransact::GetSessionReceiveType(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	if (p) return p->tranType;
	return 0;
}

Uint32 UTransact::GetSessionReceiveError(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	if (p) return p->tranError;
	return 0;
}

// returns the total size of the transaction - not how much has currently transfered
Uint32 UTransact::GetSessionReceiveSize(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	if (p) return p->totalSize;
	return 0;
}

Uint32 UTransact::GetSessionReceiveTransferedSize(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	if (p) return p->transferedSize;
	return 0;
}

Uint32 UTransact::ReceiveSessionData(TTransactSession inRef, void *outData, Uint32 inMaxSize)
{
	Require(inRef);
	Uint32 s, hs;
	STranTask *p = TSN->rcvTask;
	THdl h;
	
	if (p)
	{
		_TNProcessIncomingData((TTransact)TSN->trn);
		
		h = p->dataHdl;
		if (h)
		{
			s = hs = UMemory::GetSize(h);
			if (s > inMaxSize) s = inMaxSize;
			
			{
				void *tempp;
				StHandleLocker locker(h, tempp);
				UMemory::Copy(outData, tempp, s);
			}
			
			if (s == hs)
			{
				UMemory::Dispose(h);
				p->dataHdl = nil;
			}
			else
				UMemory::Remove(h, 0, s);
			
			// reset timeout
			if (TSN->tm)
			{
				UTimer::Start(TSN->tm, TSN->timeout, kOnceTimer);
				TSN->hasTimedOut = false;
			}
			
			return s;
		}
	}
	
	if (TSN->hasTimedOut)
		Fail(errorType_Transport, transError_DataTimedOut);
	
	return 0;
}

void UTransact::ReceiveSessionData(TTransactSession inRef, TFieldData outData)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	
	_TNProcessIncomingData((TTransact)TSN->trn);
	
	if (p && p->dataHdl)
	{
		outData->SetDataHandle(p->dataHdl);
		p->dataHdl = nil;
	}
	else
	{
		outData->DeleteAllFields();
	}
}

// returns NIL if no data
THdl UTransact::ReceiveSessionDataHdl(TTransactSession inRef)
{
	Require(inRef);
	STranTask *p = TSN->rcvTask;
	if (p)
	{
		_TNProcessIncomingData((TTransact)TSN->trn);
		THdl h = p->dataHdl;
		p->dataHdl = nil;
		
		// reset timeout
		if (TSN->tm)
		{
			UTimer::Start(TSN->tm, TSN->timeout, kOnceTimer);
			TSN->hasTimedOut = false;
		}
		return h;
	}
	
	if (TSN->hasTimedOut)
		Fail(errorType_Transport, transError_DataTimedOut);
	return nil;
}

/* -------------------------------------------------------------------------- */
#pragma mark -

// send transaction and ignore any reply
void UTransact::SendTransaction(TTransact inTrn, Uint16 inType, const void *inData, Uint32 inDataSize)
{
	Require(inTrn);
	_TNSendTran(inTrn, _TNNewTranID(inTrn), false, inType, 0, inData, inDataSize, inDataSize);
}

void UTransact::SendTransaction(TTransact inTrn, Uint16 inType, TFieldData inData)
{
	THdl h = inData->GetDataHandle();
	
	void *p;
	StHandleLocker locker(h, p);
	
	SendTransaction(inTrn, inType, p, UMemory::GetSize(h));
}

/* -------------------------------------------------------------------------- */
#pragma mark -

static Uint32 _TNNewTranID(TTransact inTrn)
{
	TRN->tranIDCounter++;
	if (TRN->tranIDCounter == 0) TRN->tranIDCounter = 1;
	return TRN->tranIDCounter;
}

static void _TNTimeoutHandler(void *inContext, void */* inObject */, Uint32 inMsg, const void */* inData */, Uint32 /* inDataSize */)
{
	if (inMsg == msg_Timer)
	{
		((STransactSession *)inContext)->hasTimedOut = true;
		((STransactSession *)inContext)->trn->tpt->PostMessage(msg_DataTimeOut);
	}
}

static void _TNEstabTimeoutHandler(void *inContext, void */* inObject */, Uint32 /* inMsg */, const void */* inData */, Uint32 /* inDataSize */)
{
	((STransact *)inContext)->estabTimedOut = true;
	((STransact *)inContext)->tpt->PostMessage(msg_DataTimeOut);
}

static STranTask *_TNNewTask(TTransact inTrn)
{
	STranTask *tsk = (STranTask *)UMemory::NewClear(sizeof(STranTask));
	
	//p->next = TRN->firstTask;
	//TRN->firstTask = p;
	
	// queue task so that things happen in order
	STranTask *p = TRN->firstTask;
	if (p)
	{
		for(;;)
		{
			if (p->next == nil) break;
			p = p->next;
		}
		
		p->next = tsk;
	}
	else
	{
		TRN->firstTask = tsk;
	}

	return tsk;
}

static void _TNKillTask(TTransact inTrn, STranTask *inTask)
{
	if (inTask == nil) return;
	
	STranTask *tm = TRN->firstTask;
	STranTask *ptm = nil;
	while (tm)
	{
		if (tm == inTask)
		{
			if (ptm)
				ptm->next = tm->next;
			else
				TRN->firstTask = tm->next;
			break;
		}
		ptm = tm;
		tm = tm->next;
	}
	
	UMemory::Dispose(inTask->dataHdl);
	UMemory::Dispose((TPtr)inTask);
}

static STranTask *_TNGetTask(TTransact inTrn, Uint16 inTaskType, Uint32 inTranID)
{
	STranTask *p = TRN->firstTask;
	
	while (p)
	{
		if (p->taskType == inTaskType && p->tranID == inTranID)
			return p;
		p = p->next;
	}
	
	return nil;
}

static STranTask *_TNGetInTask(TTransact inTrn, Uint16 inType, Uint16 inOptions)
{
	STranTask *p = TRN->firstTask;
	
	if (!TRN->isEstablished) return nil;
	
	if (inType == kAnyTransactType)
	{
		// looking for any type
		if (inOptions == kOnlyCompleteTransact)
		{
			// looking for any type and must be complete
			while (p)
			{
				if (p->taskType == kInTask && !p->isBeingReceived && p->transferedSize >= p->totalSize)
					return p;
				p = p->next;
			}
		}
		else
		{
			// looking for any type and doesn't have to be complete
			while (p)
			{
				if (p->taskType == kInTask && !p->isBeingReceived)
					return p;
				p = p->next;
			}
		}
	}
	else
	{
		// looking for a specific type
		if (inOptions == kOnlyCompleteTransact)
		{
			// looking for a specific type and must be complete
			while (p)
			{
				if (p->taskType == kInTask && p->tranType == inType && !p->isBeingReceived && p->transferedSize >= p->totalSize)
					return p;
				p = p->next;
			}
		}
		else
		{
			// looking for a specific type and doesn't have to be complete
			while (p)
			{
				if (p->taskType == kInTask && p->tranType == inType && !p->isBeingReceived)
					return p;
				p = p->next;
			}
		}
	}

	return nil;
}

static void _TNSendTran(TTransact inTrn, Uint32 inID, bool inReply, Uint16 inType, Uint32 inError, const void *inData, Uint32 inDataSize, Uint32 inTotalSize)
{
	TTransport tpt = TRN->tpt;
	STranHdr hdr;
	Uint8 rand;
	
	hdr.flag = 0;
	hdr.isReply = (inReply != 0);
	hdr.type = TB(inType);
	hdr.id = TB(inID);
	hdr.error = TB(inError);
	hdr.totalSize = TB(inTotalSize);
	hdr.dataSize = TB(inDataSize);


	if(gCrypt)
		{
		HLRand::GetBytes((DataBuffer){&rand,1});
		rand >>= 4;
		if(rand == 2 || rand == 7 || rand == 13)
			{
			HLRand::GetBytes((DataBuffer){&rand,1});
			rand >>= 2;
			if (!rand)
				{
				HLRand::GetBytes((DataBuffer){&rand,1});
				rand = (rand >> 3) + 1;
				}
			}
		else rand=0;
		
		hdr.flag=rand;	
		gCrypt->Encode((DataBuffer){(Uint8*)&hdr, sizeof(hdr)});
		}


	if (inDataSize == 0)
	{
		UTransport::Send(tpt, &hdr, sizeof(hdr));
	}
	else
	{
		Uint8 *buf = (Uint8 *)UTransport::NewBuffer(sizeof(hdr) + inDataSize);
		try
		{
			UMemory::Copy(buf, &hdr, sizeof(hdr));
			UMemory::Copy(buf+sizeof(hdr), inData, inDataSize);
			
			if(gCrypt)
				{
				if(rand)
					{
					gCrypt->Encode((DataBuffer){(Uint8*)buf+sizeof(hdr),2});
					gCrypt->PermEncodeKey(rand);
					gCrypt->Encode((DataBuffer){(Uint8*)buf+sizeof(hdr)+2,inDataSize-2});
					}
				else
					gCrypt->Encode((DataBuffer){(Uint8*)buf+sizeof(hdr), inDataSize});
				}	
			
			UTransport::SendBuffer(tpt, buf);
		}
		catch(...)
		{
			UTransport::DisposeBuffer(buf);
			throw;
		}
	}
}

static void _TNProcessIncomingData(TTransact inTrn)
{
	TTransport tpt;
	Uint32 s;
	STranTask *p;
	
	if (!TRN->isEstablished || TRN->isDead) return;

	tpt = TRN->tpt;
	Uint32& rcvLeft = TRN->rcvLeft;
	Uint32& rcvOffset = TRN->rcvOffset;
	THdl& rcvBuffer = TRN->rcvBuffer;
	STranHdr& rcvHeader = TRN->rcvHeader;
	
	for(;;)
	{
		if (rcvLeft == 0)		// if we're waiting for a new transaction
		{
			// ignore data until we have the full transaction header
			if (!UTransport::IsReadyToReceive(tpt, sizeof(STranHdr)))
				break;

			// receive transaction header
			if (UTransport::Receive(tpt, &rcvHeader, sizeof(STranHdr)) == 0)
				break;
			
			if(gCrypt)
				{
				gCrypt->Decode((DataBuffer){(Uint8*)&rcvHeader, sizeof(STranHdr)});
				}
				
				
		#if CONVERT_INTS
			rcvHeader.flag = FB(rcvHeader.flag);
			rcvHeader.type = FB(rcvHeader.type);
			rcvHeader.id = FB(rcvHeader.id);
			rcvHeader.error = FB(rcvHeader.error);
			rcvHeader.totalSize = FB(rcvHeader.totalSize);
			rcvHeader.dataSize = FB(rcvHeader.dataSize);
		#endif
				
				
			// allow max _gTransactMaxReceiveSize for totalSize and dataSize
			if (!rcvHeader.totalSize || rcvHeader.totalSize > _gTransactMaxReceiveSize || !rcvHeader.dataSize || rcvHeader.dataSize > _gTransactMaxReceiveSize)
			{
				// kill the connection
				TRN->isDead = true;
				tpt->Disconnect();
				break;
			}
			
			// allocate buffer to store data for this transaction
			try
			{
				if (rcvBuffer)
					UMemory::Reallocate(rcvBuffer, rcvHeader.dataSize);
				else
					rcvBuffer = UMemory::NewHandle(rcvHeader.dataSize);
			}
			catch(...)
			{
				// not enough memory, kill the connection so we don't cause never-ending errors
				TRN->isDead = true;
				tpt->Disconnect();
				throw;
			}
			
			// prepare to receive transaction data
			rcvLeft = rcvHeader.dataSize;
			rcvOffset = 0;
		}
		else					// we're receiveing a transaction
		{
			void *rcvBufferP;
			StHandleLocker locker(rcvBuffer, rcvBufferP);
			s = UTransport::Receive(tpt, BPTR(rcvBufferP) + rcvOffset, rcvLeft);
			if (s == 0) break;

			rcvOffset += s;
			rcvLeft -= s;
			
			if (rcvLeft != 0)
			{
				// update transfered/total sizes
				if (rcvHeader.isReply)
				{
					p = _TNGetTask(inTrn, kInReplyTask, rcvHeader.id);
					if (p)
					{
						p->transferedSize += s;
						p->totalSize = rcvHeader.totalSize;
					}
				}
			}
			else	// finished receiving transaction
			{				
				p = _TNGetTask(inTrn, rcvHeader.isReply ? kInReplyTask : kInTask, rcvHeader.id);
				
				s = UMemory::GetSize(rcvBuffer);
				
				if(gCrypt)
				{ //decode rest of buffer
				if(rcvHeader.flag)
					{					
					if(s>=2)
						gCrypt->Decode((DataBuffer){(Uint8*)rcvBufferP,2}); //decode 1st 2 bytes of 'data' with old key. shxd considers these 2 bytes as part of the header

					gCrypt->PermDecodeKey(rcvHeader.flag); //permute the key
					
					if(s>2)
						gCrypt->Decode((DataBuffer){(Uint8*)rcvBufferP+2, s-2}); //and decrypt the rest with the new key
					}
				else
					gCrypt->Decode((DataBuffer){(Uint8*)rcvBufferP,s});
				}
				
				if (p)
				{
					p->tranError = rcvHeader.error;
					p->tranType = rcvHeader.type;
					p->totalSize = rcvHeader.totalSize;
					p->hasReceivedData = true;
					
					if (p->dataHdl == nil)
					{
						p->dataHdl = rcvBuffer;
						rcvBuffer = nil;
					}
					else
					{
						UMemory::Append(p->dataHdl, rcvBufferP, s);
					}
					
					p->transferedSize += s;
				}
				else if (!rcvHeader.isReply)
				{
					p = _TNNewTask(inTrn);
					
					p->taskType = kInTask;
					p->tranType = rcvHeader.type;
					p->tranID = rcvHeader.id;
					p->totalSize = rcvHeader.totalSize;
					p->transferedSize = s;

					p->dataHdl = rcvBuffer;
					rcvBuffer = nil;
				}
			}
		}
	}
}

// gets messages from UTransport
static void _TNMessageHandler(void *inContext, void */* inObject */, Uint32 inMsg, const void */* inData */, Uint32 /* inDataSize */)
{
	if (inMsg == msg_DataArrived)
		_TNProcessIncomingData((TTransact)inContext);
	
	UTransact::PostMessage((TTransact)inContext, inMsg);
}

