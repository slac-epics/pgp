/*
 * RegisterSlaveImportFrame.cc
 *
 *  Created on: Mar 30, 2010
 *      Author: jackp
 */

#include "pds/pgp/Pgp.hh"
#include "stdio.h"

namespace Pds {
  namespace Pgp {

    unsigned RegisterSlaveImportFrame::timeout(LastBits* l) {
      return l->timeout;
    }

    unsigned RegisterSlaveImportFrame::failed(LastBits* l) {
      return l->failed;
    }

    void RegisterSlaveImportFrame::print(unsigned size) {
      char ocn[][20] = {"read", "write", "set", "clear"};
      printf("Register Slave Import Frame:\n\t");
      printf("timeout(%s), failed(%s)\n\t",
          timeout(((LastBits*)this)+size-1) ? "true" : "false",
          failed(((LastBits*)this)+size-1) ? "true" : "false");
      printf("lane(%u), vc(%u), opcode(%s), addr(0x%x), waiting(%s), tid(0x%x)\n\t",
          bits._lane, bits._vc, &ocn[bits.oc][0],
          bits._addr, bits._waiting ? "waiting" : "not waiting", bits._tid);
      uint32_t* u = &_data;
      printf("\tdata: ");
      for (unsigned i=0; i<(size-2);) {  //
        printf("0x%04x ", u[i++]);
        if (!(i&7)) printf("\n\t      ");
      }
      printf(" size(%u)\n", size);
//      u = (uint32_t*)this;
//      printf("\t"); for (unsigned i=0;i<size;i++) printf("0x%04x ", u[i]); printf("\n");
    }
  }
}
