#ifndef Pds_Pgp_Kcu1500Regs_hh
#define Pds_Pgp_Kcu1500Regs_hh

#include "pds/pgp/LclsTimingRegs.hh"
#include "pds/pgp/PgpMonRegs.hh"
#include "pgpcard/DataDriver.h"

namespace Pds {
  namespace Pgp {
    class Kcu1500Regs {
    public:
      Kcu1500Regs(int device);
      Kcu1500Regs(int device,  uint32_t nlanes);
      Kcu1500Regs(int device,  uint32_t nlanes, uint32_t nvcs);
      uint32_t nlanes() const;
      uint32_t nvcs() const;
      int getTimingFrameRx(TimingFrameRx* timingFrameRx);
      int getTriggerEventBuffer(uint32_t lane, TriggerEventBuffer* trigEventBuffer);
      int getPgpMon(uint32_t lane, PgpMon* pgpMon);
      int getPgpRxAxisMon(uint32_t lane, uint32_t vc, PgpRxAxisMon* pgpRxAxisMon);
    private:
      int       _device;
      uint32_t  _nlanes;
      uint32_t  _nvcs;
    };
  }
}

#endif
