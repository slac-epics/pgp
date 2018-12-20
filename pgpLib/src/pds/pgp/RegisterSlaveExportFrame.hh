/*
 * RegisterSlaveExportFrame.hh
 *
 *  Created on: Mar 29, 2010
 *      Author: jackp
 */

#ifndef PGPREGISTERSLAVEEXPORTFRAME_HH_
#define PGPREGISTERSLAVEEXPORTFRAME_HH_

#include <stdint.h>
#include <linux/types.h>
#include <fcntl.h>
#include "pds/pgp/PgpRSBits.hh"
#include "pds/pgp/Destination.hh"
#include "pgpcard/PgpCardMod.h"

namespace Pds {
  namespace Pgp {
    class Pgp;

    class RegisterSlaveExportFrame {
      public:
        enum {Success=0, Failure=1};
        enum masks {addrMask=(1<<24)-1};

        RegisterSlaveExportFrame(
            Pgp *pgp,
            PgpRSBits::opcode,   // opcode
            Destination* dest,   // Destination
            unsigned,            // address
            unsigned,            // transaction ID
            uint32_t=0,          // data
            PgpRSBits::waitState=PgpRSBits::notWaiting);

        ~RegisterSlaveExportFrame() {};

      public:
        static unsigned    count;
        static unsigned    errors;

      public:
        unsigned tid()                            {return bits._tid;}
        void waiting(PgpRSBits::waitState w)      {bits._waiting = w;}
        uint32_t* array()                         {return (uint32_t*)&_data;}
        unsigned post(bool use_aes_driver, int _fd, __u32 size, bool pf=false);  // the size of the entire header + payload in number of 32 bit words
        void print(unsigned = 0, unsigned = 4);


      public:
        PgpRSBits bits;
        uint32_t _data;
        uint32_t NotSupposedToCare;
    };

  }
}

#endif /* PGPREGISTERSLAVEEXPORTFRAME_HH_ */
