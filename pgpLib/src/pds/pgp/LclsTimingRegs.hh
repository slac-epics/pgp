#ifndef Pds_Pgp_LclsTimingRegs_hh
#define Pds_Pgp_LclsTimingRegs_hh

#include <stdint.h>

namespace Pds {
  namespace Pgp {
    enum TimingFrameRxAddrs {
      sofCount      = 0x00,
      eofCount      = 0x04,
      FidCount      = 0x08,
      CrcErrCount   = 0x0C,
      RxClkCount    = 0x10,
      RxRstCount    = 0x14,
      RxDecErrCount = 0x18,
      RxDspErrCount = 0x1C,
      RxStatus      = 0x20,
      MsgDelay      = 0x24,
      TxClkCount    = 0x28,
      BypassCount   = 0x2C,
    };

    typedef struct {
      uint32_t sofCount;
      uint32_t eofCount;
      uint32_t FidCount;
      uint32_t CrcErrCount;
      uint32_t RxClkCount;
      uint32_t RxRstCount;
      uint32_t RxDecErrCount;
      uint32_t RxDspErrCount;
      uint32_t ClearRxCounters:1;
      uint32_t RxLinkUp:1;
      uint32_t RxPolarity:1;
      uint32_t C_RxReset:1;
      uint32_t ClkSel:1;
      uint32_t RxDown:1;
      uint32_t BypassRst:1;
      uint32_t RxPllReset:1;
      uint32_t ModeSel:1;
      uint32_t ModeSelEn:1;
      uint32_t MsgDelay:20;
      uint32_t TxClkCount;
      uint32_t BypassDoneCount:16;
      uint32_t BypassResetCount:16;
    } TimingFrameRx;

    enum TriggerEventBufferAddrs {
      MasterEnable   = 0x00,
      Partition      = 0x04,
      PauseThreshold = 0x08,
      TriggerDelay   = 0x0C,
      TriggerCount   = 0x28,
    };

    typedef struct {
      uint32_t MasterEnable;
      uint32_t TriggerCount;
      uint32_t Partition:3;
      uint32_t PauseThreshold:5;
      uint32_t TriggerDelay;
    } TriggerEventBuffer;
  }
}

#endif
