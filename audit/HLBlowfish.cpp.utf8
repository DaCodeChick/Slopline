#include "HLBlowfish.h"
#include "HLBlowfishData.h"

void
HLBlowfish::Init()
{
UInt32 i;

for (i=0;i<8;i++)
	{
	mEncodeIvec[i]=0;	
	mDecodeIvec[i]=0;
	}
mEncodeIvecOffset=0;	
mDecodeIvecOffset=0;
}


inline
UInt32
HLBlowfish::F(UInt32 in, bf_key *key)
{
UInt16 a;
UInt16 b;
UInt16 c;
UInt16 d;
UInt32 y;

//split input into a,b,c,d
d=in&0x00FF;
in>>=8;
c=in&0x00FF;
in>>=8;
b=in&0x00FF;
in>>=8;
a=in&0x00FF;

y=key->S[0][a]+key->S[1][b];
y=y^key->S[2][c];
y=y+key->S[3][d];

return y;
}


inline
void
HLBlowfish::Encrypt(UInt32 *L, UInt32 *R, bf_key *key)
{
UInt32  dataL;
UInt32  dataR;
UInt32  swap;
Int16 i;

dataL=*L;
dataR=*R;

//16 rounds
for (i=0;i<16;i++)
	{
	dataL=dataL^key->P[i];
	dataR=F(dataL,key)^dataR;

	//swap L/R
	swap=dataL;
	dataL=dataR;
	dataR=swap;
	}

//swap L/R
swap=dataL;
dataL=dataR;
dataR=swap;

dataR=dataR^key->P[16];
dataL=dataL^key->P[17];

*L=dataL;
*R=dataR;
}

inline
void
HLBlowfish::Decrypt(UInt32 *L, UInt32 *R, bf_key *key)
{
UInt32 dataL;
UInt32 dataR;
UInt32 swap;
Int16 i;

dataL=*L;
dataR=*R;

//16 rounds
for (i=17;i>1;i--)
	{
	dataL=dataL^key->P[i];
	dataR=F(dataL,key)^dataR;

	//swap L/R
	swap=dataL;
	dataL=dataR;
	dataR=swap;
	}

//swap L/R
swap=dataL;
dataL=dataR;
dataR=swap;

dataR=dataR^key->P[1];
dataL=dataL^key->P[0];

*L=dataL;
*R=dataR;
}


void
HLBlowfish::ExpandKey(DataBuffer inKey, bf_key *outKey)
{
Int32 i,j,k;
UInt32 temp,tempL,tempR;

//fill parray and s_box[0..3] with hex digits of PI
for (i=0;i<18;i++)
	outKey->P[i]=blowfish_parrayPI[i];
for (i=0;i<256;i++)
	outKey->S[0][i]=blowfish_sboxPI0[i];
for (i=0;i<256;i++)
	outKey->S[1][i]=blowfish_sboxPI1[i];
for (i=0;i<256;i++)
	outKey->S[2][i]=blowfish_sboxPI2[i];
for (i=0;i<256;i++)
	outKey->S[3][i]=blowfish_sboxPI3[i];

//XOR parray with key, wrappping around when we run out of keybytes
for (k=0,i=0;i<18;i++)
	{
	for(j=0,temp=0x00000000L;j<4;j++)
		{
		temp=(temp<<8)|inKey.data[k];
		k=(k+1)%inKey.len;
		}
	outKey->P[i]=outKey->P[i]^temp;
	}

tempL=0x00000000L;
tempR=0x00000000L;
for (i=0;i<18;i+=2)
	{
	Encrypt(&tempL,&tempR,outKey);
	outKey->P[i]=tempL;
	outKey->P[i+1]=tempR;
	}

//continue above process through sbox[0..3]
for(i=0;i<4;i++)
	{
	for(j=0;j<256;j+=2)
		{
		Encrypt(&tempL,&tempR,outKey);
		outKey->S[i][j]=tempL;
		outKey->S[i][j+1]=tempR;
		}
	}
}


void
HLBlowfish::OFB64(DataBuffer buffer, bf_key *key, UInt32 *ivec, Int32 *pn)
{
UInt32 ivl[2];
UInt8 *cp=(UInt8*)ivl;
Int32 n=*pn;

//TB == hotnl

ivl[0]=TB(ivec[0]);
ivl[1]=TB(ivec[1]);

while(buffer.len--)
	{
	if (n==0)
		{
		Encrypt(&ivec[0],&ivec[1],key);
		ivl[0]=TB(ivec[0]);
		ivl[1]=TB(ivec[1]);
		}
		
	*buffer.data++^=cp[n];
	n=(n+1)&0x07UL; //may be faster than %8
	}

*pn=n;
}


bool
HLBlowfish::SelfTest()
{
long i,keylen;
unsigned long tempL, tempR;

//64 bit keys
for(i=0;i<NUM_VARIABLE_KEY_TESTS;i++)
	{
	SetEncodeKey((DataBuffer){variable_key[i],8});
	tempL=plaintext_l[i];
	tempR=plaintext_r[i];

	Encrypt(&tempL,&tempR,&mEncodeKey);
	if(tempL!=ciphertext_l[i] || tempR!=ciphertext_r[i]) return false;

	Decrypt(&tempL,&tempR,&mEncodeKey);
	if(tempL!=plaintext_l[i] || tempR!=plaintext_r[i]) return false;
	}

//variable length keys
for (i=NUM_VARIABLE_KEY_TESTS;i<NUM_VARIABLE_KEY_TESTS+NUM_SET_KEY_TESTS; i++)
	{
	keylen=i-NUM_VARIABLE_KEY_TESTS+1;

	SetEncodeKey((DataBuffer){set_key,keylen});
	
	tempL=plaintext_l[i];
	tempR=plaintext_r[i];

	Encrypt(&tempL,&tempR,&mEncodeKey);
	if(tempL!=ciphertext_l[i] || tempR!=ciphertext_r[i]) return false;

	Decrypt(&tempL,&tempR,&mEncodeKey);
	if(tempL!=plaintext_l[i] || tempR!=plaintext_r[i]) return false;
	}

return true;
}