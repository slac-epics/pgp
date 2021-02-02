#include "pds/pgp/Kcu1500Regs.hh"

using namespace Pds::Pgp;

/* Default number of lanes */
static const uint32_t DefaultNumLanes = 4;
static const uint32_t DefaultNumVCs = 4;

static const uint32_t RegSize = sizeof(uint32_t);

/* Top Level Register Offsets */
static const uint32_t AxiPcieCoreOffset = 0x00000000;
static const uint32_t ApplicationOffset = 0x00C00000;
static const uint32_t Kcu1500HsioOffset = 0x00800000;
/* Hsio Offsets -> Kcu1500HsioOffset */
static const uint32_t PgpAxiOffset           = 0x00000000;
static const uint32_t Pgp3AxiLStride         = 0x00010000; // per lane
static const uint32_t Pgp2bAxiStride         = 0x00010000; // per lane
static const uint32_t AxiStreamMonOffset     = 0x00002000;
static const uint32_t AxiStreamMonStride     = 0x00010000; // per lane
static const uint32_t AxiStreamMonChanStride = 0x00000040; // per vc
static const uint32_t TimingRxOffset         = 0x00100000;
/* TimingRx Offsets -> TimingRxOffset */
static const uint32_t GthRxAlignCheckOffset     = 0x00000000;
static const uint32_t GthRxAlignCheckStride     = 0x00010000; // 2 or them
static const uint32_t TimingPhyMonitorOffset    = 0x00020000;
static const uint32_t XpmMiniWrapperOffset      = 0x00030000;
static const uint32_t TriggerEventManagerOffset = 0x00040000;
static const uint32_t TimingFrameRxOffset       = 0x00080000;
/* TriggerEventManager Offsets -> TimingRxOffset */
static const uint32_t EvrV2CoreTriggersOffset   = 0x00000000;
static const uint32_t XpmMessageAlignerOffset   = 0x00008000;
static const uint32_t TriggerEventBufferOffset  = 0x00009000;
static const uint32_t TriggerEventBufferStride  = 0x00000100; // per lane


/* Calculated Offsets */
static const uint32_t PgpAxiStart = Kcu1500HsioOffset + PgpAxiOffset;
static const uint32_t AxiStreamMonStart = Kcu1500HsioOffset + AxiStreamMonOffset;
static const uint32_t TimingRxStart = Kcu1500HsioOffset + TimingRxOffset;
/* TriggerEventManager -> subnodes*/
static const uint32_t TriggerEventManagerStart = TimingRxStart + TriggerEventManagerOffset;
static const uint32_t TriggerEventBufferStart = TriggerEventManagerStart + TriggerEventBufferOffset;
/* TimingFrameRx */
static const uint32_t TimingFrameRxStart = TimingRxStart + TimingFrameRxOffset;

static inline uint32_t getbits(uint32_t bits, uint32_t index, uint32_t num=1)
{
  return (bits & (((1<<num) - 1) << index)) >> index;
}

static inline uint64_t getbits(uint64_t bits, uint64_t index, uint64_t num=1)
{
  return (bits & (((1<<num) - 1) << index)) >> index;
}

static inline ssize_t dmaReadRegister64(int32_t fd, uint32_t address, uint64_t *data)
{
  int ret = 0;
  uint32_t low = 0;
  uint32_t high = 0;

  ret += dmaReadRegister(fd, address, &low);
  ret += dmaReadRegister(fd, address + RegSize, &high);

  if ( data != NULL ) *data = ((uint64_t) high) << 32 | ((uint64_t) low);
  
  return ret;   
}

Kcu1500Regs::Kcu1500Regs(int device) :
  _device(device),
  _nlanes(DefaultNumLanes),
  _nvcs(DefaultNumVCs)
{
}

Kcu1500Regs::Kcu1500Regs(int device, uint32_t nlanes) :
  _device(device),
  _nlanes(nlanes),
  _nvcs(DefaultNumVCs)
{}

Kcu1500Regs::Kcu1500Regs(int device, uint32_t nlanes, uint32_t nvcs) :
  _device(device),
  _nlanes(nlanes),
  _nvcs(nvcs)
{}

uint32_t Kcu1500Regs::nlanes() const
{
  return _nlanes;
}

uint32_t Kcu1500Regs::nvcs() const
{
  return _nvcs;
}

int Kcu1500Regs::getTimingFrameRx(TimingFrameRx* timingFrameRx)
{
  if (timingFrameRx) {
    int ret = 0;
    uint32_t tmp;
    ret += dmaReadRegister(_device, TimingFrameRxStart + sofCount,      &timingFrameRx->sofCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + eofCount,      &timingFrameRx->eofCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + FidCount,      &timingFrameRx->FidCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + CrcErrCount,   &timingFrameRx->CrcErrCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + RxClkCount,    &timingFrameRx->RxClkCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + RxRstCount,    &timingFrameRx->RxRstCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + RxDecErrCount, &timingFrameRx->RxDecErrCount);
    ret += dmaReadRegister(_device, TimingFrameRxStart + RxDspErrCount, &timingFrameRx->RxDspErrCount);
    // unpack the RxStatus register
    ret += dmaReadRegister(_device, TimingFrameRxStart + RxStatus, &tmp);
    timingFrameRx->ClearRxCounters = getbits(tmp, 0);
    timingFrameRx->RxLinkUp = getbits(tmp, 1);
    timingFrameRx->RxPolarity = getbits(tmp, 2);
    timingFrameRx->C_RxReset = getbits(tmp, 3);
    timingFrameRx->ClkSel = getbits(tmp, 4);
    timingFrameRx->RxDown = getbits(tmp, 5);
    timingFrameRx->BypassRst = getbits(tmp, 6);
    timingFrameRx->RxPllReset = getbits(tmp, 7);
    timingFrameRx->ModeSel = getbits(tmp, 9);
    timingFrameRx->ModeSelEn = getbits(tmp, 10);
    // unpack the MsgDelay register
    ret += dmaReadRegister(_device, TimingFrameRxStart + MsgDelay, &tmp);
    timingFrameRx->MsgDelay = getbits(tmp, 0, 20);
    ret += dmaReadRegister(_device, TimingFrameRxStart + TxClkCount, &timingFrameRx->TxClkCount);
    // unpack the BypassCount register
    ret += dmaReadRegister(_device, TimingFrameRxStart + BypassCount, &tmp);
    timingFrameRx->BypassDoneCount = getbits(tmp, 0, 16);
    timingFrameRx->BypassResetCount = getbits(tmp, 16, 16);
    return ret;
  } else {
    return -1;
  }
}

int Kcu1500Regs::getTriggerEventBuffer(uint32_t lane, TriggerEventBuffer* trigEventBuffer)
{
  if (trigEventBuffer && lane < _nlanes) {
    int ret = 0;
    uint32_t tmp;
    uint32_t base = TriggerEventBufferStart + TriggerEventBufferStride * lane;
    // extract the master enable bit
    ret += dmaReadRegister(_device, base + MasterEnable, &tmp);
    trigEventBuffer->MasterEnable = getbits(tmp, 0);
    ret += dmaReadRegister(_device, base + TriggerCount, &trigEventBuffer->TriggerCount);
    // extract partition bits
    ret += dmaReadRegister(_device, base + Partition, &tmp);
    trigEventBuffer->Partition = getbits(tmp, 0, 3);
    // extract pause threshold bits
    ret += dmaReadRegister(_device, base + PauseThreshold, &tmp);
    trigEventBuffer->PauseThreshold = getbits(tmp, 0, 5);
    ret += dmaReadRegister(_device, base + TriggerDelay, &trigEventBuffer->TriggerDelay);
    return ret;
  } else {
    return -1;
  }
}

int Kcu1500Regs::getPgpMon(uint32_t lane, PgpMon* pgpMon)
{
  if(pgpMon && lane < _nlanes) {
    int ret = 0;
    uint32_t tmp;
    uint64_t tmp64;
    uint32_t base = PgpAxiStart + Pgp3AxiLStride * lane;
    // extract autostatus bit
    ret += dmaReadRegister(_device, base + AutoStatus, &tmp);
    pgpMon->AutoStatus = getbits(tmp, 0);
    // extract lookback bit
    ret += dmaReadRegister(_device, base + Loopback, &tmp);
    pgpMon->Loopback = getbits(tmp, 0, 3);
    ret += dmaReadRegister(_device, base + SkipInterval, &pgpMon->SkipInterval);
    // extract rx link bits
    ret += dmaReadRegister(_device, base + RxPhyAndLink, &tmp);
    pgpMon->RxPhyActive = getbits(tmp, 0);
    pgpMon->RxLocalLinkReady = getbits(tmp, 1);
    pgpMon->RxRemLinkReady = getbits(tmp, 2);
    // extract RxRemOverflow and RxRemPause
    ret += dmaReadRegister(_device, base + RxRemState, &tmp);
    pgpMon->RxRemOverflow = getbits(tmp, 0, _nvcs);
    pgpMon->RxRemPause = getbits(tmp, 16, _nvcs);
    // extract the RxClockFreq
    ret += dmaReadRegister(_device, base + RxClockFreqRaw, &tmp);
    pgpMon->RxClockFrequency = 1.0e-6 * tmp;
    // extrace the RxFrameCount
    ret += dmaReadRegister(_device, base + RxFrameCount, &tmp);
    pgpMon->RxFrameCount = getbits(tmp, 0, 16);
    // rx error bits
    ret += dmaReadRegister(_device, base + RxFrameErrorCount, &tmp);
    pgpMon->RxFrameErrorCount = getbits(tmp, 0, 8);
    ret += dmaReadRegister(_device, base + RxCellErrorCount, &tmp);
    pgpMon->RxCellErrorCount = getbits(tmp, 0, 8);
    ret += dmaReadRegister(_device, base + RxLinkDownCount, &tmp);
    pgpMon->RxLinkDownCount = getbits(tmp, 0, 8);
    ret += dmaReadRegister(_device, base + RxLinkErrorCount, &tmp);
    pgpMon->RxLinkErrorCount = getbits(tmp, 0, 8);
    ret += dmaReadRegister(_device, base + RxOpCodeCount, &tmp);
    pgpMon->RxOpCodeCount = getbits(tmp, 0, 8);
    // rx overflows for each vc
    for (uint32_t i=0; i<_nvcs; i++) {
      ret += dmaReadRegister(_device, base + RxRemOverflowCount + i*4, &tmp);
      pgpMon->RxRemOverflowCount[i] = getbits(tmp, 0, 8);
    }
    // extract RxOpCodeLast
    ret += dmaReadRegister64(_device, base + RxOpCodeLast, &tmp64);
    pgpMon->RxOpCodeDataLast = getbits(tmp64, 0, 56);
    pgpMon->RxOpCodeNumLast = getbits(tmp64, 56, 3);
    // extract PhyRxValid
    ret += dmaReadRegister(_device, base + PhyRxState, &tmp);
    pgpMon->PhyRxValid = getbits(tmp, 2);
    pgpMon->PhyRxHeader = getbits(tmp, 0, 2);
    ret += dmaReadRegister64(_device, base + PhyRxData, &pgpMon->PhyRxData);
    // extract RbRxValid
    ret += dmaReadRegister(_device, base + EbRxState, &tmp);
    pgpMon->EbRxValid = getbits(tmp, 2);
    pgpMon->EbRxHeader = getbits(tmp, 0, 2);
    pgpMon->EbRxStatus = getbits(tmp, 3, 9);
    ret += dmaReadRegister64(_device, base + EbRxData, &pgpMon->EbRxData);
    // extract EbRxOverflow
    ret += dmaReadRegister(_device, base + EbRxOverflow, &tmp);
    pgpMon->EbRxOverflow = getbits(tmp, 0);
    pgpMon->EbRxOverflowCnt = getbits(tmp, 1, 8);
    // extract GearboxAligned
    ret += dmaReadRegister(_device, base + GearboxAligned, &tmp);
    pgpMon->GearboxAligned = getbits(tmp, 0);
    pgpMon->GearboxAlignCnt = getbits(tmp, 8, 8);
    // extract PhyRxInitCnt
    ret += dmaReadRegister(_device, base + PhyRxInitCnt, &tmp);
    pgpMon->PhyRxInitCnt = getbits(tmp, 0, 4);
    // extract RemLinkData
    ret += dmaReadRegister64(_device, base + RemLinkData, &tmp64);
    pgpMon->RemLinkData = getbits(tmp64, 0, 56);
    // extract FlowControl/TxDisable
    ret += dmaReadRegister(_device, base + FlowControlTx, &tmp);
    pgpMon->FlowControlDisable = getbits(tmp, 0);
    pgpMon->TxDisable = getbits(tmp, 1);
    // extract TxPhyActive and TxLinkReady
    ret += dmaReadRegister(_device, base + TxPhyAndLink, &tmp);
    pgpMon->TxPhyActive = getbits(tmp, 1);
    pgpMon->TxLinkReady = getbits(tmp, 0);
    // extract TxLocOverflow and TxLocPause
    ret += dmaReadRegister(_device, base + TxLocState, &tmp);
    pgpMon->TxLocOverflow = getbits(tmp, 0, _nvcs);
    pgpMon->TxLocPause = getbits(tmp, 16, _nvcs);
    // extract the RxClockFreq
    ret += dmaReadRegister(_device, base + TxClockFreqRaw, &tmp);
    pgpMon->TxClockFrequency = 1.0e-6 * tmp;
    // extrace the TxFrameCount
    ret += dmaReadRegister(_device, base + TxFrameCount, &tmp);
    pgpMon->TxFrameCount = getbits(tmp, 0, 16);
    // extrace TxFrameErrorCount
    ret += dmaReadRegister(_device, base + TxFrameErrorCount, &tmp);
    pgpMon->TxFrameErrorCount = getbits(tmp, 0, 8);
    // rx overflows for each vc
    for (uint32_t i=0; i<_nvcs; i++) {
      ret += dmaReadRegister(_device, base + TxLocOverflowCount + i*4, &tmp);
      pgpMon->TxLocOverflowCount[i] = getbits(tmp, 0, 8);
    }
    ret += dmaReadRegister(_device, base + TxOpCodeCount, &tmp);
    pgpMon->TxOpCodeCount = getbits(tmp, 0, 8);
    // extract TxOpCodeLast
    ret += dmaReadRegister64(_device, base + TxOpCodeLast, &tmp64);
    pgpMon->TxOpCodeDataLast = getbits(tmp64, 0, 56);
    pgpMon->TxOpCodeNumLast = getbits(tmp64, 56, 3);
    return ret;
  } else {
    return -1;
  }
}

int Kcu1500Regs::getPgpRxAxisMon(uint32_t lane, uint32_t vc, PgpRxAxisMon* pgpRxAxisMon)
{
  if(pgpRxAxisMon && lane < _nlanes && vc < _nvcs) {
    int ret = 0;
    uint64_t tmp64;
    uint32_t base = AxiStreamMonStart + AxiStreamMonStride * lane + AxiStreamMonChanStride * vc;
    // FrameCnt
    ret += dmaReadRegister64(_device, base + FrameCnt, &pgpRxAxisMon->FrameCnt);
    // FrameRate
    ret += dmaReadRegister(_device, base + FrameRate, &pgpRxAxisMon->FrameRate);
    ret += dmaReadRegister(_device, base + FrameRateMax, &pgpRxAxisMon->FrameRateMax);
    ret += dmaReadRegister(_device, base + FrameRateMin, &pgpRxAxisMon->FrameRateMin);
    // Bandwidth
    ret += dmaReadRegister64(_device, base + Bandwidth, &tmp64);
    pgpRxAxisMon->Bandwidth = 8.e-6 * tmp64;
    ret += dmaReadRegister64(_device, base + BandwidthMax, &tmp64);
    pgpRxAxisMon->BandwidthMax = 8.e-6 * tmp64;
    ret += dmaReadRegister64(_device, base + BandwidthMin, &tmp64);
    pgpRxAxisMon->BandwidthMin = 8.e-6 * tmp64;
    // FrameSize
    ret += dmaReadRegister(_device, base + FrameSize, &pgpRxAxisMon->FrameSize);
    ret += dmaReadRegister(_device, base + FrameSizeMax, &pgpRxAxisMon->FrameSizeMax);
    ret += dmaReadRegister(_device, base + FrameSizeMin, &pgpRxAxisMon->FrameSizeMin);
    return ret;
  } else {
    return -1;
  }
}
