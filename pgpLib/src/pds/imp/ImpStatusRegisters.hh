/*
 * ImpStatusRegisters.hh
 *
 *  Created on: April 12, 2013
 *      Author: jackp
 */

#ifndef IMPSTATUSREGISTERS_HH_
#define IMPSTATUSREGISTERS_HH_
#include "pds/pgp/Pgp.hh"

namespace Pds {

  namespace Imp {

//    Register definitions:
//    25 = 0xDEAD0001 usRxReset,
//         0x00000002 is Count Reset,
//         0xDEAD0004 Reset_sft.
//
//    26 = 0x00000001 enable data frames.
//
//    12 =  (31:28) powers okay
//          (27:18) zeroes
//          (17) usRemLinked
//          (16) usLocLinked
//          (15:12) usRxCount
//          (11:8).  UsCellErrCount
//          (7:4).    UsLinkDownCount
//          (3:0).    UsLinkErrCount.
//
//    24 = same as 12, except that the upper 4 bits are zeroes.
//
//    Registers 12 and 24 are read only.
//    Register 12 is returned in the data frames as well as the ranges that are being used.

    enum Addresses {sequenceCountAddr=1, laneStatusAddr=12, chipIdAddr=2,
      versionAddr=23, resetAddr=25, enableTriggersAddr=26, unGateTriggersAddr=28};
    enum Masks {CountResetMask=2, enable=1, disable=0};
    enum resetValues { usRxReset=0xDEAD0001, CountReset=2, SoftReset=0xDEAD0004 };
    enum runControlValues { EnableDataFramesValue=1 };

    class StatusLane {
      public:
        StatusLane() {};
        ~StatusLane() {};
        void print();
      public:
        unsigned usLinkErrCount:     4;  //(3:0)
        unsigned usLinkDownCount:    4;  //(7:4)
        unsigned usCellErrCount:     4;  //(11:8)
        unsigned usRxCount:          4;  //(15:12)
        unsigned usLocLinked:        1;  //16
        unsigned usRemLinked:        1;  //17
        unsigned zeros:             10;  //(27:18)
        unsigned powersOkay:         4;  //(31:28)
    };

    class ImpStatusRegisters {
      public:
        ImpStatusRegisters() {};
        ~ImpStatusRegisters() {};

        int      read();
        void     print();
        unsigned readVersion(uint32_t*);

      public:
        Pds::Pgp::Pgp*  pgp;
        unsigned        version;
        StatusLane      lane;
        unsigned        txCounter;
        unsigned        chipIdRegLow;
        unsigned        chipIdRegHi;
    };
  }
}

#endif /* IMPSTATUSREGISTERS_HH_ */
