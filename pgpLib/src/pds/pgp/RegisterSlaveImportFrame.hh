/*
 * RegisterSlaveImportFrame.hh
 *
 *  Created on: Mar 30, 2010
 *      Author: jackp
 */

#ifndef PGPREGISTERSLAVEIMPORTFRAME_HH_
#define PGPREGISTERSLAVEIMPORTFRAME_HH_

#include <stdint.h>
#include "pds/pgp/RegisterSlaveExportFrame.hh"
#include "pds/pgp/PgpRSBits.hh"

namespace Pds {
  namespace Pgp {

    class LastBits {
      public:
        unsigned mbz2:        16;      //15:0
        unsigned failed:       1;      //16
        unsigned timeout:      1;      //17
        unsigned mbz1:        14;      //31:18
    };

    class RegisterSlaveImportFrame {
      public:
        RegisterSlaveImportFrame() {};
        ~RegisterSlaveImportFrame() {};

      public:
        unsigned tid()                                {return bits._tid;}
        unsigned addr()                               {return bits._addr;}
        PgpRSBits::waitState waiting() {return (PgpRSBits::waitState)bits._waiting;}
        unsigned lane()                               {return bits._lane;}
        unsigned vc()                                 {return bits._vc;}
        PgpRSBits::opcode opcode()     {return (PgpRSBits::opcode)bits.oc;}
        uint32_t data()                               {return _data;}
        uint32_t* array()                             {return (uint32_t*)&_data;}
        unsigned timeout()                            {return lbits.timeout;}
        unsigned failed()                             {return lbits.failed;}
        unsigned timeout(LastBits* l);
        unsigned failed(LastBits* l);
        void print(unsigned size=4);

      public:
        PgpRSBits    bits;
        uint32_t  _data;
        LastBits  lbits;
    };

  }
}

#endif /* PGPREGISTERSLAVEIMPORTFRAME_HH_ */
