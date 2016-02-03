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
#include "pds/pgp/Destination.hh"
#include "pgpcard/PgpCardMod.h"

namespace Pds {
  namespace Pgp {

    class Pgp {
      public:
        Pgp(int, bool printFlag = false);
        virtual ~Pgp();

      public:
        enum {BufferWords=2048};
        enum {Success=0, Failure=1, SelectSleepTimeUSec=10000};
        Pds::Pgp::RegisterSlaveImportFrame* do_read(unsigned *buffer, int *size);
        Pds::Pgp::RegisterSlaveImportFrame* read(
                          unsigned size = (sizeof(Pds::Pgp::RegisterSlaveImportFrame)/sizeof(uint32_t)));
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
        unsigned      readStatus( PgpCardStatus* );
        unsigned      stopPolling();
        int      IoctlCommand(unsigned command, unsigned arg = 0);

        void          portOffset(unsigned p) { _portOffset = p;    }
        unsigned      portOffset()           { return _portOffset; }
        int           fd()                   { return _fd; }

      private:
        int        _fd;
        unsigned   _readBuffer[BufferWords];
        unsigned   _portOffset;
    };
  }
}

#endif /* PGP_HH_ */
