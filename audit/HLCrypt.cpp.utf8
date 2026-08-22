#include "HLCrypt.h"
#include "ANSI.h"


HLCrypt::~HLCrypt()
{
if (mCipher) delete mCipher;
if (mHash) delete mHash;
if (mSessionKey.data) delete [] mSessionKey.data;
if (mEncodeKey.data) delete [] mEncodeKey.data;
if (mDecodeKey.data) delete [] mDecodeKey.data;
}


/* Init takes ownership of cipher and hash ONLY */
void
HLCrypt::Init(HLCipher *cipher, HLHash *hash, DataBuffer sessionKey, DataBuffer password, bool isClient)
{
UInt8 *temp1, *temp2;

mCipher=cipher;
mHash=hash;
mMacLen=mHash->GetMacLen();

mSessionKey.len=sessionKey.len;
mSessionKey.data=new UInt8 [mSessionKey.len];
memcpy(mSessionKey.data, sessionKey.data, mSessionKey.len);

mEncodeKey.len=mMacLen;
mDecodeKey.len=mMacLen;

temp1=new UInt8 [mMacLen];
temp2=new UInt8 [mMacLen];

mHash->HMAC_XXX(temp1, password, sessionKey);
mHash->HMAC_XXX(temp1, password, (DataBuffer){temp1, mMacLen});

mHash->HMAC_XXX(temp2, password, (DataBuffer){temp1, mMacLen});

if (isClient)
	{
	mEncodeKey.data=temp2;
	mDecodeKey.data=temp1;
	}
else
	{
	mEncodeKey.data=temp1;
	mDecodeKey.data=temp2;
	}

mCipher->Init();
mCipher->SetEncodeKey(mEncodeKey);
mCipher->SetDecodeKey(mDecodeKey);
}

void
HLCrypt::PermEncodeKey(UInt32 n)
{
while (n--)
	{
	mHash->HMAC_XXX(mEncodeKey.data, mEncodeKey, mSessionKey);
	}
mCipher->SetEncodeKey(mEncodeKey);
}

void
HLCrypt::PermDecodeKey(UInt32 n)
{
while (n--)
	{
	mHash->HMAC_XXX(mDecodeKey.data, mDecodeKey, mSessionKey);
	}
mCipher->SetDecodeKey(mDecodeKey);
}


