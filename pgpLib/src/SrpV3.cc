#include "pds/pgp/SrpV3.hh"
#include "pds/pgp/RegisterSlaveImportFrame.hh"
#include "pgpcard/PgpDriver.h"
#include <new>
#include <stdio.h>

//#define DBUG

using namespace Pds::Pgp::SrpV3;

#define DMA_ERR_FIFO 0x01
#define DMA_ERR_LEN  0x02
#define DMA_ERR_MAX  0x04
#define DMA_ERR_BUS  0x08
#define PGP_ERR_EOFE 0x10

enum {Success=0, Failure=1};

static const unsigned SelectSleepTimeUSec=100000;

static void printError(unsigned error, unsigned dest)
{
  printf("SrpV3::read error eofe(%s), fifo(%s), len(%s), bus%s\n",
         error & PGP_ERR_EOFE ? "true" : "false",
         error & DMA_ERR_FIFO ? "true" : "false",
         error & DMA_ERR_LEN ? "true" : "false",
         error & DMA_ERR_BUS ? "true" : "false");
  printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dest), pgpGetVc(dest));
}

RegisterSlaveFrame::RegisterSlaveFrame(Pds::Pgp::PgpRSBits::opcode oc,
                                       Destination* dest,
                                       uint64_t     addr,
                                       unsigned     tid,
                                       uint32_t     size) :
  _word0   (0x03 | ((oc&3)<<8) | (0xa<<24)),
  _tid     (tid),
  _addr_lo ((addr>> 0)&0xffffffff),
  _addr_hi ((addr>>32)&0xffffffff),
  _req_size(size*4-1)  // bytes-1
{}

void RegisterSlaveFrame::print(unsigned nw) const {
  printf("Read:");
  for(unsigned i=0; i<nw; i++)
    printf(" %08x",reinterpret_cast<const uint32_t*>(this)[i]);
  printf("\n");
}

Protocol::Protocol(int fd, unsigned lane) :
  _fd    (fd),
  _lane  (lane)
{
  memset(_readBuffer, 0, BufferWords*sizeof(unsigned));
}

Pds::Pgp::RegisterSlaveImportFrame* Protocol::read(unsigned size) 
{
#ifdef DBUG
  printf("SrpV3::read fd[%u] this[%p]\n",
         _fd, this);
#endif

  size += sizeof(RegisterSlaveFrame)/sizeof(uint32_t)+1;

  Pds::Pgp::RegisterSlaveImportFrame* ret = 0;

  struct timeval  timeout;
  timeout.tv_sec  = 0;
  timeout.tv_usec = SelectSleepTimeUSec;
  fd_set          fds;
  FD_ZERO(&fds);
  FD_SET(_fd,&fds);
  bool   found  = false;
  struct DmaReadData       pgpCardRx;
  pgpCardRx.flags   = 0;
  pgpCardRx.is32    = 0;
  pgpCardRx.dest    = 0;
  pgpCardRx.size = BufferWords*sizeof(uint32_t);
  pgpCardRx.data    = (uint64_t)(&(_readBuffer));
  pgpCardRx.is32    = sizeof(&pgpCardRx)==4;
  while (found == false) {
    int sret = select(_fd+1,&fds,NULL,NULL,&timeout);
    if (sret > 0) {
      int readRet = ::read(_fd, &pgpCardRx, sizeof(struct DmaReadData));
      if (readRet >= 0) {
        SrpV3::RegisterSlaveFrame* rsf = 
          reinterpret_cast<SrpV3::RegisterSlaveFrame*>(_readBuffer);
        if (rsf->opcode() == PgpRSBits::read) {
          found = true;
          if (pgpCardRx.error) {
            printError(pgpCardRx.error, pgpCardRx.dest);
          } else {
            //            rsf->print(readRet/sizeof(uint32_t));
            // Sometimes we are missing the trailing word
            if (readRet != int(size*sizeof(uint32_t))) {
              printf("SrpV3::read read returned %u, we were looking for %u uint32s\n", readRet, size);
              printf("\tDmaReadData: data(%llx)  dest(%x)  flags(%x)  index(%x)  error(%x)  size(%x)  is32(%x)\n",
                     (unsigned long long) pgpCardRx.data, pgpCardRx.dest, pgpCardRx.flags, pgpCardRx.index, pgpCardRx.error, pgpCardRx.size, pgpCardRx.is32);
              rsf->print();
            } else {
              bool hardwareFailure = false;
              uint32_t* u = (uint32_t*)rsf;
              if (reinterpret_cast<LastBits*>(u+size-1)->failed) {
                printf("SrpV3::read received HW failure\n");
                rsf->print();
                hardwareFailure = true;
              }
              if (reinterpret_cast<LastBits*>(u+size-1)->timeout) {
                printf("SrpV3::read received HW timed out\n");
                rsf->print();
                hardwareFailure = true;
              }
              if (!hardwareFailure) {
                // Format the SrpV0 frame header
                SrpV3::RegisterSlaveFrame rsfv = *rsf;
                ret = reinterpret_cast<Pds::Pgp::RegisterSlaveImportFrame*>(_readBuffer+3);
                Destination dest(pgpCardRx.dest);
                ret->bits._vc      = dest.vc();
                ret->bits.mbz      = 0;
                ret->bits._lane    = dest.lane();
                ret->bits._waiting = PgpRSBits::Waiting;
                ret->bits._tid     = rsfv.tid();
                ret->bits._addr    = rsfv.addr()&0xffffff;
                ret->bits.dnc      = 0;
                ret->bits.oc       = rsfv.opcode();
              }
            }
          }
        }
      } else {
        perror("SrpV3::read ERROR ! ");
        found = true;
      }
    } else {
      found = true;  // we might as well give up!
      if (sret < 0) {
        perror("SrpV3::read select error: ");
        printf("\tpgpLane(%u), pgpVc(%u)\n", pgpCardRx.dest>>2, pgpCardRx.dest&3);
      } else {
        printf("SrpV3::read select timed out! fd[%u]\n", _fd);
      }
    }
  }
  return ret;
}

unsigned Protocol::writeRegister(Destination* dest,
                                 unsigned     addr,
                                 uint32_t     data) {
#ifdef DBUG
  printf("SrpV3::writeRegister dest_lane[%x] addr[%x] fd[%u] proto_lane[%u] this[%p]\n",
         dest->lane(), addr, _fd, _lane, this);
#endif

  unsigned tid = 0x6969;
  unsigned size = 1;
  SrpV3::RegisterSlaveFrame* hdr = 
    new (_writeBuffer) SrpV3::RegisterSlaveFrame(PgpRSBits::write, 
                                                 dest, 
                                                 addr, 
                                                 tid, 
                                                 size);
  memcpy(hdr+1, &data, size*sizeof(uint32_t));

  // post
  // Wait for write ready
  struct timeval  timeout;
  timeout.tv_sec=0;
  timeout.tv_usec=100000;
  fd_set          fds;
  FD_ZERO(&fds);
  FD_SET(_fd,&fds);
    
  struct DmaWriteData  pgpCardTx;
  pgpCardTx.is32   = (sizeof(&pgpCardTx) == 4);
  pgpCardTx.flags  = 0;
  pgpCardTx.dest   = dest->vc() | ((dest->lane() + _lane)<<2);
  pgpCardTx.index  = 0;
  pgpCardTx.size   = sizeof(*hdr) + size*sizeof(uint32_t);
  pgpCardTx.data   = (__u64)hdr;

  int ret;
  if ((ret = select( _fd+1, NULL, &fds, NULL, &timeout)) <= 0) {
    printf("SrpV3::writeRegister select: fd[%u]\n", _fd);
    if (ret < 0) {
      perror("SrpV3 post select error: ");
    } else {
      printf("SrpV3 post select timed out\n");
    }
    return Failure;
  }
  ::write(_fd, &pgpCardTx, sizeof(pgpCardTx));
  return Success;
}

unsigned Protocol::readRegister(Destination* dest,
                                unsigned addr,
                                unsigned tid,
                                uint32_t* retp) {
#ifdef DBUG
  printf("SrpV3::readRegister dest_lane[%u] addr[%u] fd[%u] this[%p]\n",
         dest->lane(), addr, _fd, this);
#endif

  unsigned size = 1;
  SrpV3::RegisterSlaveFrame hdr(PgpRSBits::read, dest, addr, tid, size);
  // post
  // Wait for write ready
  struct timeval  timeout;
  timeout.tv_sec=0;
  timeout.tv_usec=100000;
  fd_set          fds;
  FD_ZERO(&fds);
  FD_SET(_fd,&fds);
    
  struct DmaWriteData  pgpCardTx;
  pgpCardTx.is32   = (sizeof(&pgpCardTx) == 4);
  pgpCardTx.flags  = 0;
  pgpCardTx.dest   = dest->vc() | ((dest->lane() + _lane)<<2);
  pgpCardTx.index  = 0;
  pgpCardTx.size   = sizeof(hdr);
  pgpCardTx.data   = (__u64)&hdr;

  int ret;
  if ((ret = select( _fd+1, NULL, &fds, NULL, &timeout)) <= 0) {
    if (ret < 0) {
      perror("SrpV3 post select error: ");
    } else {
      printf("SrpV3 post select timed out: fd[%u] dest[%x]\n", _fd, pgpCardTx.dest);
    }
    return Failure;
  }
  ::write(_fd, &pgpCardTx, sizeof(pgpCardTx));
    
  unsigned errorCount = 0;
  while (true) {
    RegisterSlaveImportFrame* rsif = this->read(size);
    if (rsif == 0) {
      printf("SrpV3::readRegister _pgp->read failed! dest[%x] fd[%d] this[%p]\n",
             pgpCardTx.dest, _fd, this);
      return Failure;
    }
    if ((addr&0xffffff) != rsif->addr()) {  // Can only test lowest 24 bits of addr
      printf("SrpV3::readRegister out of order response lane=%u, vc=%u, addr=0x%x(0x%x), tid=0x%x(0x%x), errorCount=%u\n",
             dest->lane(), dest->vc(), addr, rsif->addr(), tid, rsif->tid(), ++errorCount);
      if (errorCount > 5) return Failure;
    } else {  // copy the data
      memcpy(retp, rsif->array(), size * sizeof(uint32_t));
      return Success;
    }
  }
}

unsigned Protocol::writeRegisterBlock(Destination* dest,
                                      unsigned     addr,
                                      uint32_t*    data,
                                      unsigned     size) {
#ifdef DBUG
  printf("SrpV3::writeRegister dest_lane[%x] addr[%x] fd[%u] proto_lane[%u] this[%p]\n",
         dest->lane(), addr, _fd, _lane, this);
#endif

  unsigned tid = 0x6970;
  SrpV3::RegisterSlaveFrame* hdr =
    new (_writeBuffer) SrpV3::RegisterSlaveFrame(PgpRSBits::write,
                                                 dest,
                                                 addr,
                                                 tid,
                                                 size);
  memcpy(hdr+1, data, size*sizeof(uint32_t));

  // post
  // Wait for write ready
  struct timeval  timeout;
  timeout.tv_sec=0;
  timeout.tv_usec=100000;
  fd_set          fds;
  FD_ZERO(&fds);
  FD_SET(_fd,&fds);

  struct DmaWriteData  pgpCardTx;
  pgpCardTx.is32   = (sizeof(&pgpCardTx) == 4);
  pgpCardTx.flags  = 0;
  pgpCardTx.dest   = dest->vc() | ((dest->lane() + _lane)<<2);
  pgpCardTx.index  = 0;
  pgpCardTx.size   = sizeof(*hdr) + size*sizeof(uint32_t);
  pgpCardTx.data   = (__u64)hdr;

  int ret;
  if ((ret = select( _fd+1, NULL, &fds, NULL, &timeout)) <= 0) {
    printf("SrpV3::writeRegister select: fd[%u]\n", _fd);
    if (ret < 0) {
      perror("SrpV3 post select error: ");
    } else {
      printf("SrpV3 post select timed out\n");
    }
    return Failure;
  }
  ::write(_fd, &pgpCardTx, sizeof(pgpCardTx));
  return Success;
}
