// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMDImportHelper.h"

#include "ShiftJisConvertTable.h"

FString FMmdImportHelper::ShiftJisToFString(const uint8* InBuffer, const int32 InSize)
{
	FString Output;
	Output.Reserve(3 * InSize); //ShiftJis won't give 4byte UTF8, so max. 3 byte per input char are needed
	
	PTRINT IndexInput = 0;

	while (IndexInput < InSize)
	{
		const uint8 ArraySection = InBuffer[IndexInput] >> 4;

		PTRINT ArrayOffset;
		if (ArraySection == 0x8) ArrayOffset = 0x100; //these are two-byte shiftjis
		else if (ArraySection == 0x9) ArrayOffset = 0x1100;
		else if (ArraySection == 0xE) ArrayOffset = 0x2100;
		else ArrayOffset = 0; //this is one byte shiftjis

		//determining real array offset
		if (ArrayOffset)
		{
			ArrayOffset += (InBuffer[IndexInput] & 0xf) << 8;
			IndexInput += 1;
			if (IndexInput >= InSize) break;
		}
		ArrayOffset += InBuffer[IndexInput];
		IndexInput += 1;
		ArrayOffset <<= 1;
		
		const uint16 UnicodeValue = (GShiftJisConvertTable[ArrayOffset] << 8) | GShiftJisConvertTable[ArrayOffset + 1];
		Output.AppendChar(UnicodeValue);
	}

	Output.Shrink();
	return Output;
}
