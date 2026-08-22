/* (c)2003 Hotsprings Inc. Licensed under GPL - see LICENSE in HotlineSources diresctory */


/*
	Routines for translating between an HL Community ID (length kHLCommID_MaxIDLen)
	and the 32-bit integer passed between the Hotline Server, Client and Ad System	
*/

#ifndef NULL
#define NULL	0L
#endif

#include "HotlineServIDTranslate.h"


// this is 1-indexed by the space at the front
const char kHLCommID_IntToCharTab[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._";
const int kHLCommID_IDBase = sizeof(kHLCommID_IntToCharTab); // -1 (because 1-indexed) +1 for the base
const char kHLCommID_CharToIntTab[] = 
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x25\x00"
	"\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x00\x00\x00\x00\x00\x00"
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x00\x00\x00\x00\x26"
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
	"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";

/*	
	sizeof(unsigned long) * 8 * log(2) / log(kIDBase) < 7 but > 6
	therefore, 6 is the max length we can use
*/

const int kHLCommID_MaxIDLen = 6;


/*	Note on how values are stored
	These functions work with data in a sort of "little endian" format.
	basically, for the string "BILLY6", the data is encoded as: (assume ^ denotes exponent)
	'6' * kIDBase^5 + 'Y' * kIDBase^4 + 'L' * kIDBase^3 + 'L' * kIDBase^2 + 'I' * kIDBase^1 + 'B' * kIDBase^0
 */

/*
	Translates the unsigned integer ID passed in by the Hotline Server into
	a string composed of only valid characters (max length kHLCommID_MaxIDLen)
	outCharID is a NULL-terminated string
	returns 0 if success, -1 invalid ID
	This could be more strict by not allowing NULLs within a string
	e.g., "fr\x00ank"
 */

int HLCommID_IntToChar( unsigned long inIntID, char *outCharID )
{
	unsigned long currentValue;
	unsigned long currentID;
	unsigned long nextID;
	int currentCharIndex;
	char *currentChar;
	
	currentID = inIntID;
	currentChar = outCharID;
	
	for (currentCharIndex = 0; currentCharIndex != kHLCommID_MaxIDLen; currentCharIndex++)
	{
		nextID = (currentID / kHLCommID_IDBase);
		
		currentValue = currentID - nextID * kHLCommID_IDBase;
		if (currentValue)
		{
			*currentChar++ = kHLCommID_IntToCharTab[currentValue];
		}
		
		currentID = nextID;
	}
	
	*currentChar = NULL;
	
	if (currentChar == outCharID)
	{
		// if the length is 0 - ie, no ID
		return -1;
	}
		
/*
	Uncomment this to be more strict on error checking
	
	If there's overflow (ie, there was a remainder after all the characters were processed
	which could come about if the string was not properly encoded in the beginning), we can
	return an "ID invalid" code.
	
	if( nextID )
	{
		return -1;
	}
*/
	return 0;
}

/*
	translates a string to an unsigned long
	only values A-Z 0-9 . and _ are accepted.
	All other values denote an empty character.
	(a-z are mapped to A-Z)
	inID is a NULL-terminated string (and only the first kHLCommID_MaxIDLen valid characters will be processed)
	returns the integer id
*/

unsigned long HLCommID_CharToInt( const char *inID )
{
	unsigned long result, multiplier;
	int charIndex;
	unsigned char current;
	
	result = 0;
	
	for (charIndex = 0, multiplier = 1; charIndex != kHLCommID_MaxIDLen; charIndex++, multiplier *= kHLCommID_IDBase)
	{
		current = inID[charIndex];
		if (current == NULL)
			break;
			
		result += multiplier * kHLCommID_CharToIntTab[current];
	}

	return result;
}

/* ————————————————————————————————————————————————————————————————————————— */
#pragma mark -

Uint8 const Base10ToBase36[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','0','1','2','3','4','5','6','7','8','9'};

Uint32 Base36ToBase10(Uint8 inData)
{
	switch (inData)
	{
		case 'A': case 'a': return 0;
		case 'B': case 'b': return 1;
		case 'C': case 'c': return 2;
		case 'D': case 'd': return 3;
		case 'E': case 'e': return 4;
		case 'F': case 'f': return 5;
		case 'G': case 'g': return 6;
		case 'H': case 'h': return 7;
		case 'I': case 'i': return 8;
		case 'J': case 'j': return 9;
		case 'K': case 'k': return 10;
		case 'L': case 'l': return 11;
		case 'M': case 'm': return 12;
		case 'N': case 'n': return 13;
		case 'O': case 'o': return 14;
		case 'P': case 'p': return 15;
		case 'Q': case 'q': return 16;
		case 'R': case 'r': return 17;
		case 'S': case 's': return 18;
		case 'T': case 't': return 19;
		case 'U': case 'u': return 20;
		case 'V': case 'v': return 21;
		case 'W': case 'w': return 22;
		case 'X': case 'x': return 23;
		case 'Y': case 'y': return 24;
		case 'Z': case 'z': return 25;
		case '0': 			return 26;
		case '1': 			return 27;
		case '2': 			return 28;
		case '3': 			return 29;
		case '4': 			return 30;
		case '5': 			return 31;
		case '6': 			return 32;
		case '7': 			return 33;
		case '8': 			return 34;
		case '9': 			return 35;
	}
	
	return 0;
} 

Uint8 ValidateBase36(Uint8 inData)
{
	if ((inData >= 'A' && inData <= 'Z') || (inData >= '0' && inData <= '9'))
		return inData;
		
	// convert from lowercase to uppercase
	if (inData >= 'a' && inData <= 'z')
		return inData - 32;
	
	return 0;
} 

// translate from unsigned integer to base 36
Uint32 TranslateToBase36(Uint32 inData, Uint8 *outData, Uint32 inMaxSize)
{
	if (!outData || !inMaxSize)
		return 0;
		
	Uint32 nSize = 0;
	Uint8 bufData[32];
	
	// process data
	while (inData >= 36 && nSize < inMaxSize && nSize < sizeof(bufData))
	{
		Uint32 nRest = inData%36;
		inData = inData/36;
	
		bufData[nSize++] = Base10ToBase36[nRest];
	}

	// check size
	if (nSize >= inMaxSize || nSize >= sizeof(bufData))
		return 0;

	bufData[nSize++] = Base10ToBase36[inData];

	// copy data
	for (Uint32 i = 0; i < nSize; i++)
		outData[i] = bufData[nSize - i - 1];

	return nSize;
}

// translate from unsigned integer to base 36
Uint32 TranslateToBase36(Uint32 inData, Uint8 *outData, Uint32 inMaxSize, Uint32 inMinSize)
{
	if (!outData || !inMaxSize || inMinSize > inMaxSize)
		return 0;
		
	Uint32 nSize = TranslateToBase36(inData, outData, inMaxSize);
	
	if (nSize < inMinSize)
	{
		Uint8 psTempData[32];
		psTempData[0] = UMemory::Copy(psTempData + 1, outData, nSize);
		
		nSize = inMinSize - nSize;
		UMemory::Fill(outData, nSize, 'A');
		nSize += UMemory::Copy(outData + nSize, psTempData + 1, psTempData[0]);
	}
	
	return nSize;
}

// translate from base 36 to unsigned integer (max 7 chars)
Uint32 TranslateFromBase36(const Uint8 *inData, Uint32 inSize)
{
	if (!inData || !inSize || inSize > 7)
		return 0;

	Uint32 nBase = 1;
	Uint32 nValue = 0;

	// process data
	const Uint8 *pData = inData + inSize;
	do
	{
		pData--;
		
		nValue += Base36ToBase10(*pData) * nBase;
		nBase *= 36;
	
	} while(pData != inData);
	
	return nValue;
}

