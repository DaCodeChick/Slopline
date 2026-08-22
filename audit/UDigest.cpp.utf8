/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */

#include "UDigest.h"

static void _Digest_WriteData(void*& ioData, Uint32& ioDataOffset, Uint32& ioDataSize, const Uint8 *inNewData, Uint32 inNewDataSize);
static void _Digest_AdjustData(void*& ioData, Uint32& ioDataSize, Uint32 inDataOffset);


// Encoder in Base64 format
// Decoded block 87654321 87654321 87654321 -> 3 bytes of 8 bits 
// | byte 1 | byte 2 | byte 3 | 
// Encoded block 876543 218765 432187 654321 -> 4 bytes of 6 bits 
// | byte 1 | byte 2 | byte 3 | byte 4 | 
void *UDigest::Base64_Encode(const void *inData, Uint32 inDataSize, Uint32& outDataSize)
{ 
	outDataSize = 0;
    if (!inData || !inDataSize)
       return nil;

   	void *outData = nil;
	Uint32 nDataSize = 0;

	Int8 bufEncodingTable[64] = {
	    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
		'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
    	'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
    	'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
	};

	Uint32 nOffset = 0;
	Uint8 bufDataIn[3], bufDataOut[4];
	Uint8 ch;
			
	while (nOffset < inDataSize) 
	{
		for (Uint8 i = 0; i < 3; i++) 
		{ 
			Uint32 nDepl = nOffset + i;
			
			if (nDepl < inDataSize)
				bufDataIn[i] = *((Uint8 *)inData + nDepl);
			else
				bufDataIn[i] = 0;
		} 
			
		bufDataOut[0] = (bufDataIn[0] & 0xFC) >> 2;
    	bufDataOut[1] = ((bufDataIn[0] & 0x03) << 4) | ((bufDataIn[1] & 0xF0) >> 4);
    	bufDataOut[2] = ((bufDataIn[1] & 0x0F) << 2) | ((bufDataIn[2] & 0xC0) >> 6);
    	bufDataOut[3] = bufDataIn[2] & 0x3F;
							
		Uint8 nCopySize;
		switch (inDataSize - nOffset) 
		{
			case 1: 
				nCopySize = 2; 
				break;
			
			case 2: 
				nCopySize = 3; 
				break;
				
			default:
				nCopySize = 4;
		};

		for (Uint8 i = 0; i < nCopySize; i++)
		{
			ch = bufEncodingTable[bufDataOut[i]];
		   	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
		}
							
		for (Uint8 i = nCopySize; i < 4; i++)
		{
			ch = '=';
		   	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
		}
			
		nOffset += 3;
	}
	
	_Digest_AdjustData(outData, nDataSize, outDataSize);
	return outData;
} 

// Decoder in Base64 format
//	Encoded block 654321 654321 654321 654321 -> 4 bytes of 6 bits 
//	| byte 1 | byte 2 | byte 3 | byte 4 | 
//	Decoded block 65432165 43216543 21654321 -> 3 bytes of 8 bits 
//	| byte 1 | byte 2 | byte 3 | 
void *UDigest::Base64_Decode(const void *inData, Uint32 inDataSize, Uint32& outDataSize)
{
	outDataSize = 0;
    if (!inData || !inDataSize)
       return nil;

   	void *outData = nil;
	Uint32 nDataSize = 0;

	Uint32 nOffset = 0;
	Uint8 nBufOffset = 0;
	Uint8 bufDataIn[4], bufDataOut[3];
	bool bDataEnd = false;
    	
	while (nOffset < inDataSize) 
	{
		Uint8 ch = *((Uint8 *)inData + nOffset++);
		
		if ((ch >= 'A') && (ch <= 'Z'))
			ch = ch - 'A';
		else if ((ch >= 'a') && (ch <= 'z'))
			ch = ch - 'a' + 26;
		else if ((ch >= '0') && (ch <= '9'))
			ch = ch - '0' + 52;
		else if (ch == '+')
			ch = 62;
		else if (ch == '/')
			ch = 63;
		else if (ch == '=')
			bDataEnd = true;
		else
			continue; // ignore this char (can be '\n')
		
		Uint8 nCopySize = 3;
		if (bDataEnd) 
		{
			if (nBufOffset == 0)
				break;
					
			if (nBufOffset == 1 || nBufOffset == 2)
				nCopySize = 1;
			else
				nCopySize = 2;
				
			nBufOffset = 3;
		}
			
		bufDataIn[nBufOffset++] = ch;
			
		if (nBufOffset == 4) 
		{
			nBufOffset = 0;
				
			bufDataOut[0] = (bufDataIn[0] << 2) | ((bufDataIn[1] & 0x30) >> 4);
			bufDataOut[1] = ((bufDataIn[1] & 0x0F) << 4) | ((bufDataIn[2] & 0x3C) >> 2);
			bufDataOut[2] = ((bufDataIn[2] & 0x03) << 6) | (bufDataIn[3] & 0x3F);
			
		   	_Digest_WriteData(outData, nDataSize, outDataSize, bufDataOut, nCopySize);
		}
			
		if (bDataEnd)
			break;
	}
			
	_Digest_AdjustData(outData, nDataSize, outDataSize);
	return outData;
} 

#pragma mark -

#define UU_ENCODE(c) ((c) ? ((c) & 077) + ' ': '`')

// Encoder in UU format
void *UDigest::UU_Encode(const void *inData, Uint32 inDataSize, Uint32& outDataSize)
{
	outDataSize = 0;
    if (!inData || !inDataSize)
       return nil;

   	void *outData = nil;
	Uint32 nDataSize = 0;

    Uint8 ch;
    const Uint8 *pData = (Uint8 *)inData;

  	while(true)
    {
  		Int32 nEnc = pData - inData;
  		if (nEnc >= inDataSize)
  			break;
  		
  		nEnc = inDataSize - nEnc > 45 ? 45 : inDataSize - nEnc;
  		
        ch = UU_ENCODE(nEnc);
       	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
        
        for (; nEnc > 0; nEnc -= 3, pData += 3)
        {
	        if (pData - inData >= inDataSize)
    	    	break;        

        	ch = *pData >> 2;
          	ch = UU_ENCODE(ch);
           	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
          	
          	ch = ((*pData << 4) & 060) | ((pData[1] >> 4) & 017); 
          	ch = UU_ENCODE(ch);
           	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);

          	ch = ((pData[1] << 2) & 074) | ((pData[2] >> 6) & 03); 
          	ch = UU_ENCODE(ch);
          	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);

          	ch = pData[2] & 077;
          	ch = UU_ENCODE(ch);
           	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
        }
        
       	_Digest_WriteData(outData, nDataSize, outDataSize, (Uint8 *)"\r\n", 2);
	}  

   	ch = UU_ENCODE(0);
   	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);

	_Digest_AdjustData(outData, nDataSize, outDataSize);
	return outData;
}

#define UU_DECODE(c)  (((c) - ' ') & 077)

// Decoder in UU format
void *UDigest::UU_Decode(const void *inData, Uint32 inDataSize, Uint32& outDataSize)
{
	outDataSize = 0;
    if (!inData || !inDataSize)
       return nil;

   	void *outData = nil;
	Uint32 nDataSize = 0;

    Uint8 ch;
    const Uint8 *pData = (Uint8 *)inData;

 	while (true)
    {
      	Int32 nDec = UU_DECODE(*pData);
      	if (nDec <= 0)
        	break;
        
      	for (++pData; nDec > 0; pData += 4, nDec -= 3)
        {
	        if (pData - inData >= inDataSize)
    	    	break;        

        	if (nDec >= 3)
          	{
            	ch = UU_DECODE(pData[0]) << 2 | UU_DECODE(pData[1]) >> 4; 
             	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
              	ch = UU_DECODE(pData[1]) << 4 | UU_DECODE(pData[2]) >> 2; 
             	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
              	ch = UU_DECODE(pData[2]) << 6 | UU_DECODE(pData[3]);
             	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
          	}
          	else
          	{
            	if (nDec >= 1)
             	{
                	ch = UU_DECODE(pData[0]) << 2 | UU_DECODE(pData[1]) >> 4; 
	             	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
             	}
          
             	if (nDec >= 2)
              	{
                	ch = UU_DECODE(pData[1]) << 4 | UU_DECODE(pData[2]) >> 2; 
	             	_Digest_WriteData(outData, nDataSize, outDataSize, &ch, 1);
              	}
          	}
        }
        
        if (*pData != '\r' && *(pData + 1) != '\n')
        	break;
        	
        pData += 2;
        if (pData - inData >= inDataSize)
        	break;        
    }
	
	_Digest_AdjustData(outData, nDataSize, outDataSize);
	return outData;
}

#pragma mark -

// outDataSize = 16 bytes
void *UDigest::MD5_Encode(const void *inData, Uint32 inDataSize, Uint32& outDataSize)
{
	outDataSize = 0;
    if (!inData || !inDataSize)
       return nil;

   	_MD5 clHash;                   		    	// create a _MD5 object
   	clHash.Update((Uint8 *)inData, inDataSize); // process the string
   	
	_MD5Digest clDigest;
   	clHash.Report(clDigest);       				// extract digest

   	outDataSize = _MD5_DIGEST_LENGTH;
   	void *outData = UMemory::New(outDataSize);
			
	if (!outData)
		Fail(errorType_Memory, memError_NotEnough);

   	Uint8 *pData = (Uint8 *)outData;
   	for(Uint32 i = 0; i < outDataSize; i++)
    	pData[i] = clDigest[i];
   
   	return outData;
}

#pragma mark-

static void _Digest_WriteData(void*& ioData, Uint32& ioDataSize, Uint32& ioDataOffset, const Uint8 *inNewData, Uint32 inNewDataSize)
{
	if (ioDataOffset > ioDataSize || !inNewData || !inNewDataSize)
		return;
	
	if (ioDataOffset + inNewDataSize > ioDataSize)
	{
		ioDataSize += RoundUp8(inNewDataSize + 4096);
		
		if (!ioData)
			ioData = UMemory::New(ioDataSize);
		else
			ioData = UMemory::Reallocate((TPtr)ioData, ioDataSize);
			
		if (!ioData)
			Fail(errorType_Memory, memError_NotEnough);
	}

	ioDataOffset += UMemory::Copy((Uint8 *)ioData + ioDataOffset, inNewData, inNewDataSize);
}

static void _Digest_AdjustData(void*& ioData, Uint32& ioDataSize, Uint32 inDataOffset)
{
	if (!ioData || !ioDataSize || !inDataOffset || inDataOffset >= ioDataSize)
		return;
	
	ioDataSize = inDataOffset;
	ioData = UMemory::Reallocate((TPtr)ioData, ioDataSize);

	if (!ioData)
		Fail(errorType_Memory, memError_NotEnough);
}

#pragma mark -

// F, G, H and I are basic MD5 functions
inline Uint32 _F(Uint32 xx, Uint32 yy, Uint32 zz)	{  return ((xx & yy) | (~xx & zz));  		}
inline Uint32 _G(Uint32 xx, Uint32 yy, Uint32 zz) 	{  return ((xx & zz) | (yy & ~zz)); 		}
inline Uint32 _H(Uint32 xx, Uint32 yy, Uint32 zz)	{  return xx ^ yy ^ zz; 					}
inline Uint32 _I(Uint32 xx, Uint32 yy, Uint32 zz) 	{  return yy ^ (xx | ~zz); 					}

// ROTATE_LEFT rotates x left n bits
inline Uint32 _ROTATE_LEFT(Uint32 xx, Int32 nn)		{  return (xx << nn) | (xx >> (32 - nn)); 	}

// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4 (rotation is separate from addition to prevent recomputation)
inline void _FF(Uint32 &aa, Uint32 bb, Uint32 cc, Uint32 dd, Uint32 xx, Int32 ss, Uint32 ac)
{ 
   	aa += _F(bb, cc, dd) + xx + ac;
   	aa = _ROTATE_LEFT(aa, ss);
   	aa += bb;
}

inline void _GG(Uint32 &aa, Uint32 bb, Uint32 cc, Uint32 dd, Uint32 xx, Int32 ss, Uint32 ac)
{ 
   	aa += _G(bb, cc, dd) + xx + ac;
   	aa = _ROTATE_LEFT(aa, ss);
   	aa += bb;
}

inline void _HH(Uint32 &aa, Uint32 bb, Uint32 cc, Uint32 dd, Uint32 xx, Int32 ss, Uint32 ac)
{ 
   	aa += _H(bb, cc, dd) + xx + ac;
   	aa = _ROTATE_LEFT(aa, ss);
   	aa += bb;
}

inline void _II(Uint32 &aa, Uint32 bb, Uint32 cc, Uint32 dd, Uint32 xx, Int32 ss, Uint32 ac)
{ 
   	aa += _I(bb, cc, dd) + xx + ac;
   	aa = _ROTATE_LEFT(aa, ss);
   	aa += bb;
}

_MD5Digest::_MD5Digest()
{ 
   	Clear();
}

_MD5Digest::~_MD5Digest()
{
   	Clear();
}

void _MD5Digest::Clear()
{
   	UMemory::Clear(mData, sizeof(mData));
}

Uint8& _MD5Digest::operator[](Uint32 inIndex)
{
   	static Uint8 nVal = 0;
   
   	if (inIndex >= _MD5_DIGEST_LENGTH) 
   	{
    	nVal = 0;
      	return nVal;
   	}
   
   	return mData[inIndex];
}

Uint8 _MD5Digest::operator[](Uint32 inIndex) const
{
	if (inIndex >= _MD5_DIGEST_LENGTH)
      return 0;
   
   	return mData[inIndex];
}

bool operator==(const _MD5Digest &inLhs, const _MD5Digest &inRhs)
{
   	bool bResult = true;   // assume true to begin with
   
   	for (Uint32 i = 0; bResult && i < _MD5_DIGEST_LENGTH; i++) 
   		bResult = (inLhs.mData[i] == inRhs.mData[i]);
      
   	return bResult;
}

bool operator!=(const _MD5Digest &inLhs, const _MD5Digest &inRhs)
{
   	return !(inLhs == inRhs);
}

_MD5::_MD5()
{
	Init();  // set up the initial state values
}

_MD5::~_MD5()
{
  	Clear(); // wipe the class data clean
}

// load magic initialization constants 
void _MD5::Init()
{ 
   	mState[0] = 0x67452301L;  // these values are only used here
   	mState[1] = 0xefcdab89L;
   	mState[2] = 0x98badcfeL;
   	mState[3] = 0x10325476L;
   	mCount[0] = mCount[1] = 0;
   
   	UMemory::Clear(mBuffer, sizeof(mBuffer));
}

// zero the data memebers of this class
void _MD5::Clear()
{  
   	mState[0] = mState[1] = mState[2] = mState[3] = 0;
  	mCount[0] = mCount[1] = 0;
   
   	UMemory::Clear(mBuffer, sizeof(mBuffer));
}

// MD5 block update operation, continues an MD5 message-digest operation, processing another message block, and updating the context
void _MD5::Update(const Uint8 *inData, Uint32 inDataSize)
{  
	// compute number of bytes mod 64
   	Uint32 index = (mCount[0] >> 3) & 0x3F;

   	// update number of bits
   	if((mCount[0] += (inDataSize << 3)) < (inDataSize << 3)) 
   		mCount[1]++;
      
   	mCount[1] += inDataSize >> 29;
   	Uint32 partLen = 64 - index;

	// transform as many times as possible
   	Uint32 i = 0;
   	if(inDataSize >= partLen) 
   	{
      	UMemory::Copy(&mBuffer[index], inData, partLen);
      	Transform(mBuffer);

      	for(i = partLen; i + 63 < inDataSize; i += 64)
         	Transform(&inData[i]);
      
      	index = 0;
   }
   
   	// buffer remaining input
   	if (inDataSize - i)
      	UMemory::Copy(&mBuffer[index], &inData[i], inDataSize - i);
}

// MD5 finalization, ends an MD5 message-digest operation, writing the the message digest and zeroizing the context
void _MD5::Report(_MD5Digest &outDigest)
{ 
   	Uint8 bits[8];
	static Uint8 padding[64] = {
      	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

   	Encode(bits, mCount, 8);    // save number of bits

   	// pad out to 56 mod 64
   	Uint32 index  = (mCount[0] >> 3) & 0x3f;
  	Uint32 padLen = (index < 56) ? (56 - index) : (120 - index);
   
   	Update(padding, padLen);
   	Update(bits,    8);     	// append length (before padding)

   	Encode(outDigest);       	// Store state in digest
   	Clear();                    // zero sensitive information
}

// MD5 basic transformation, transforms state based on block
void _MD5::Transform(const Uint8 inBlock[64])
{  
   	const Int8 S11 =  7;
	const Int8 S12 = 12;
   	const Int8 S13 = 17;
   	const Int8 S14 = 22;
   	const Int8 S21 =  5;
   	const Int8 S22 =  9;
   	const Int8 S23 = 14;
   	const Int8 S24 = 20;
   	const Int8 S31 =  4;
   	const Int8 S32 = 11;
   	const Int8 S33 = 16;
   	const Int8 S34 = 23;
   	const Int8 S41 =  6;
   	const Int8 S42 = 10;
   	const Int8 S43 = 15;
   	const Int8 S44 = 21;

   	Uint32 aa = mState[0], bb = mState[1], cc = mState[2], dd = mState[3];
   	Uint32 xx[16];

   	Decode(xx, inBlock, 64);

   	// round 1
   	_FF(aa, bb, cc, dd, xx[ 0], S11, 0xd76aa478L); // 1
   	_FF(dd, aa, bb, cc, xx[ 1], S12, 0xe8c7b756L); // 2 
   	_FF(cc, dd, aa, bb, xx[ 2], S13, 0x242070dbL); // 3 
   	_FF(bb, cc, dd, aa, xx[ 3], S14, 0xc1bdceeeL); // 4 
   	_FF(aa, bb, cc, dd, xx[ 4], S11, 0xf57c0fafL); // 5 
   	_FF(dd, aa, bb, cc, xx[ 5], S12, 0x4787c62aL); // 6 
   	_FF(cc, dd, aa, bb, xx[ 6], S13, 0xa8304613L); // 7 
   	_FF(bb, cc, dd, aa, xx[ 7], S14, 0xfd469501L); // 8 
   	_FF(aa, bb, cc, dd, xx[ 8], S11, 0x698098d8L); // 9 
   	_FF(dd, aa, bb, cc, xx[ 9], S12, 0x8b44f7afL); // 10
   	_FF(cc, dd, aa, bb, xx[10], S13, 0xffff5bb1L); // 11
   	_FF(bb, cc, dd, aa, xx[11], S14, 0x895cd7beL); // 12
   	_FF(aa, bb, cc, dd, xx[12], S11, 0x6b901122L); // 13
   	_FF(dd, aa, bb, cc, xx[13], S12, 0xfd987193L); // 14
   	_FF(cc, dd, aa, bb, xx[14], S13, 0xa679438eL); // 15
   	_FF(bb, cc, dd, aa, xx[15], S14, 0x49b40821L); // 16

   	// round 2
   	_GG(aa, bb, cc, dd, xx[ 1], S21, 0xf61e2562L); // 17
   	_GG(dd, aa, bb, cc, xx[ 6], S22, 0xc040b340L); // 18
   	_GG(cc, dd, aa, bb, xx[11], S23, 0x265e5a51L); // 19
   	_GG(bb, cc, dd, aa, xx[ 0], S24, 0xe9b6c7aaL); // 20
   	_GG(aa, bb, cc, dd, xx[ 5], S21, 0xd62f105dL); // 21
   	_GG(dd, aa, bb, cc, xx[10], S22, 0x02441453L); // 22
   	_GG(cc, dd, aa, bb, xx[15], S23, 0xd8a1e681L); // 23
   	_GG(bb, cc, dd, aa, xx[ 4], S24, 0xe7d3fbc8L); // 24
   	_GG(aa, bb, cc, dd, xx[ 9], S21, 0x21e1cde6L); // 25
   	_GG(dd, aa, bb, cc, xx[14], S22, 0xc33707d6L); // 26
   	_GG(cc, dd, aa, bb, xx[ 3], S23, 0xf4d50d87L); // 27
   	_GG(bb, cc, dd, aa, xx[ 8], S24, 0x455a14edL); // 28
   	_GG(aa, bb, cc, dd, xx[13], S21, 0xa9e3e905L); // 29
   	_GG(dd, aa, bb, cc, xx[ 2], S22, 0xfcefa3f8L); // 30
   	_GG(cc, dd, aa, bb, xx[ 7], S23, 0x676f02d9L); // 31
   	_GG(bb, cc, dd, aa, xx[12], S24, 0x8d2a4c8aL); // 32
 
   	// round 3
   	_HH(aa, bb, cc, dd, xx[ 5], S31, 0xfffa3942L); // 33
   	_HH(dd, aa, bb, cc, xx[ 8], S32, 0x8771f681L); // 34
   	_HH(cc, dd, aa, bb, xx[11], S33, 0x6d9d6122L); // 35
   	_HH(bb, cc, dd, aa, xx[14], S34, 0xfde5380cL); // 36
   	_HH(aa, bb, cc, dd, xx[ 1], S31, 0xa4beea44L); // 37
   	_HH(dd, aa, bb, cc, xx[ 4], S32, 0x4bdecfa9L); // 38
   	_HH(cc, dd, aa, bb, xx[ 7], S33, 0xf6bb4b60L); // 39
   	_HH(bb, cc, dd, aa, xx[10], S34, 0xbebfbc70L); // 40
   	_HH(aa, bb, cc, dd, xx[13], S31, 0x289b7ec6L); // 41
   	_HH(dd, aa, bb, cc, xx[ 0], S32, 0xeaa127faL); // 42
   	_HH(cc, dd, aa, bb, xx[ 3], S33, 0xd4ef3085L); // 43
   	_HH(bb, cc, dd, aa, xx[ 6], S34, 0x04881d05L); // 44
   	_HH(aa, bb, cc, dd, xx[ 9], S31, 0xd9d4d039L); // 45
   	_HH(dd, aa, bb, cc, xx[12], S32, 0xe6db99e5L); // 46
   	_HH(cc, dd, aa, bb, xx[15], S33, 0x1fa27cf8L); // 47
   	_HH(bb, cc, dd, aa, xx[ 2], S34, 0xc4ac5665L); // 48
 
   	// round 4
   	_II(aa, bb, cc, dd, xx[ 0], S41, 0xf4292244L); // 49
   	_II(dd, aa, bb, cc, xx[ 7], S42, 0x432aff97L); // 50
   	_II(cc, dd, aa, bb, xx[14], S43, 0xab9423a7L); // 51
   	_II(bb, cc, dd, aa, xx[ 5], S44, 0xfc93a039L); // 52
   	_II(aa, bb, cc, dd, xx[12], S41, 0x655b59c3L); // 53
   	_II(dd, aa, bb, cc, xx[ 3], S42, 0x8f0ccc92L); // 54
   	_II(cc, dd, aa, bb, xx[10], S43, 0xffeff47dL); // 55
   	_II(bb, cc, dd, aa, xx[ 1], S44, 0x85845dd1L); // 56
   	_II(aa, bb, cc, dd, xx[ 8], S41, 0x6fa87e4fL); // 57
   	_II(dd, aa, bb, cc, xx[15], S42, 0xfe2ce6e0L); // 58
   	_II(cc, dd, aa, bb, xx[ 6], S43, 0xa3014314L); // 59
   	_II(bb, cc, dd, aa, xx[13], S44, 0x4e0811a1L); // 60
   	_II(aa, bb, cc, dd, xx[ 4], S41, 0xf7537e82L); // 61
   	_II(dd, aa, bb, cc, xx[11], S42, 0xbd3af235L); // 62
   	_II(cc, dd, aa, bb, xx[ 2], S43, 0x2ad7d2bbL); // 63
   	_II(bb, cc, dd, aa, xx[ 9], S44, 0xeb86d391L); // 64
 
   	mState[0] += aa;
   	mState[1] += bb;
   	mState[2] += cc;
   	mState[3] += dd;

   	UMemory::Clear(xx, sizeof (xx));  	// clear sensitive information
}

// encodes inData into outData, assumes inDataSize is a multiple of 4
void _MD5::Encode(Uint8  *outData, const Uint32 *inData, Uint32 inDataSize)
{  
  	Uint32 i, j;
  	for (i = 0, j = 0; j < inDataSize; i++, j += 4) 
  	{
     	outData[j]   = (Uint8)( inData[i]        & 0xFF);
     	outData[j+1] = (Uint8)((inData[i] >>  8) & 0xFF);
     	outData[j+2] = (Uint8)((inData[i] >> 16) & 0xFF);
     	outData[j+3] = (Uint8)((inData[i] >> 24) & 0xFF);
  	}
}

// encodes mState into outDigest
void _MD5::Encode(_MD5Digest &outDigest)
{  
  	Uint32 i, j;
  	for(i = 0, j = 0; j < _MD5_DIGEST_LENGTH; i++, j += 4) 
  	{
     	outDigest.mData[j]     = (Uint8)( mState[i]        & 0xFF);
     	outDigest.mData[j + 1] = (Uint8)((mState[i] >>  8) & 0xFF);
     	outDigest.mData[j + 2] = (Uint8)((mState[i] >> 16) & 0xFF);
     	outDigest.mData[j + 3] = (Uint8)((mState[i] >> 24) & 0xFF);
  	}
}

// decodes inData into outData, assumes inDataSize is a multiple of 4
void _MD5::Decode(Uint32 *outData, const Uint8 *inData, Uint32 inDataSize)
{  
  	Uint32 i, j;
  	for(i = 0, j = 0; j < inDataSize; i++, j += 4 ) 
  	{
     	outData[i] = ((Uint32)inData[j]) 			 | 
                 	(((Uint32)inData[j + 1]) <<  8 ) |
                 	(((Uint32)inData[j + 2]) << 16 ) | 
                 	(((Uint32)inData[j + 3]) << 24 );
  	}
}
