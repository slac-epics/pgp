//---------------------------------------------------------------------------------
// Title         : Kernel Module For PGP To PCI Bridge Card
// Project       : PGP To PCI-E Bridge Card
//---------------------------------------------------------------------------------
// File          : PgpCardMod.h
// Author        : Ryan Herbst, rherbst@slac.stanford.edu
// Created       : 05/18/2010
//---------------------------------------------------------------------------------
//
//---------------------------------------------------------------------------------
// Copyright (c) 2010 by SLAC National Accelerator Laboratory. All rights reserved.
//---------------------------------------------------------------------------------
// Modification history:
// 05/18/2010: created.
//---------------------------------------------------------------------------------
#ifndef PGP_CARD_MOD_H
#define PGP_CARD_MOD_H

#include <linux/types.h>

#ifndef NUMBER_OF_LANES
#define NUMBER_OF_LANES (4)
#endif

// Return values
#define SUCCESS 0
#define ERROR   -1

// Scratchpad write value
#define SPAD_WRITE 0x55441122

// TX Structure
typedef struct {

   __u32  model; // large=8, small=4
   __u32  cmd; // ioctl commands
   __u32* data;
   // Lane & VC
   __u32  pgpLane;
   __u32  pgpVc;

   // Data
   __u32   size;  // dwords

} PgpCardTx;

// RX Structure
typedef struct {
    __u32   model; // large=8, small=4
    __u32   maxSize; // dwords
    __u32*  data;

   // Lane & VC
   __u32    pgpLane;
   __u32    pgpVc;

   // Data
   __u32   rxSize;  // dwords

   // Error flags
   __u32   eofe;
   __u32   fifoErr;
   __u32   lengthErr;

} PgpCardRx;

typedef struct {
    __u32 PgpLoopBack;
    __u32 PgpRxReset;
    __u32 PgpTxReset;
    __u32 PgpLocLinkReady;
    __u32 PgpRemLinkReady;
    __u32 PgpRxReady;
    __u32 PgpTxReady;
    __u32 PgpRxCount;
    __u32 PgpCellErrCnt;
    __u32 PgpLinkDownCnt;
    __u32 PgpLinkErrCnt;
    __u32 PgpFifoErr;
} PgpCardLinkStatus;

// G2 Status Structure
typedef struct {

   // General Status
   __u32 Version;

   // Scratchpad
   __u32 ScratchPad;

   // PCI Status & Control Registers
   __u32 PciCommand;
   __u32 PciStatus;
   __u32 PciDCommand;
   __u32 PciDStatus;
   __u32 PciLCommand;
   __u32 PciLStatus;
   __u32 PciLinkState;
   __u32 PciFunction;
   __u32 PciDevice;
   __u32 PciBus;

   PgpCardLinkStatus PgpLink[4];

   // TX Descriptor Status
   __u32 TxDma3AFull;
   __u32 TxDma2AFull;
   __u32 TxDma1AFull;
   __u32 TxDma0AFull;
   __u32 TxReadReady;
   __u32 TxRetFifoCount;
   __u32 TxCount;
   __u32 TxBufferCount;
   __u32 TxRead;

   // RX Descriptor Status
   __u32 RxFreeEmpty;
   __u32 RxFreeFull;
   __u32 RxFreeValid;
   __u32 RxFreeFifoCount;
   __u32 RxReadReady;
   __u32 RxRetFifoCount;
   __u32 RxCount;
   __u32 RxBufferCount;
   __u32 RxWrite[8];
   __u32 RxRead[8];

} PgpCardStatus;

// Status Structure
typedef struct {

   // General Status
   __u32 Version;
   __u32 SerialNumber[2];
   __u32 ScratchPad;
   __u32 BuildStamp[64];
   __u32 CountReset;
   __u32 CardReset;

   // PCI Status & Control Registers
   __u32 PciCommand;
   __u32 PciStatus;
   __u32 PciDCommand;
   __u32 PciDStatus;
   __u32 PciLCommand;
   __u32 PciLStatus;
   __u32 PciLinkState;
   __u32 PciFunction;
   __u32 PciDevice;
   __u32 PciBus;
   __u32 PciBaseHdwr;
   __u32 PciBaseLen;

   // PGP Status
   __u32 PpgRate;
   __u32 PgpLoopBack[8];
   __u32 PgpTxReset[8];
   __u32 PgpRxReset[8];
   __u32 PgpTxPllRst[2];
   __u32 PgpRxPllRst[2];
   __u32 PgpTxPllRdy[2];
   __u32 PgpRxPllRdy[2];
   __u32 PgpLocLinkReady[8];
   __u32 PgpRemLinkReady[8];
   __u32 PgpRxCount[8][4];
   __u32 PgpCellErrCnt[8];
   __u32 PgpLinkDownCnt[8];
   __u32 PgpLinkErrCnt[8];
   __u32 PgpFifoErrCnt[8];

   // EVR Status & Control Registers
   __u32 EvrRunCode[8];
   __u32 EvrAcceptCode[8];
   __u32 EvrEnHdrCheck[8][4];
   __u32 EvrEnable;
   __u32 EvrReady;
   __u32 EvrReset;
   __u32 EvrLaneStatus;
   __u32 EvrLaneEnable;
   __u32 EvrLaneMode;
   __u32 EvrPllRst;
   __u32 EvrErrCnt;
   __u32 EvrFiducial;
   __u32 EvrRunDelay[8];
   __u32 EvrAcceptDelay[8];
   __u32 EvrRunCodeCount[8];
   __u32 EvrLutDropCount[8];
   __u32 EvrAcceptCount[8];
   __u32 EvrLaneFiducials[8];

   // RX Descriptor Status
   __u32 RxFreeFull[8];
   __u32 RxFreeValid[8];
   __u32 RxFreeFifoCount[8];
   __u32 RxReadReady;
   __u32 RxRetFifoCount;
   __u32 RxCount;
   __u32 RxWrite[8];
   __u32 RxRead[8];

   // TX Descriptor Status
   __u32 TxDmaAFull[8];
   __u32 TxReadReady;
   __u32 TxRetFifoCount;
   __u32 TxCount;
   __u32 TxRead;

} PgpCardG3Status;

//////////////////////
// IO Control Commands
//////////////////////

#define IOCTL_Normal_Write            1
#define IOCTL_Write_Scratch           2
#define IOCTL_Set_Debug               3
#define IOCTL_Count_Reset             4
// Set Loopback, Pass PGP Channel As Arg
#define IOCTL_Set_Loop                5
#define IOCTL_Clr_Loop                6
// Set RX Reset, Pass PGP Channel As Arg
#define IOCTL_Set_Rx_Reset            7
#define IOCTL_Clr_Rx_Reset            8
// Set TX Reset, Pass PGP Channel As Arg
#define IOCTL_Set_Tx_Reset            9
#define IOCTL_Clr_Tx_Reset           10
// Set EVR configuration
#define IOCTL_Evr_Enable             11
#define IOCTL_Evr_Disable            12
#define IOCTL_Evr_Set_Reset          13
#define IOCTL_Evr_Clr_Reset          14
#define IOCTL_Evr_Set_PLL_RST        15
#define IOCTL_Evr_Clr_PLL_RST        16
#define IOCTL_Evr_LaneModeFiducial   17
#define IOCTL_Evr_LaneModeNoFiducial 18
#define IOCTL_Evr_Fiducial           19
#define IOCTL_Evr_LaneEnable         20
#define IOCTL_Evr_LaneDisable        21
#define IOCTL_Evr_RunMask            22
#define IOCTL_Evr_RunCode            23
#define IOCTL_Evr_RunDelay           24
#define IOCTL_Evr_AcceptCode         25
#define IOCTL_Evr_AcceptDelay        26
#define IOCTL_Evr_En_Hdr_Check       27
// Read Status, Pass PgpG3CardStatus pointer as arg
#define IOCTL_Read_Status            28
// Dump debug
#define IOCTL_Dump_Debug             29
#define IOCTL_Clear_Open_Clients     30
#define IOCTL_Clear_Polling          31
#define IOCTL_ClearFrameCounter      32
#define IOCTL_Add_More_Ports         33
#define IOCTL_Set_VC_Mask            34
#define IOCTL_Show_Version           35
#define IOCTL_Clear_Run_Count        36
#define IOCTL_End_Of_List            37
#endif
