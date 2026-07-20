//----------------------------------------------------------------------
// GalFrame.c:
//   Galileo I/NAV frame sync and frame data decode
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//
//----------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "PlatformCtrl.h"
#include "DataTypes.h"
#include "TaskManager.h"
#include "GlobalVar.h"
#include "SupportPackage.h"
#include "TimeManager.h"

#define ENABLE_FEC2_DECODE 1	// if defined to 1, word type 17~20 will be used to do FEC2 RS decode

#if ENABLE_FEC2_DECODE == 1	// extra 13kB constant table for precalculated matrix
#include "fec2_matrix.c"
#endif

#define PAYLOAD_LENGTH 4	// 128bit page contents
#define PACKAGE_LENGTH (sizeof(SYMBOL_PACKAGE) + sizeof(unsigned int)*(PAYLOAD_LENGTH))	// 3 variables + 4 payload

extern U32 EphAlmMutex;

static int GalPageProc(PFRAME_INFO GalFrameInfo, PDATA_FOR_DECODE DataForDecode, unsigned int PageData[4]);
static int INavPageDecode(void* Param);
static int DecodeGalileoEphemeris(int svid, const unsigned int data[16]);
static int DecodeGalileoGroupDelay(int svid, const unsigned int data[4]);
static int DecodeGalileoAlmanac(int AllocationType, const unsigned int Page1[4], const unsigned int Page2[4]);

// for Viterbi decode
static int Distance[64], DistanceNew[64];
static unsigned long long Trace[64], TraceNew[64];
static int GalViterbiDecode(unsigned int SymbolBuffer[30], unsigned int DecodeResult[4]);
static void ViterbiDecodePair(unsigned int SymbolPair);
static void MergeBranches(const int StateArray[8], int DistanceSum);
static int FindMinIndex();

// for ephemeris page combination
static int PutEphemerisPage(PFRAME_INFO FrameInfo, int WordType, unsigned int Symbols[4], unsigned int EphIod);
static int PutFEC2Page(PFRAME_INFO FrameInfo, unsigned int Symbols[4]);

static int DefaultBgd[36] = {
	-38, -74,   6, -20, -46,  16, -46,  20,
	  2,  10, -38, -54, -50, -34, -46, -44,
	-46, -36, -66, -30, -42, -48, -36,  10,
	 24,  28,  10, -40, -42,  18, -58,   4,
	  0,   0,   0,   0
};

// placement of SymbolData and FrameData
// SymbolData index 0 to store sign bit of 4bit symbols to match sync pattern 0101100000
// SymbolData index 1~7 to store decoded 196 page bits (even page 114bits + odd page 82bits before CRC) with 28MSB of index 1 filled with 0
// FrameData index 0~29 to put page part symbols (4bit each symbol, total 240 symbols)
// FrameData index 30~45 to put word type 1~4
// FrameData index 46~53 to put word type 17~20 for FEC2 recovery (maximum 2 pages)
// FrameData index 54 to put first 32bits of word type 7
// FrameData index 55~57 to put last 96bits of word type 7~9 (combine with next page to do almanac decode)

//*************** Galileo navigation data process ****************
//* Do Galileo navigation data process and page sync
//* meaning of FrameStatus:
//* -1: frame sync not completed
//* >=0: bit0 for even page valid, bit 1 for check sync pattern again (confirm page sync correct)
// Parameters:
//   pFrameInfo: pointer to frame info structure
//   DataForDecode: pointer to structure of symbols to be decoded
// Return value:
//   current millisecond count within current week (negative if unknown)
int GalNavDataProc(PFRAME_INFO pFrameInfo, PDATA_FOR_DECODE DataForDecode)
{
	int data_count = 8;	// DataStream contais 8 4bit data
	int SymbolCount = -1;
	U32 DataStream = DataForDecode->DataStream;
	int PosIndex;
	unsigned int PageData[4];

	pFrameInfo->WeekNumber = -1;	// reset decoded week number to invalid
	// meaning of FrameState:
	// -1: frame sync not completed
	// >=0: bit 0 reserved, bit 1 for check sync pattern again
	// if not frame sync, find sync pattern 0101100000 first
	if (pFrameInfo->FrameStatus < 0)
	{
		while (data_count > 0)
		{
			// move in sign bit of the symbol
			pFrameInfo->SymbolData[0] = (pFrameInfo->SymbolData[0] << 1) | (DataStream >> 31);
			DataStream <<= 4;
			data_count --;
			pFrameInfo->SymbolNumber ++;
			if (pFrameInfo->SymbolNumber >= 10)
			{
				if ((pFrameInfo->SymbolData[0] & 0x3ff) == 0x160)	// sync pattern match
				{
					if (((DataForDecode->SymbolIndex + 25 - data_count) % 25) == 10)	// check sync pattern align to NH boundary, add 25 to avoid negative value
					{
						// sync pattern found, change frame status to 2
						pFrameInfo->FrameStatus = 2;
						// force symbol number to 0
						pFrameInfo->SymbolNumber = 0;
						break;
					}
				}
				pFrameInfo->SymbolNumber = 9;	// discard oldest symbol
			}
		}
	}
	// put data in FrameData
	if (pFrameInfo->FrameStatus >= 0)
	{
		while (data_count > 0)
		{
			if (pFrameInfo->SymbolNumber < 0)	// sync pattern
			{
				pFrameInfo->SymbolData[0] = (pFrameInfo->SymbolData[0] << 1) | (DataStream >> 31);
				if ((pFrameInfo->FrameStatus & 2) && pFrameInfo->SymbolNumber == -1)	// need to double check sync pattern
				{
					pFrameInfo->SymbolData[0] ^= 0x160;
					PosIndex = __builtin_popcount(pFrameInfo->SymbolData[0] & 0x3ff);	// count number of symbols that sign does not match sync pattern
					if (PosIndex == 0)	// sync pattern double check passed
						pFrameInfo->FrameStatus &= ~2;	// clear check sync pattern flag
					else if (PosIndex >= 2)	// sync pattern double check failed
					{
						pFrameInfo->FrameStatus = -1;	// need to search sync pattern again
						break;
					}
					// otherwise wait to next sync pattern and check again
				}
			}
			else
			{
				PosIndex = pFrameInfo->SymbolNumber % 30;
				pFrameInfo->FrameData[PosIndex] <<= 4;
				pFrameInfo->FrameData[PosIndex] |= (DataStream >> 28);
			}
			DataStream <<= 4;
			data_count --;
			pFrameInfo->SymbolNumber ++;
			if (pFrameInfo->SymbolNumber == 240)	// one page completed
			{
				pFrameInfo->SymbolNumber = -10;	// skip first 10 symbols for next page as sync pattern
				pFrameInfo->TickCount = DataForDecode->TickCount - data_count * 4;	// TickCount of current symbol (will not change until next page part)
				// do Viterbi decode on page data, 	assume each symbol has at least +-2 amplitude, 240 symbols total distance will be less than (7-2)*240
				if (GalViterbiDecode(pFrameInfo->FrameData, PageData) < 1200)
					SymbolCount = GalPageProc(pFrameInfo, DataForDecode, PageData) + data_count;
			}
		}
	}

	return SymbolCount * 4;
}

//*************** Do Galileo page part process ****************
// Parameters:
//   GalFrameInfo: pointer to frame info structure
//   DataForDecode: pointer to structure of symbols to be decoded
// Return value:
//   current symbols within week, <0 if unknown
int GalPageProc(PFRAME_INFO GalFrameInfo, PDATA_FOR_DECODE DataForDecode, unsigned int PageData[4])
{
	unsigned int crc;
	unsigned int Package[PACKAGE_LENGTH];
	PSYMBOL_PACKAGE SymbolPackage = (PSYMBOL_PACKAGE)Package;
	int WordType;
	int tow = -1, wn = -1;

	// after decode, MSB of first DWORD is even/odd flag
	// each page part has 120 valid bit, leaving 8LSB of last DWORD unused
 	if (PageData[0] & 0x80000000)	// odd page
	{
		if (GalFrameInfo->FrameStatus & 1)	// even page valid
		{
			// put decoded odd page part (remove 6bit tail) into GalFrameInfo->SymbolData
			// starting bit17 of GalFrameInfo->SymbolData[5] until bit0 of GalFrameInfo->SymbolData[7] (totally 82bits)
			GalFrameInfo->SymbolData[5] |= PageData[0] >> 14;
			GalFrameInfo->SymbolData[6] = (PageData[0] << 18) | (PageData[1] >> 14);
			GalFrameInfo->SymbolData[7] = (PageData[1] << 18) | ((PageData[2] >> 6) & 0x3ffff);
			// extract 24bit CRC
			crc = ((PageData[2] << 18) & 0xfc0000) | (PageData[3] >> 14);
			GalFrameInfo->FrameStatus &= ~1;	// clear even page valid flag
			if (Crc24qEncode(&GalFrameInfo->SymbolData[1], 196) != crc)	// CRC check fail
				return -1;
			// send whole nominal page to decode
			SymbolPackage->ChannelState = DataForDecode->ChannelState;
			SymbolPackage->FrameInfo = GalFrameInfo;	// put pointer of FrameInfo in FrameIndex
			SymbolPackage->PayloadLength = 4;
			SymbolPackage->Symbols[0] = (GalFrameInfo->SymbolData[1] << 30) | (GalFrameInfo->SymbolData[2] >> 2);
			SymbolPackage->Symbols[1] = (GalFrameInfo->SymbolData[2] << 30) | (GalFrameInfo->SymbolData[3] >> 2);
			SymbolPackage->Symbols[2] = (GalFrameInfo->SymbolData[3] << 30) | (GalFrameInfo->SymbolData[4] >> 2);
			SymbolPackage->Symbols[3] = (GalFrameInfo->SymbolData[4] << 30) | ((GalFrameInfo->SymbolData[5] >> 2) & 0x3fff0000);
			SymbolPackage->Symbols[3] |= (GalFrameInfo->SymbolData[5] & 0xffff);
			AddToTask(TASK_POSTMEAS, INavPageDecode, SymbolPackage, PACKAGE_LENGTH);
			// decode week number and week millisecond
			WordType = SymbolPackage->Symbols[0] >> 26;
			if (WordType == 0)
			{
				tow = (SymbolPackage->Symbols[3] & 0xfffff) + 2;	// plus current decoded page
				wn = (SymbolPackage->Symbols[3] >> 20) & 0xfff;
			}
			else if (WordType == 5)
			{
				tow = (((SymbolPackage->Symbols[2] & 0x7ff) << 9) | (SymbolPackage->Symbols[3] >> 23)) + 2;	// plus current decoded page
				wn = (SymbolPackage->Symbols[2] >> 11) & 0xfff;
			}
			else if (WordType == 6)
			{
				tow = ((SymbolPackage->Symbols[3] >> 3) & 0xfffff) + 2;	// plus current decoded page
			}
			GalFrameInfo->WeekNumber = wn;	// set decoded week number to TimeTag
		}
	}
	else	// even page
	{
		// put decoded even page part (remove 6bit tail) into GalFrameInfo->SymbolData
		// starting bit3 of GalFrameInfo->SymbolData[1] until bit18 of GalFrameInfo->SymbolData[5] (totally 114bits)
		GalFrameInfo->SymbolData[1] = PageData[0] >> 28;
		GalFrameInfo->SymbolData[2] = (PageData[0] << 4) | (PageData[1] >> 28);
		GalFrameInfo->SymbolData[3] = (PageData[1] << 4) | ((PageData[2] >> 20) & 0xf);
		GalFrameInfo->SymbolData[4] = (PageData[2] << 12) | (PageData[3] >> 20);
		GalFrameInfo->SymbolData[5] = (PageData[3] << 12) & 0xfffc0000;
		GalFrameInfo->FrameStatus |= 1;	// set even page valid flag
	}

	return tow * 250;	// 250 symbols per second
}

//*************** Task function to process one Galileo nominal page ****************
// Parameters:
//   Param: pointer to structure holding one page data
// Return value:
//   0
int INavPageDecode(void* Param)
{
	PSYMBOL_PACKAGE SymbolPackage = (PSYMBOL_PACKAGE)Param;
	int Svid = SymbolPackage->ChannelState->Svid;
	PFRAME_INFO FrameInfo = SymbolPackage->FrameInfo;
	int WordType = SymbolPackage->Symbols[0] >> 26;
	int TimeTag = FrameInfo->TickCount / 1000;
	PGNSS_EPHEMERIS pEph;

	if (WordType >= 1 && WordType <= 4)	// Ephemeris/Clock
	{
		pEph = &g_GalileoEphemeris[Svid-1];
		PutEphemerisPage(FrameInfo, WordType, SymbolPackage->Symbols, (pEph->flag & 1) ? pEph->iodc : (1 << 10));	// use invalid IOD value if ephemeris not valid
	}
	else if (WordType == 5)	// Iono/BGD
		DecodeGalileoGroupDelay(Svid, SymbolPackage->Symbols);
	else if (WordType >= 7 && WordType <= 10)	// Almanac
	{
		switch (WordType)
		{
		case 7:	// first part of almanac data set
			FrameInfo->TimeTag = TimeTag;
			memcpy(&FrameInfo->FrameData[54], SymbolPackage->Symbols, sizeof(unsigned int) * 4);
			break;
		case 8:	// second part of almanac data set
			if (FrameInfo->TimeTag < 0 || (TimeTag - FrameInfo->TimeTag) != 2)	// not continuous from previous WordType 7
				break;
			FrameInfo->TimeTag = TimeTag;
			DecodeGalileoAlmanac(0, &FrameInfo->FrameData[54], SymbolPackage->Symbols);
			memcpy(&FrameInfo->FrameData[55], SymbolPackage->Symbols + 1, sizeof(unsigned int) * 3);
			break;
		case 9:	// third part of almanac data set
			if (FrameInfo->TimeTag < 0 || (TimeTag - FrameInfo->TimeTag) != 28)	// not continuous from previous WordType 8
				break;
			FrameInfo->TimeTag = TimeTag;
			DecodeGalileoAlmanac(1, &FrameInfo->FrameData[54], SymbolPackage->Symbols);
			memcpy(&FrameInfo->FrameData[55], SymbolPackage->Symbols + 1, sizeof(unsigned int) * 3);
			break;
		case 10:	// fourth part of almanac data set
			if (FrameInfo->TimeTag < 0 || (TimeTag - FrameInfo->TimeTag) != 2)	// not continuous from previous WordType 9
				break;
			DecodeGalileoAlmanac(2, &FrameInfo->FrameData[54], SymbolPackage->Symbols);
			break;
		}
	}
#if ENABLE_FEC2_DECODE == 1
	else if (WordType >= 17 && WordType <= 20)	// FEC2 RS page
		PutFEC2Page(FrameInfo, SymbolPackage->Symbols);
#endif

	if ((FrameInfo->FrameFlag & 0xf) == 0x0f)	// bit0~4 set, WordType 1~5 completed
	{
		DecodeGalileoEphemeris(Svid, FrameInfo->FrameData + 30);
		FrameInfo->FrameFlag &= ~0xf;
	}

	return 0;
}

//*************** Galileo ephemeris decode ****************
// Parameters:
//   svid: SVID of corresponding satellite
//   data: data contents of WordType1~4, each occupies 4 DWORD
// Return value:
//   0 if decode unsuccessful, svid otherwise
int DecodeGalileoEphemeris(int svid, const unsigned int data[16])
{
	PGNSS_EPHEMERIS pEph = &g_GalileoEphemeris[svid-1];

	// check IOD identical and svid matches source signal
//	if (svid != GET_UBITS(data[12], 10, 6))
//		return 0;

	DEBUG_OUTPUT(OUTPUT_CONTROL(DATA_DECODE, INFO), "Decode ephemeris of E%02d\n", svid);
	MutexTake(EphAlmMutex);

	pEph->svid = svid - 1 + MIN_GAL_SAT_ID;
	pEph->health = 0;
	pEph->flag |= 1;
	pEph->iodc = (unsigned short)GET_UBITS(data[12], 16, 10);

	pEph->toe = GET_UBITS(data[0], 2, 14) * 60;
	pEph->M0 = ScaleDouble(((data[0] << 30) & 0xc0000000) | GET_UBITS(data[1], 2, 30), 31) * PI;
	pEph->ecc = ScaleDoubleU(((data[1] << 30) & 0xc0000000) | GET_UBITS(data[2], 2, 30), 33);
	pEph->sqrtA = ScaleDoubleU(((data[2] << 30) & 0xc0000000) | GET_UBITS(data[3], 2, 30), 19);
	pEph->omega0 = ScaleDouble(((data[4] << 16) & 0xffff0000) | GET_UBITS(data[5], 16, 16), 31) * PI;
	pEph->i0 = ScaleDouble(((data[5] << 16) & 0xffff0000) | GET_UBITS(data[6], 16, 16), 31) * PI;
	pEph->w = ScaleDouble(((data[6] << 16) & 0xffff0000) | GET_UBITS(data[7], 16, 16), 31) * PI;
	pEph->idot = ScaleDouble(GET_BITS(data[7], 2, 14), 43) * PI;
	pEph->omega_dot = ScaleDouble((GET_BITS(data[8], 0, 16) << 8) | GET_UBITS(data[9], 24, 8), 43) * PI;
	pEph->delta_n = ScaleDouble(GET_BITS(data[9], 8, 16), 43) * PI;
	pEph->cuc = ScaleDouble((GET_BITS(data[9], 0, 8) << 8) | GET_UBITS(data[10], 24, 8), 29);
	pEph->cus = ScaleDouble(GET_BITS(data[10], 8, 16), 29);
	pEph->crc = ScaleDouble((GET_BITS(data[10], 0, 8) << 8) | GET_UBITS(data[11], 24, 8), 5);
	pEph->crs = ScaleDouble(GET_BITS(data[11], 8, 16), 5);
	pEph->cic = ScaleDouble((GET_BITS(data[12], 0, 10) << 6) | GET_UBITS(data[13], 26, 6), 29);
	pEph->cis = ScaleDouble(GET_BITS(data[13], 10, 16), 29);
	pEph->toc = (((data[13] & 0x3ff) << 4) | GET_UBITS(data[14], 28, 4)) * 60;
	pEph->af0 = ScaleDouble((GET_BITS(data[14], 0, 28) << 3) | GET_UBITS(data[15], 29, 3), 34);
	pEph->af1 = ScaleDouble(GET_BITS(data[15], 8, 21), 46);
	pEph->af2 = ScaleDouble(GET_BITS(data[15], 2, 6), 59);
	if ((pEph->flag & 2) == 0)	// if tgd not valid, use preset value
	{
		pEph->tgd = ScaleDouble(DefaultBgd[svid - 1], 32);
		pEph->tgd2 = 0.;	// BGD(E1,E5a) not used
	}
	pEph->week = GetReceiverWeekNumber(SIGNAL_E1);

	// calculate derived variables
	pEph->axis = pEph->sqrtA * pEph->sqrtA;
	pEph->n = WGS_SQRT_GM / (pEph->sqrtA * pEph->axis) + pEph->delta_n;
	pEph->root_ecc = sqrt(1.0 - pEph->ecc * pEph->ecc);
	pEph->omega_t = pEph->omega0 - WGS_OMEGDOTE * pEph->toe;
	pEph->omega_delta = pEph->omega_dot - WGS_OMEGDOTE;

	MutexGive(EphAlmMutex);
	return svid;
}

int DecodeGalileoGroupDelay(int svid, const unsigned int data[4])
{
	PGNSS_EPHEMERIS pEph = &g_GalileoEphemeris[svid-1];

	// set tgd and tgd2 from BGD disregard IOD and validity of ephemeris
	pEph->tgd = ScaleDouble(GET_BITS(data[1], 7, 10), 32);
	pEph->tgd2 = ScaleDouble((GET_BITS(data[1], 0, 7) << 3) | GET_UBITS(data[2], 29, 3), 32);
	pEph->flag |= 2;	// set bit1 to indicate tgd from decoded navigation message

	return svid;
}

//*************** Galileo ephemeris decode ****************
// Parameters:
//   AllocationType: indicate contents of Page1 and Page2, 0 for WordType 7/8, 1 for WordType 8/9, 2 for WordType 9/10
//   Page1: data contents of first word
//   Page2: data contents of second word
// Return value:
//   0 if decode unsuccessful, svid otherwise
#define SQRT_A0 5440.588203494177338011974948823
#define NORMINAL_I0 0.97738438111682456307726683035362
int DecodeGalileoAlmanac(int AllocationType, const unsigned int Page1[4], const unsigned int Page2[4])
{
	int week, WNa, toa, svid = 0;
	PMIDI_ALMANAC pAlm;
	int idata;
	unsigned int udata;

	if ((week = GetReceiverWeekNumber(SIGNAL_E1)) < 0)
		return 0;
	// check week number
	WNa = GET_UBITS(Page1[0], 20, 2);	// last 2bit of almanac week
	if (((week + 1) & 3) == WNa)
		week  ++;
	else if (((week - 1) & 3) == WNa)
		week  --;
	else if ((week & 3) != WNa)
		return 0;

	toa = GET_UBITS(Page1[0], 10, 10) * 600;
	switch (AllocationType)
	{
	case 0:
		svid = GET_UBITS(Page1[0], 4, 6);
		break;
	case 1:
		svid = GET_UBITS(Page1[1], 15, 6);
		break;
	case 2:
		svid = GET_UBITS(Page1[2], 19, 6);
		break;
	}
	if (svid < 1 || svid > 36)
		return 0;
	pAlm = &g_GalileoAlmanac[svid - 1];
	if (pAlm->week == week && pAlm->toa == toa)	// repeat almanac
		return svid;

	MutexTake(EphAlmMutex);

	pAlm->svid = svid;
	pAlm->week = week;
	pAlm->toa = toa;
	switch (AllocationType)
	{
		case 0:
			idata = (GET_BITS(Page1[0], 0, 4) << 9) | GET_UBITS(Page1[1], 23, 9);
			pAlm->sqrtA = ScaleDoubleU(idata, 9) + SQRT_A0;
			pAlm->ecc = ScaleDoubleU(GET_UBITS(Page1[1], 12, 11), 16);
			idata = (GET_BITS(Page1[1], 0, 12) << 4) | GET_UBITS(Page1[2], 28, 4);
			pAlm->w = ScaleDouble(idata, 15) * PI;
			pAlm->i0 = NORMINAL_I0 + ScaleDouble(GET_BITS(Page1[2], 17, 11), 14) * PI;
			pAlm->omega0 = ScaleDouble(GET_BITS(Page1[2], 1, 16), 15) * PI;
			idata = (GET_BITS(Page1[2], 0, 1) << 10) | GET_UBITS(Page1[3], 22, 10);
			pAlm->omega_dot = ScaleDouble(idata, 33) * PI;
			pAlm->M0 = ScaleDouble(GET_BITS(Page1[3], 6, 16), 15) * PI;
			pAlm->af0 = ScaleDouble(GET_BITS(Page2[0], 6, 16), 19);
			idata = (GET_BITS(Page2[0], 0, 6) << 7) | GET_UBITS(Page2[1], 25, 7);
			pAlm->af1 = ScaleDouble(idata, 38);
			pAlm->health = GET_UBITS(Page2[1], 21, 2);
			break;
		case 1:
			pAlm->sqrtA = ScaleDoubleU(GET_BITS(Page1[1], 2, 13), 9) + SQRT_A0;
			udata = (GET_UBITS(Page1[1], 0, 2) << 9) | GET_UBITS(Page1[2], 23, 9);
			pAlm->ecc = ScaleDoubleU(udata, 16);
			pAlm->w = ScaleDouble(GET_BITS(Page1[2], 7, 16), 15) * PI;
			idata = (GET_BITS(Page1[2], 0, 7) << 4) | GET_UBITS(Page1[3], 28, 4);
			pAlm->i0 = NORMINAL_I0 + ScaleDouble(idata, 14) * PI;
			pAlm->omega0 = ScaleDouble(GET_BITS(Page1[3], 12, 16), 15) * PI;
			pAlm->omega_dot = ScaleDouble(GET_BITS(Page1[3], 1, 11), 33) * PI;
			idata = (GET_BITS(Page2[0], 0, 10) << 6) | GET_UBITS(Page2[1], 26, 6);
			pAlm->M0 = ScaleDouble(idata, 15) * PI;
			pAlm->af0 = ScaleDouble(GET_BITS(Page2[1], 10, 16), 19);
			idata = (GET_BITS(Page2[1], 0, 10) << 3) | GET_UBITS(Page2[2], 29, 3);
			pAlm->af1 = ScaleDouble(idata, 38);
			pAlm->health = GET_UBITS(Page2[2], 25, 2);
			break;
		case 2:
			pAlm->sqrtA = ScaleDoubleU(GET_BITS(Page1[2], 6, 13), 9) + SQRT_A0;
			udata = (GET_UBITS(Page1[2], 0, 6) << 5) | GET_UBITS(Page1[3], 27, 5);
			pAlm->ecc = ScaleDoubleU(udata, 16);
			pAlm->w = ScaleDouble(GET_BITS(Page1[3], 11, 16), 15) * PI;
			pAlm->i0 = NORMINAL_I0 + ScaleDouble(GET_BITS(Page1[3], 0, 11), 14) * PI;
			pAlm->omega0 = ScaleDouble(GET_BITS(Page2[0], 6, 16), 15) * PI;
			idata = (GET_BITS(Page2[0], 0, 6) << 5) | GET_UBITS(Page2[1], 27, 5);
			pAlm->omega_dot = ScaleDouble(idata, 33) * PI;
			pAlm->M0 = ScaleDouble(GET_BITS(Page2[1], 11, 16), 15) * PI;
			idata = (GET_BITS(Page2[1], 0, 11) << 5) | GET_UBITS(Page2[2], 27, 5);
			pAlm->af0 = ScaleDouble(idata, 19);
			pAlm->af1 = ScaleDouble(GET_BITS(Page2[2], 14, 13), 38);
			pAlm->health = GET_UBITS(Page2[2], 10, 2);
			break;
	}

	pAlm->flag = 1;
	// calculate derived variables
	pAlm->axis = pAlm->sqrtA * pAlm->sqrtA;
	pAlm->n = WGS_SQRT_GM / (pAlm->sqrtA * pAlm->axis);
	pAlm->root_ecc = sqrt(1.0 - pAlm->ecc * pAlm->ecc);
	pAlm->omega_t = pAlm->omega0 - WGS_OMEGDOTE * (pAlm->toa);
	pAlm->omega_delta = pAlm->omega_dot - WGS_OMEGDOTE;

	MutexGive(EphAlmMutex);
	return svid;
}

//*************** Galileo Viterbi decode for one page ****************
//* assume input symbol is 4bit, totally 240 symbols placed from MSB with interleaving
// Parameters:
//   SymbolBuffer: array of input symbols
//   DecodeResult: decoded result, 120bits MSB first (8LSB of DecodeResult[1] and 8MSB of DecodeResult[2] will overlap)
// Return value:
//   minimum distance
int GalViterbiDecode(unsigned int SymbolBuffer[30], unsigned int DecodeResult[4])
{
//	unsigned int Symbols[30], DataWord = SymbolBuffer[0];	// store deinterleaved symbols
	int i, index = 0;
	int TotalMinDistance, MinState;

	memset(Distance, 1, sizeof(Distance));
	Distance[0] = 0;	// set Distance a big value except index 0 to ensure start state is 0
	for (i = 0; i < 30 * 4; i ++)
	{
		ViterbiDecodePair(SymbolBuffer[i>>2] >> (24 - (i & 3)*8));
		if (i >= 63 && ((i & 7) == 7))	// to reduce the number of comparision, do it every 8 bits
		{
			MinState = FindMinIndex();
			DecodeResult[(i - 56) / 32] = (DecodeResult[(i - 56) / 32] << 8) | ((unsigned int)(Trace[MinState] >> 56));
		}
	}
	MinState = FindMinIndex();
	DecodeResult[2] = (unsigned int)(Trace[MinState] >> 32);
	DecodeResult[3] = (unsigned int)(Trace[MinState]);
	TotalMinDistance = Distance[MinState];

	return TotalMinDistance;
}

static const int OutputTable[4][8] = {
	{ 3, 5, 11, 13, 16, 22, 24, 30, },	// output 00 if input 0
	{ 0, 6,  8, 14, 19, 21, 27, 29, },	// output 01 if input 0
	{ 2, 4, 10, 12, 17, 23, 25, 31, },	// output 10 if input 0
	{ 1, 7,  9, 15, 18, 20, 26, 28, },	// output 11 if input 0
};

//*************** Viterbi decoder to decode one pair of symbols ****************
//* assume input symbol is 4bit, first symbol in bit7~4, second symbol in bit3~0
// Parameters:
//   SymbolPair: input symbols
// Return value:
//   Minimum increased distance
void ViterbiDecodePair(unsigned int SymbolPair)
{
	int DistanceSum;
	int Symbol1, Symbol2;

	Symbol1 = (int)((SymbolPair >> 4) & 0xf);
	Symbol2 = (int)(SymbolPair & 0xf);
	DistanceSum = (Symbol1 ^ 0x7) + (Symbol2 ^ 0x7);	// distance for output 00
	MergeBranches(OutputTable[0], DistanceSum);
	MergeBranches(OutputTable[3], 30 - DistanceSum);
	DistanceSum = (Symbol1 ^ 0x7) + (Symbol2 ^ 0x8);	// distance for output 01
	MergeBranches(OutputTable[1], DistanceSum);
	MergeBranches(OutputTable[2], 30 - DistanceSum);

//	printf("Decode one pair completed with MinState=%d\n", MinState);
	// copy back distances and trace
	memcpy(Distance, DistanceNew, sizeof(Distance));
	memcpy(Trace, TraceNew, sizeof(Trace));
//	for (state = 0; state < 64; state ++)
//		printf("Distance = %8d Trace = %08x%08x\n", Distance[state], (unsigned int)(Trace[state] >> 32), (unsigned int)(Trace[state] & 0xffffffff));
}

//*************** Viterbi decoder to decode one pair of symbols ****************
//* assume input symbol is 4bit, first symbol in bit7~4, second symbol in bit3~0
// Parameters:
//   SymbolPair: input symbols
// Return value:
//   none
void MergeBranches(const int StateArray[8], int DistanceSum)
{
	int Distance00, Distance01, Distance10, Distance11;
	int DistanceCmp = 30 - DistanceSum;	// complement distance
	int i, state;

	for (i = 0; i < 8; i ++)
	{
		state = StateArray[i];
		Distance00 = Distance[state] + DistanceSum;	// distance for state with input 0
		Distance01 = Distance[state] + DistanceCmp;	// distance for state with input 1
		Distance10 = Distance[state+32] + DistanceCmp;	// distance for state+32 with input 0
		Distance11 = Distance[state+32] + DistanceSum;	// distance for state+32 with input 1

		// merge branch to state*2 by comparing Distance00 and Distance10, last bit of trace is 0
		if (Distance00 <= Distance10)	// from first branch (state)
		{
			TraceNew[state*2] = Trace[state] << 1;
			DistanceNew[state*2] = Distance00;
		}
		else	// from first branch (state+32)
		{
			TraceNew[state*2] = Trace[state+32] << 1;
			DistanceNew[state*2] = Distance10;
		}

		// merge branch to state*2+1 by comparing Distance01 and Distance11, last bit of trace is 1
		if (Distance01 <= Distance11)	// from first branch (state)
		{
			TraceNew[state*2+1] = (Trace[state] << 1) + 1;
			DistanceNew[state*2+1] = Distance01;
		}
		else	// from first branch (state+32)
		{
			TraceNew[state*2+1] = (Trace[state+32] << 1) + 1;
			DistanceNew[state*2+1] = Distance11;
		}
	}
}

//*************** Find state index with minimum distance ****************
//* assume input symbol is 4bit, first symbol in bit7~4, second symbol in bit3~0
// Parameters:
//   none
// Return value:
//   state with minimum distance
int FindMinIndex()
{
	int i, MinState = 0;
	int MinDistance = 32 * 250;	// maximum distance 32 * 250

	// calculate minimum distance
	for (i = 0; i < 64; i ++)
	{
		if (MinDistance > Distance[i])
		{
			MinDistance = Distance[i];
			MinState = i;
		}
	}

	return MinState;
}

//*************** Store new 128bit page contents of word type 1~4 ****************
// Parameters:
//   FrameInfo: pointer to frame info structure
//   WordType: word type range 1~4
//   Symbols: array for 128bit contents
//   EphIod: IOD of current valid ephemeris (0~1023, 1024 for ephemeris invalid)
// Return value:
//   word type or 0 if contents not stored (duplicate IOD)
int PutEphemerisPage(PFRAME_INFO FrameInfo, int WordType, unsigned int Symbols[4], unsigned int EphIod)
{
	int i;
	unsigned int iod = GET_UBITS(Symbols[0], 16, 10);

	if (iod == EphIod)	// if match current ephemeris IOD, do not decode
		return 0;

	for (i = 0; i < 4; i ++)
	{
		if (FrameInfo->FrameFlag & (1 << i))	// if any word filled, check IOD identical
		{
			if (iod != GET_UBITS(FrameInfo->FrameData[30 + i * 4], 16, 10))
				FrameInfo->FrameFlag &= ~0xf;
			break;
		}
	}
	memcpy(FrameInfo->FrameData + WordType * 4 + 26, Symbols, sizeof(unsigned int) * 4);
	FrameInfo->FrameFlag |= (1 << (WordType - 1));

	return WordType;
}

#if ENABLE_FEC2_DECODE == 1
static int RecoveryEphemeris1(PFRAME_INFO FrameInfo);
static int RecoveryEphemeris2(PFRAME_INFO FrameInfo);
static void Stream2Vector(unsigned int Stream[4], unsigned char Vector[14]);
static void Vector2Stream(unsigned char Vector[14], unsigned int Stream[4]);
static unsigned char GF8IntMul(unsigned char a, unsigned char b);
static void MaxtrixVectorMulAcc(const unsigned char *Matrix, const unsigned char *Vector, unsigned char *Result, int Length);

//*************** Store new 128bit page contents of word type 17~20 ****************
// if CED page + FEC2 page number reaches 4, try to recover missing CED page
// Parameters:
//   FrameInfo: pointer to frame info structure
//   Symbols: array for 128bit contents
// Return value:
//   word type or 0 if recover failed
int PutFEC2Page(PFRAME_INFO FrameInfo, unsigned int Symbols[4])
{
	if ((FrameInfo->FrameFlag & 0x30) != 0x30)	// if any of two FEC2 page not filled
	{
		// put data and set flag
		if (!(FrameInfo->FrameFlag & 0x10))
		{
			memcpy(&FrameInfo->FrameData[46], Symbols, sizeof(unsigned int) * 4);
			FrameInfo->FrameFlag |= 0x10;
		}
		else
		{
			memcpy(&FrameInfo->FrameData[50], Symbols, sizeof(unsigned int) * 4);
			FrameInfo->FrameFlag |= 0x20;
		}
		// check any 4 CED or FEC2 page available
		if (__builtin_popcount(FrameInfo->FrameFlag & 0x3f) >= 4)
		{
			if (__builtin_popcount(FrameInfo->FrameFlag & 0xf) == 3)
				return RecoveryEphemeris1(FrameInfo);
			else
				return RecoveryEphemeris2(FrameInfo);
		}
	}
	return 0;
}

int RecoveryEphemeris1(PFRAME_INFO FrameInfo)
{
	int TargetPage = __builtin_ctz(~FrameInfo->FrameFlag);
	int ParityPage = (FrameInfo->FrameData[46] >> 26) - 17;
	int iod = ((TargetPage != 0) ? FrameInfo->FrameData[30] : FrameInfo->FrameData[34]) & 0x3ff0000;
	int i;
	unsigned char TypeIod[2], Message[14], Residual[14];

	if (((FrameInfo->FrameData[46] ^ iod) & 0x30000) != 0)	// if 2LSB of IOD not identical
	{
		FrameInfo->FrameFlag &= ~0x30;	// clear content valid flag
		return 0;
	}
	// recover Type+IOD
	TypeIod[0] = (unsigned char)(0x4 | ((iod >> 16) & 0x3)); TypeIod[1] = (unsigned char)(iod >> 18);
	Stream2Vector(&FrameInfo->FrameData[46], Residual);
	MaxtrixVectorMulAcc(&MatrixD[ParityPage*14][0], TypeIod, Residual, 2);
	for (i = 0; i < 4; i ++)
	{
		if (i == TargetPage) continue;
		Stream2Vector(FrameInfo->FrameData + i * 4 + 30, Message);
		MaxtrixVectorMulAcc((const unsigned char *)MatrixP[ParityPage][i], Message, Residual, 14);
	}
	memset(Message, 0, sizeof(Message));
	MaxtrixVectorMulAcc((const unsigned char *)MatrixInvP[ParityPage][TargetPage], Residual, Message, 14);
	Vector2Stream(Message, FrameInfo->FrameData + TargetPage * 4 + 30);
	FrameInfo->FrameData[TargetPage * 4 + 30] |= (((TargetPage + 1) << 26) | iod);
	FrameInfo->FrameFlag |= (1 << TargetPage);
	return TargetPage + 1;
}

int RecoveryEphemeris2(PFRAME_INFO FrameInfo)
{
	int m1, m2, p1, p2, t1, t2;	// message, parity and target index
	int MessageIndex, ParityIndex;
    const int index_map[] = {0, 2, 4, 0, 5, 1, 3, 0};
	int target_map[6][2] = { {2, 3}, {1, 3}, {1, 2}, {0, 3}, {0, 2}, {0, 1} };
	unsigned char TypeIod[2], Message1[14], Message2[14], Residual1[14], Residual2[14];
	int iod;

	// determine index of known message m1/m2, missing message index t1/t2
	MessageIndex = index_map[FrameInfo->FrameFlag & 7];
	m1 = target_map[5-MessageIndex][0];
	m2 = target_map[5-MessageIndex][1];
	t1 = target_map[MessageIndex][0];
	t2 = target_map[MessageIndex][1];
	// determine index of known parity
	p1 = (FrameInfo->FrameData[46] >> 26) - 17;
	p2 = (FrameInfo->FrameData[50] >> 26) - 17;
	ParityIndex = index_map[((1 << p1) | (1 << p2)) & 7];

	// recover Type+IOD
	iod = FrameInfo->FrameData[m1 * 4 + 30] & 0x3ff0000;
	TypeIod[0] = (unsigned char)(0x4 | ((iod >> 16) & 0x3)); TypeIod[1] = (unsigned char)(iod >> 18);
	Stream2Vector(&FrameInfo->FrameData[m1 * 4 + 30], Message1);
	Stream2Vector(&FrameInfo->FrameData[m2 * 4 + 30], Message2);
	// calculate r1
	Stream2Vector(&FrameInfo->FrameData[46], Residual1);
	MaxtrixVectorMulAcc(&MatrixD[p1*14][0], TypeIod, Residual1, 2);
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p1][m1], Message1, Residual1, 14);
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p1][m2], Message2, Residual1, 14);
	// calculate r2
	Stream2Vector(&FrameInfo->FrameData[50], Residual2);
	MaxtrixVectorMulAcc(&MatrixD[p2*14][0], TypeIod, Residual2, 2);
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p2][m1], Message1, Residual2, 14);
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p2][m2], Message2, Residual2, 14);
	// minus r2 from r1
	memset(Message1, 0, sizeof(Message1));
	MaxtrixVectorMulAcc((const unsigned char *)MatrixInvP[p2][t2], Residual2, Message1, 14);
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p1][t2], Message1, Residual1, 14);
	// calculate target message 1
	memset(Message1, 0, sizeof(Message1));
	MaxtrixVectorMulAcc((const unsigned char *)MatrixS[MessageIndex][ParityIndex], Residual1, Message1, 14);
	// calculate target message 2
	MaxtrixVectorMulAcc((const unsigned char *)MatrixP[p2][t1], Message1, Residual2, 14);
	memset(Message2, 0, sizeof(Message2));
	MaxtrixVectorMulAcc((const unsigned char *)MatrixInvP[p2][t2], Residual2, Message2, 14);

	// recover stream in FrameInfo
	Vector2Stream(Message1, FrameInfo->FrameData + t1 * 4 + 30);
	FrameInfo->FrameData[t1 * 4 + 30] |= (((t1 + 1) << 26) | iod);
	FrameInfo->FrameFlag |= (1 << t1);
	Vector2Stream(Message2, FrameInfo->FrameData + t2 * 4 + 30);
	FrameInfo->FrameData[t2 * 4 + 30] |= (((t2 + 1) << 26) | iod);
	FrameInfo->FrameFlag |= (1 << t2);

	return (1 << t1) | (1 << t2);
}

//*************** Convert 4 DWORD stream (MSB first) to 8bit vector ****************
// first 16bits of stream will be omitted to form length 14 vectors
// Parameters:
//   Stream: bit stream of 32bit DWORD with total 128 bits
//   Vector: 8bit vector with size of 14
// Return value:
//   none
void Stream2Vector(unsigned int Stream[4], unsigned char Vector[14])
{
	Vector[0] = (unsigned char)(Stream[0] >> 8); Vector[1] = (unsigned char)(Stream[0]);
	Vector[2] = (unsigned char)(Stream[1] >> 24); Vector[3] = (unsigned char)(Stream[1] >> 16); Vector[4] = (unsigned char)(Stream[1] >> 8); Vector[5] = (unsigned char)(Stream[1]);
	Vector[6] = (unsigned char)(Stream[2] >> 24); Vector[7] = (unsigned char)(Stream[2] >> 16); Vector[8] = (unsigned char)(Stream[2] >> 8); Vector[9] = (unsigned char)(Stream[2]);
	Vector[10] = (unsigned char)(Stream[3] >> 24); Vector[11] = (unsigned char)(Stream[3] >> 16); Vector[12] = (unsigned char)(Stream[3] >> 8); Vector[13] = (unsigned char)(Stream[3]);
}

//*************** Convert 8bit vector to 4 DWORD stream (MSB first) ****************
// first 16bits of stream be filled with 0
// Parameters:
//   Vector: 8bit vector with size of 14
//   Stream: bit stream of 32bit DWORD with total 128 bits
// Return value:
//   none
void Vector2Stream(unsigned char Vector[14], unsigned int Stream[4])
{
	Stream[0] = ((unsigned int)Vector[0] << 8) | (unsigned int)Vector[1];
	Stream[1] = ((unsigned int)Vector[2] << 24) | (unsigned int)(Vector[3] << 16) | ((unsigned int)Vector[4] << 8) | (unsigned int)Vector[5];
	Stream[2] = ((unsigned int)Vector[6] << 24) | (unsigned int)(Vector[7] << 16) | ((unsigned int)Vector[8] << 8) | (unsigned int)Vector[9];
	Stream[3] = ((unsigned int)Vector[10] << 24) | (unsigned int)(Vector[11] << 16) | ((unsigned int)Vector[12] << 8) | (unsigned int)Vector[13];
}

const unsigned char Oct2Power[256] = {
   0,   0,   1,  25,   2,  50,  26, 198,   3, 223,  51, 238,  27, 104, 199,  75,
   4, 100, 224,  14,  52, 141, 239, 129,  28, 193, 105, 248, 200,   8,  76, 113,
   5, 138, 101,  47, 225,  36,  15,  33,  53, 147, 142, 218, 240,  18, 130,  69,
  29, 181, 194, 125, 106,  39, 249, 185, 201, 154,   9, 120,  77, 228, 114, 166,
   6, 191, 139,  98, 102, 221,  48, 253, 226, 152,  37, 179,  16, 145,  34, 136,
  54, 208, 148, 206, 143, 150, 219, 189, 241, 210,  19,  92, 131,  56,  70,  64,
  30,  66, 182, 163, 195,  72, 126, 110, 107,  58,  40,  84, 250, 133, 186,  61,
 202,  94, 155, 159,  10,  21, 121,  43,  78, 212, 229, 172, 115, 243, 167,  87,
   7, 112, 192, 247, 140, 128,  99,  13, 103,  74, 222, 237,  49, 197, 254,  24,
 227, 165, 153, 119,  38, 184, 180, 124,  17,  68, 146, 217,  35,  32, 137,  46,
  55,  63, 209,  91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190,  97,
 242,  86, 211, 171,  20,  42,  93, 158, 132,  60,  57,  83,  71, 109,  65, 162,
  31,  45,  67, 216, 183, 123, 164, 118, 196,  23,  73, 236, 127,  12, 111, 246,
 108, 161,  59,  82,  41, 157,  85, 170, 251,  96, 134, 177, 187, 204,  62,  90,
 203,  89,  95, 176, 156, 169, 160,  81,  11, 245,  22, 235, 122, 117,  44, 215,
 79, 174, 213, 233, 230, 231, 173, 232, 116, 214, 244, 234, 168,  80,  88, 175 };

const unsigned char Power2Oct[510] = {
   1,   2,   4,   8,  16,  32,  64, 128,  29,  58, 116, 232, 205, 135,  19,  38,
  76, 152,  45,  90, 180, 117, 234, 201, 143,   3,   6,  12,  24,  48,  96, 192,
 157,  39,  78, 156,  37,  74, 148,  53, 106, 212, 181, 119, 238, 193, 159,  35,
  70, 140,   5,  10,  20,  40,  80, 160,  93, 186, 105, 210, 185, 111, 222, 161,
  95, 190,  97, 194, 153,  47,  94, 188, 101, 202, 137,  15,  30,  60, 120, 240,
 253, 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163,  91, 182, 113, 226,
 217, 175,  67, 134,  17,  34,  68, 136,  13,  26,  52, 104, 208, 189, 103, 206,
 129,  31,  62, 124, 248, 237, 199, 147,  59, 118, 236, 197, 151,  51, 102, 204,
 133,  23,  46,  92, 184, 109, 218, 169,  79, 158,  33,  66, 132,  21,  42,  84,
 168,  77, 154,  41,  82, 164,  85, 170,  73, 146,  57, 114, 228, 213, 183, 115,
 230, 209, 191,  99, 198, 145,  63, 126, 252, 229, 215, 179, 123, 246, 241, 255,
 227, 219, 171,  75, 150,  49,  98, 196, 149,  55, 110, 220, 165,  87, 174,  65,
 130,  25,  50, 100, 200, 141,   7,  14,  28,  56, 112, 224, 221, 167,  83, 166,
  81, 162,  89, 178, 121, 242, 249, 239, 195, 155,  43,  86, 172,  69, 138,   9,
  18,  36,  72, 144,  61, 122, 244, 245, 247, 243, 251, 235, 203, 139,  11,  22,
  44,  88, 176, 125, 250, 233, 207, 131,  27,  54, 108, 216, 173,  71, 142,   1,
   2,   4,   8,  16,  32,  64, 128,  29,  58, 116, 232, 205, 135,  19,  38,  76,
 152,  45,  90, 180, 117, 234, 201, 143,   3,   6,  12,  24,  48,  96, 192, 157,
  39,  78, 156,  37,  74, 148,  53, 106, 212, 181, 119, 238, 193, 159,  35,  70,
 140,   5,  10,  20,  40,  80, 160,  93, 186, 105, 210, 185, 111, 222, 161,  95,
 190,  97, 194, 153,  47,  94, 188, 101, 202, 137,  15,  30,  60, 120, 240, 253,
 231, 211, 187, 107, 214, 177, 127, 254, 225, 223, 163,  91, 182, 113, 226, 217,
 175,  67, 134,  17,  34,  68, 136,  13,  26,  52, 104, 208, 189, 103, 206, 129,
  31,  62, 124, 248, 237, 199, 147,  59, 118, 236, 197, 151,  51, 102, 204, 133,
  23,  46,  92, 184, 109, 218, 169,  79, 158,  33,  66, 132,  21,  42,  84, 168,
  77, 154,  41,  82, 164,  85, 170,  73, 146,  57, 114, 228, 213, 183, 115, 230,
 209, 191,  99, 198, 145,  63, 126, 252, 229, 215, 179, 123, 246, 241, 255, 227,
 219, 171,  75, 150,  49,  98, 196, 149,  55, 110, 220, 165,  87, 174,  65, 130,
  25,  50, 100, 200, 141,   7,  14,  28,  56, 112, 224, 221, 167,  83, 166,  81,
 162,  89, 178, 121, 242, 249, 239, 195, 155,  43,  86, 172,  69, 138,   9,  18,
  36,  72, 144,  61, 122, 244, 245, 247, 243, 251, 235, 203, 139,  11,  22,  44,
  88, 176, 125, 250, 233, 207, 131,  27,  54, 108, 216, 173,  71, 142 };

//*************** GF(256) multiply using precalculated table ****************
// the table is generated using primitive polynomial 0x11d
// Parameters:
//   a: first argument
//   b: second argument
// Return value:
//   result of a*b=exp(log(a)+log(b))
unsigned char GF8IntMul(unsigned char a, unsigned char b)
{
	return (a && b) ? Power2Oct[Oct2Power[a] + Oct2Power[b]] : 0;
}

//*************** Calculate result of Result+=Matrix*Vector ****************
// row number of Matrix is fixed to 14, column size and vector size is Length
// Parameters:
//   Matrix: pointer to 14xLength matrix
//   Vector: pointer to column vector
//   Result: pointer to result vector, which size is fixed to 14
//   Length: Matrix column size and Vector size
// Return value:
//   none
#define VECTOR_SIZE 14
void MaxtrixVectorMulAcc(const unsigned char *Matrix, const unsigned char *Vector, unsigned char *Result, int Length)
{
	int i, j;
	const unsigned char *p1 = Matrix, *p2;
	unsigned char sum;

	for (i = 0; i < VECTOR_SIZE; i ++)
	{
		p2 = Vector;
		sum = *Result;
		for (j = 0; j < Length; j ++)
			sum ^= GF8IntMul(*p1++, *p2++);
		*Result++ = sum;
	}
}

#endif
