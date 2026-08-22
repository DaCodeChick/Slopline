/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#pragma once
#include "typedefs.h"
#include "UTransport.h"
#include "UFieldData.h"


/*
 * Constants
 */

enum {
	kAnyTransactType		= 0,
	kOnlyCompleteTransact	= 1
};

enum {
	kWaitTransactionClient			= 0,	// client has not identified itself yet
	kIsTransactionClient			= 1,	// client supports transactions
	kNotTransactionClient			= 2,	// client does not do transactions
	kIncompatibleTransactionClient	= 3		// client version is too old or new
};

/*
 * Types
 */

typedef class TTransactObj *TTransact;
typedef class TTransactSessionObj *TTransactSession;

/*
 * Transact module
 */

class UTransact
{
	public:
		// new, dispose, properties
		static TTransact New(Uint32 inProtocol, bool inRegular = true, Uint32 inOptions = 0);
		static void Dispose(TTransact inTrn);
		static void SetRef(TTransact inTrn, void *inRef);
		static void *GetRef(TTransact inTrn);
		static void SetTransport(TTransact inTrn, TTransport inTpt);
		static TTransport GetTransport(TTransact inTrn);
		
		// messaging
		static void SetMessageHandler(TTransact inTrn, TMessageProc inProc, void *inContext = nil);
		static void PostMessage(TTransact inTrn, Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal);
		static void ReplaceMessage(TTransact inTrn, Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal);
		
		// addresses
		static void SetIPAddress(TTransact inTrn, SIPAddress inIPAddress);

		// connecting and disconnecting
		static bool IsConnected(TTransact inTrn);
		static void StartConnect(TTransact inTrn, const void *inAddr, Uint32 inAddrSize, Uint32 inMaxSecs = 0);
		static Uint16 GetConnectStatus(TTransact inTrn);
		static void Disconnect(TTransact inTrn);
		static void StartDisconnect(TTransact inTrn);
		static bool IsDisconnecting(TTransact inTrn);

		// remote computer requests connection
		static void Listen(TTransact inTrn, const void *inAddr, Uint32 inAddrSize);
		static TTransact Accept(TTransact inTrn, void *outAddr = nil, Uint32 *ioAddrSize = nil);
		static bool IsEstablished(TTransact inTrn);
		static Uint16 ReceiveEstablish(TTransact inTrn, Uint32& outProtocol, Uint16& outVersion);
		static void AcceptEstablish(TTransact inTrn);
		static void RejectEstablish(TTransact inTrn, Uint32 inReason = 1);
		
		// version control
		static void SetVersion(TTransact inTrn, Uint32 inProtocol, Uint16 inVersion);
		static void GetRemoteVersion(TTransact inTrn, Uint32& outProtocol, Uint16& outVersion);
		
		// send and receive sessions
		static TTransactSession NewSendSession(TTransact inTrn, Uint16 inType);												// send one transaction and optionally receive one reply to it
		static TTransactSession NewReceiveSession(TTransact inTrn, Uint16 inType = 0, Uint16 inOptions = 0);	// receive one transaction and optionally send one reply to it
		static void DisposeSession(TTransactSession inRef);
		static void SetSessionTimeout(TTransactSession inRef, Uint32 inMaxMillisecs);
		static void SetDefaultSessionTimeout(TTransact inTrn, Uint32 inMaxMillisecs);

		// session send functions
		static void SetSessionSendType(TTransactSession inRef, Uint16 inType);
		static void SetSessionSendError(TTransactSession inRef, Uint32 inError);
		static void SetSessionSendSize(TTransactSession inRef, Uint32 inSize);
		static void SendSessionData(TTransactSession inRef, const void *inData, Uint32 inDataSize, Uint16 inFlags = 0);
		static void SendSessionData(TTransactSession inRef, TFieldData inData);

		// session receive functions
		static bool IsSessionReceiveComplete(TTransactSession inRef);
		static Uint16 GetSessionReceiveType(TTransactSession inRef);
		static Uint32 GetSessionReceiveError(TTransactSession inRef);
		static Uint32 GetSessionReceiveSize(TTransactSession inRef);
		static Uint32 GetSessionReceiveTransferedSize(TTransactSession inRef);
		static Uint32 ReceiveSessionData(TTransactSession inRef, void *outData, Uint32 inMaxSize);
		static void ReceiveSessionData(TTransactSession inRef, TFieldData outData);
		static THdl ReceiveSessionDataHdl(TTransactSession inRef);
		
		// send transaction and don't wait for reply
		static void SendTransaction(TTransact inTrn, Uint16 inType, const void *inData, Uint32 inDataSize);
		static void SendTransaction(TTransact inTrn, Uint16 inType, TFieldData inData);
};

/*
 * Stack TTransact
 */

class StTransact
{
	public:
		StTransact()									{	mRef = nil;										}
		void New(Uint32 inProtocol, bool inRegular = true, Uint32 inOptions = 0) 	{	mRef = UTransact::New(inProtocol, inRegular, inOptions);	}
		~StTransact()									{	UTransact::Dispose(mRef);						}
		operator TTransact()							{	return mRef;									}
		TTransact operator->() const					{	return mRef;									}
		bool IsValid()									{	return mRef != nil;								}
		bool IsInvalid()								{	return mRef == nil;								}

	private:
		TTransact mRef;
};

/*
 * UTransact Object Interface
 */

class TTransactObj
{
	public:
		void SetRef(void *inRef)																			{	UTransact::SetRef(this, inRef);													}
		void *GetRef()																						{	return UTransact::GetRef(this);													}
		void SetTransport(TTransport inTpt)																	{	UTransact::SetTransport(this, inTpt);											}
		TTransport GetTransport()																			{	return UTransact::GetTransport(this);											}
		
		void SetMessageHandler(TMessageProc inProc, void *inContext = nil)									{	UTransact::SetMessageHandler(this, inProc, inContext);							}
		void PostMessage(Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal)			{	UTransact::PostMessage(this, inMsg, inData, inDataSize, inPriority);		}
		void ReplaceMessage(Uint32 inMsg, const void *inData = nil, Uint32 inDataSize = 0, Int16 inPriority = priority_Normal)		{	UTransact::ReplaceMessage(this, inMsg, inData, inDataSize, inPriority);		}

		void SetIPAddress(SIPAddress inIPAddress)															{	UTransact::SetIPAddress(this, inIPAddress);										}

		bool IsConnected()																					{	return UTransact::IsConnected(this);											}
		void StartConnect(const void *inAddr, Uint32 inAddrSize, Uint32 inMaxSecs = 0)						{	UTransact::StartConnect(this, inAddr, inAddrSize, inMaxSecs);					}
		Uint16 GetConnectStatus()																			{	return UTransact::GetConnectStatus(this);										}
		void Disconnect()																					{	UTransact::Disconnect(this);													}
		void StartDisconnect()																				{	UTransact::StartDisconnect(this);												}
		bool IsDisconnecting()																				{	return UTransact::IsDisconnecting(this);										}

		void Listen(const void *inAddr, Uint32 inAddrSize)													{	UTransact::Listen(this, inAddr, inAddrSize);									}
		TTransact Accept(void *outAddr = nil, Uint32 *ioAddrSize = nil)										{	return UTransact::Accept(this, outAddr, ioAddrSize);							}
		
		bool IsEstablished()																				{	return UTransact::IsEstablished(this);											}
		Uint16 ReceiveEstablish(Uint32& outProtocol, Uint16& outVersion)									{	return UTransact::ReceiveEstablish(this, outProtocol, outVersion);				}
		void AcceptEstablish()																				{	UTransact::AcceptEstablish(this);												}
		void RejectEstablish(Uint32 inReason = 1)															{	UTransact::RejectEstablish(this, inReason);										}

		void SetVersion(Uint32 inProtocol, Uint16 inVersion)												{	UTransact::SetVersion(this, inProtocol, inVersion);								}
		void GetRemoteVersion(Uint32& outProtocol, Uint16& outVersion)										{	UTransact::GetRemoteVersion(this, outProtocol, outVersion);						}
		
		TTransactSession NewSendSession(Uint16 inType)														{	return UTransact::NewSendSession(this, inType);									}
		TTransactSession NewReceiveSession(Uint16 inType = 0, Uint16 inOptions = 0)							{	return UTransact::NewReceiveSession(this, inType, inOptions);					}
		void SendTransaction(Uint16 inType, const void *inData, Uint32 inDataSize)							{	UTransact::SendTransaction(this, inType, inData, inDataSize);					}
		void SendTransaction(Uint16 inType, TFieldData inData)												{	UTransact::SendTransaction(this, inType, inData);								}
		void SetDefaultSessionTimeout(Uint32 inMaxMillisecs)												{	UTransact::SetDefaultSessionTimeout(this, inMaxMillisecs);						}

		void operator delete(void *p)																		{	UTransact::Dispose((TTransact)p);												}
	protected:
		TTransactObj() {}				// force creation via UTransact
};

class TTransactSessionObj
{
	public:
		void SetTimeout(Uint32 inMaxMillisecs)																{	UTransact::SetSessionTimeout(this, inMaxMillisecs);								}

		void SetSendType(Uint16 inType)																		{	UTransact::SetSessionSendType(this, inType);									}
		void SetSendError(Uint32 inError)																	{	UTransact::SetSessionSendError(this, inError);									}
		void SetSendSize(Uint32 inSize)																		{	UTransact::SetSessionSendSize(this, inSize);									}
		void SendData(const void *inData, Uint32 inDataSize, Uint16 inFlags = 0)							{	UTransact::SendSessionData(this, inData, inDataSize, inFlags);					}
		void SendData(TFieldData inData)																	{	UTransact::SendSessionData(this, inData);										}

		bool IsReceiveComplete()																			{	return UTransact::IsSessionReceiveComplete(this);								}
		Uint16 GetReceiveType()																				{	return UTransact::GetSessionReceiveType(this);									}
		Uint32 GetReceiveError()																			{	return UTransact::GetSessionReceiveError(this);									}
		Uint32 GetReceiveSize()																				{	return UTransact::GetSessionReceiveSize(this);									}
		Uint32 GetReceiveTransferedSize()																	{	return UTransact::GetSessionReceiveTransferedSize(this);						}
		Uint32 ReceiveData(void *outData, Uint32 inMaxSize)													{	return UTransact::ReceiveSessionData(this, outData, inMaxSize);					}
		void ReceiveData(TFieldData outData)																{	UTransact::ReceiveSessionData(this, outData);									}
		THdl ReceiveDataHdl()																				{	return UTransact::ReceiveSessionDataHdl(this);									}

		void operator delete(void *p)																		{	UTransact::DisposeSession((TTransactSession)p);									}
	protected:
		TTransactSessionObj() {}		// force creation via UTransact
};

