//----------------------------------------------------------------------
// TEManager.h:
//   Tracking engine management functions and definitions
//
//          Copyright (C) 2020-2029 by Jun Mo, All rights reserved.
//
//----------------------------------------------------------------------

#if !defined __TE_MANAGER_H__
#define __TE_MANAGER_H__

#include "CommonDefines.h"
#include "ChannelManager.h"

extern int NominalMeasInterval;
extern int MeasurementInterval;
extern unsigned int MeasIntCounter;
extern unsigned int BasebandTickCount;
extern BB_MEASUREMENT BasebandMeasurement[TOTAL_CHANNEL_NUMBER];

void TEInitialize();
PCHANNEL_STATE IterateChannel(int First);
U32 GetChannelEnable();
PCHANNEL_STATE GetChannelState(int ch);
void UpdateChannels();
PCHANNEL_STATE GetAvailableChannel();
void ReleaseChannel(int ChannelID);
void CohSumInterruptProc();
void MeasurementProc();
int AdjustMeasInterval(void* Param);

#define ENUMERATE_CHANNEL(ChannelMask, ch) \
	for (ChannelMask = GetChannelEnable(); ch = __builtin_ctz(ChannelMask), ChannelMask; ChannelMask &= ~(1 << ch))


#endif // __TE_MANAGER_H__
