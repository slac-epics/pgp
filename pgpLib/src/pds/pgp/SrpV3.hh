#ifndef SRPV3_HH_
#define SRPV3_HH_

#include <stdint.h>
#include <linux/types.h>
#include <fcntl.h>
#include "pds/pgp/Destination.hh"
#include "pds/pgp/PgpRSBits.hh"
#include "pgpcard/PgpCardMod.h"

namespace Pds {
  namespace Pgp {
    class RegisterSlaveImportFrame;
    namespace SrpV3 {
      class RegisterSlaveFrame {
      public:
        RegisterSlaveFrame(
            PgpRSBits::opcode,   // opcode
            Destination* dest,   // Destination
            uint64_t,            // address
            unsigned,            // transaction ID
            uint32_t=0);         // data

        ~RegisterSlaveFrame() {};

        uint64_t  addr  () const { uint64_t v=_addr_hi; v<<=32; v|=_addr_lo; return v; }
        PgpRSBits::opcode opcode() const { return PgpRSBits::opcode((_word0>>8)&3); }

        unsigned  tid  () const { return _tid; }
        uint32_t* array()                         {return (uint32_t*)(this+1);}
        void      print(unsigned nw=8) const;
      public:
        uint32_t _word0;
        uint32_t _tid;
        uint32_t _addr_lo;
        uint32_t _addr_hi;
        uint32_t _req_size;
      };

      class Protocol {
      public:
        Protocol(int fd, unsigned lane);
      public:
        unsigned      writeRegister( Destination* dest,
                                     unsigned     addr,
                                     uint32_t     val);
        unsigned      readRegister( Destination* dest,
                                    unsigned     addr,
                                    unsigned     tid,
                                    uint32_t*    retp);
        unsigned      writeRegisterBlock( Destination* dest,
                                          unsigned     addr,
                                          uint32_t*    val,
                                          unsigned     size);
        RegisterSlaveImportFrame*  read(unsigned size);
      public:
        int       fd  () const { return _fd; }
        unsigned  lane() const { return _lane; }
      private:
        enum {BufferWords=8192};
        int                    _fd;
        unsigned               _lane;
        unsigned               _readBuffer [BufferWords];
        unsigned               _writeBuffer[BufferWords];
      };
    }
  }
}

#endif
