#ifndef Pds_Pgp_PgpMonRegs_hh
#define Pds_Pgp_PgpMonRegs_hh

#include <stdint.h>

namespace Pds {
  namespace Pgp {
    enum PgpRxAxisMonAddrs {
      FrameCnt      = 0x04,
      FrameRate     = 0x0C,
      FrameRateMax  = 0x10,
      FrameRateMin  = 0x14,
      Bandwidth     = 0x18,
      BandwidthMax  = 0x20,
      BandwidthMin  = 0x28,
      FrameSize     = 0x30,
      FrameSizeMax  = 0x34,
      FrameSizeMin  = 0x38,
    };

    typedef struct {
      uint64_t FrameCnt;
      uint32_t FrameRate;
      uint32_t FrameRateMax;
      uint32_t FrameRateMin;
      double   Bandwidth;
      double   BandwidthMax;
      double   BandwidthMin;
      uint32_t FrameSize;
      uint32_t FrameSizeMax;
      uint32_t FrameSizeMin;
    } PgpRxAxisMon;

    enum PgpMonAddrs {
      AutoStatus          = 0x04,
      Loopback            = 0x08,
      SkipInterval        = 0x0C,
      RxPhyAndLink        = 0x10,
      RxRemState          = 0x20,
      RxClockFreqRaw      = 0x2C,
      RxFrameCount        = 0x24,
      RxFrameErrorCount   = 0x28,
      RxCellErrorCount    = 0x14,
      RxLinkDownCount     = 0x18,
      RxLinkErrorCount    = 0x1C,
      RxRemOverflowCount  = 0x40,
      RxOpCodeCount       = 0x30,
      RxOpCodeLast        = 0x34,
      PhyRxData           = 0x100,
      PhyRxState          = 0x108,
      EbRxData            = 0x110,
      EbRxState           = 0x118,
      EbRxOverflow        = 0x11C,
      GearboxAligned      = 0x120,
      PhyRxInitCnt        = 0x130,
      RemLinkData         = 0x138,
      FlowControlTx       = 0x80,
      TxPhyAndLink        = 0x84,
      TxLocState          = 0x8C,
      TxClockFreqRaw      = 0x9C,
      TxFrameCount        = 0x90,
      TxFrameErrorCount   = 0x94,
      TxLocOverflowCount  = 0xB0,
      TxOpCodeCount       = 0xA0,
      TxOpCodeLast        = 0xA4,
    };

    typedef struct {
      uint32_t AutoStatus:1;
      uint32_t Loopback:3;
      uint32_t SkipInterval;
      uint32_t RxPhyActive:1;
      uint32_t RxLocalLinkReady:1;
      uint32_t RxRemLinkReady:1;
      uint32_t RxRemOverflow:16;
      uint32_t RxRemPause:16;
      double   RxClockFrequency;
      uint32_t RxFrameCount:16;
      uint32_t RxFrameErrorCount:8;
      uint32_t RxCellErrorCount:8;
      uint32_t RxLinkDownCount:8;
      uint32_t RxLinkErrorCount:8;
      uint8_t  RxRemOverflowCount[4];
      uint32_t RxOpCodeCount:8;
      uint64_t RxOpCodeNumLast:3;
      uint64_t RxOpCodeDataLast:56;
      uint32_t PhyRxValid:1;
      uint32_t PhyRxHeader:2;
      uint64_t PhyRxData;
      uint32_t EbRxValid:1;
      uint32_t EbRxHeader:2;
      uint32_t EbRxStatus:9;
      uint64_t EbRxData;
      uint32_t EbRxOverflow:1;
      uint32_t EbRxOverflowCnt:8;
      uint32_t GearboxAligned:1;
      uint32_t GearboxAlignCnt:8;
      uint32_t PhyRxInitCnt:4;
      uint64_t RemLinkData:56;
      uint32_t FlowControlDisable:1;
      uint32_t TxDisable:1;
      uint32_t TxPhyActive:1;
      uint32_t TxLinkReady:1;
      uint32_t TxLocOverflow:16;
      uint32_t TxLocPause:16;
      double   TxClockFrequency;
      uint32_t TxFrameCount:16;
      uint32_t TxFrameErrorCount:8;
      uint8_t  TxLocOverflowCount[4];
      uint32_t TxOpCodeCount:8;
      uint64_t TxOpCodeNumLast:3;
      uint64_t TxOpCodeDataLast:56;
    } PgpMon;
  }
}

#endif
