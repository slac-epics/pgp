/*
 * Pgp.hh
 *
 *  Created on: Apr 5, 2011
 *      Author: jackp
 */

#ifndef PGP_HH_
#define PGP_HH_

#include "pds/pgp/PgpRSBits.hh"
#include "pds/pgp/RegisterSlaveImportFrame.hh"
#include "pds/pgp/RegisterSlaveExportFrame.hh"
#include "pds/pgp/SrpV3.hh"
#include "pds/pgp/Destination.hh"
#include "pgpcard/PgpDriver.h"

namespace Pds {
  namespace Pgp {

    class Pgp {
      public:
        Pgp(int mask, int vcm, bool srpV3, bool printFlag = false);
        virtual ~Pgp();

      public:
        enum {BufferWords=2048, DummyWords=1024*1024};
        enum {Success=0, Failure=1, SelectSleepTimeUSec=10000};
        enum {DirectWrite=0xffffffff};
        enum CardType {G2, G3, DataDev, Unknown};
        Pds::Pgp::RegisterSlaveImportFrame* do_read(unsigned *buffer, int *size);
        Pds::Pgp::RegisterSlaveImportFrame* read(
                          unsigned size = (sizeof(Pds::Pgp::RegisterSlaveImportFrame)/sizeof(uint32_t)));
        unsigned       writeData(
                          Destination*,
                          uint32_t,
                          bool pf=false);
        unsigned       writeDataBlock(
                          Destination*,
                          uint32_t*,
                          unsigned size,
                          bool pf=false);
        unsigned       writeRegister(
                          Destination*,
                          unsigned,
                          uint32_t,
                          bool pf = false,
                          Pds::Pgp::PgpRSBits::waitState = Pds::Pgp::PgpRSBits::notWaiting);
        // NB size should be the size of data to be written in uint32_t's
        unsigned       writeRegisterBlock(
                          Destination*,
                          unsigned,
                          uint32_t*,
                          unsigned size = 1,
                          Pds::Pgp::PgpRSBits::waitState = Pds::Pgp::PgpRSBits::notWaiting,
                          bool pf=false);

        // NB size should be the size of the block requested in uint32_t's
        unsigned      readRegister(
                          Destination*,
                          unsigned,
                          unsigned,
                          uint32_t*,
                          unsigned size=1,
                          bool pf=false);
        unsigned      enableRunTrigger(
                          uint32_t enable,
                          uint32_t runCode,
                          uint32_t runDelay,
                          bool printFlag=false);
        unsigned      readStatus( void * );
        unsigned      flushInputQueue(bool printFlag=false);
        unsigned      lastWriteData(Destination*, uint32_t*);

        void          portOffset(unsigned p) { _portOffset = p;    }
        unsigned      portOffset() const     { return _portOffset; }
        int           fd() const             { return _fd; }
        bool          useSrpV3() const       { return _srpV3; }
        bool          isG3() const           { return _type == G3; }
        CardType      type() const           { return _type; }
        bool          usesPgpDriver() const  { return _type == G2 || _type == G3; }
        bool          usesDataDriver() const { return _type == DataDev; }

      private:
        int               _fd;
        bool              _srpV3;
        CardType          _type;
        unsigned          _readBuffer[BufferWords];
        unsigned          _dummyBuffer[DummyWords];
        unsigned          _portOffset;
        unsigned          _directWrites[4];
        unsigned*         _dummy;
        SrpV3::Protocol*  _proto;
    };
  }
}

#endif /* PGP_HH_ */
