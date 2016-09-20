/*
 * Pgp.cc
 *
 *  Created on: Apr 5, 2011
 *      Author: jackp
 */

#include "pds/pgp/Pgp.hh"
#include <stdlib.h> // For exit
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <new>

namespace Pds {

  namespace Pgp {

    // MCB - No longer passing a fd, now just the port/lane/VC/G3 information.
    Pgp::Pgp(int f, int vcm, int G3, bool pf) {
      unsigned ports = (f >> 4) & 0xf;
      char devName[128];
      char err[128];
      _G3 = G3;
      if (ports == 0 && !G3)
        ports = 15;
      sprintf(devName, "/dev/pgpcard%s_%u_%u", G3 ? "G3" : "", f & 0xf, ports);
      printf("Opening %s\n", devName);
      _fd = open( devName,  O_RDWR | O_NONBLOCK);
      if (_fd < 0) {
        sprintf(err, "Pgp::Pgp() opening %s failed", devName);
        perror(err);
        ::exit(-1);
      }
      if (G3) {
        _portOffset = ports - 1;
      } else {
        _portOffset = 0;
        while ((((ports>>_portOffset) & 1) == 0) && (_portOffset < 4)) {
          _portOffset += 1;
        }
      }
      if (vcm)
        IoctlCommand(IOCTL_Set_VC_Mask, (vcm<<8) | _portOffset);
      // End MCB changes.
      if (pf) printf("Pgp::Pgp(fd(%d)), offset(%u)\n", _fd, _portOffset);
      for (int i=0; i<BufferWords; i++) _readBuffer[i] = i;
    }

    Pgp::~Pgp() {}

    Pds::Pgp::RegisterSlaveImportFrame* Pgp::do_read(unsigned *buffer, int *size) {
      Pds::Pgp::RegisterSlaveImportFrame* ret = (Pds::Pgp::RegisterSlaveImportFrame*)buffer;
      memset(_readBuffer, 0, sizeof(_readBuffer));
      PgpCardRx       pgpCardRx;
      pgpCardRx.model   = sizeof(&pgpCardRx);
      pgpCardRx.maxSize = *size;
      pgpCardRx.data    = (__u32*)buffer;
      if ((*size = ::read(_fd, &pgpCardRx, sizeof(PgpCardRx))) >= 0) {
          if (pgpCardRx.eofe || pgpCardRx.fifoErr || pgpCardRx.lengthErr) {
              printf("Pgp::do_read error eofe(%u), fifoErr(%u), lengthErr(%u)\n",
                     pgpCardRx.eofe, pgpCardRx.fifoErr, pgpCardRx.lengthErr);
              printf("\tpgpLane(%u), pgpVc(%u), size(%d)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc, *size);
              ret = 0;
          } else {
              bool hardwareFailure = false;
              uint32_t* u = (uint32_t*)ret;
              if (ret->failed((Pds::Pgp::LastBits*)(u+*size-1))) {
                  printf("Pgp::do_read received HW failure\n");
                  ret->print();
                  hardwareFailure = true;
              }
              if (ret->timeout((Pds::Pgp::LastBits*)(u+*size-1))) {
                  printf("Pgp::do_read received HW timed out\n");
                  ret->print();
                  hardwareFailure = true;
              }
              if (hardwareFailure) ret = 0;
          }
      } else {
          ret = 0;
      }
      return ret;
    }

    Pds::Pgp::RegisterSlaveImportFrame* Pgp::read(unsigned size) {
      Pds::Pgp::RegisterSlaveImportFrame* ret = (Pds::Pgp::RegisterSlaveImportFrame*)_readBuffer;
      int             sret = 0;
      struct timeval  timeout;
      timeout.tv_sec  = 0;
      timeout.tv_usec = SelectSleepTimeUSec;
      PgpCardRx       pgpCardRx;
      pgpCardRx.model   = sizeof(&pgpCardRx);
      pgpCardRx.maxSize = BufferWords;
      pgpCardRx.data    = (__u32*)_readBuffer;
      fd_set          fds;
      FD_ZERO(&fds);
      FD_SET(_fd,&fds);
      int readRet = 0;
      bool   found  = false;
      while (found == false) {
        if ((sret = select(_fd+1,&fds,NULL,NULL,&timeout)) > 0) {
          if ((readRet = ::read(_fd, &pgpCardRx, sizeof(PgpCardRx))) >= 0) {
            if ((ret->waiting() == Pds::Pgp::PgpRSBits::Waiting) || (ret->opcode() == Pds::Pgp::PgpRSBits::read)) {
              found = true;
              if (pgpCardRx.eofe || pgpCardRx.fifoErr || pgpCardRx.lengthErr) {
                printf("Pgp::read error eofe(%u), fifoErr(%u), lengthErr(%u)\n",
                    pgpCardRx.eofe, pgpCardRx.fifoErr, pgpCardRx.lengthErr);
                printf("\tpgpLane(%u), pgpVc(%u)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc);
                ret = 0;
              } else {
                if (readRet != (int)size) {
                  printf("Pgp::read read returned %u, we were looking for %u\n", readRet, size);
                  ret->print(readRet);
                  ret = 0;
                } else {
                  bool hardwareFailure = false;
                  uint32_t* u = (uint32_t*)ret;
                  if (ret->failed((Pds::Pgp::LastBits*)(u+size-1))) {
                    printf("Pgp::read received HW failure\n");
                    ret->print();
                    hardwareFailure = true;
                  }
                  if (ret->timeout((Pds::Pgp::LastBits*)(u+size-1))) {
                    printf("Pgp::read received HW timed out\n");
                    ret->print();
                    hardwareFailure = true;
                  }
                  if (hardwareFailure) ret = 0;
                }
              }
            }
          } else {
            perror("Pgp::read() ERROR ! ");
            ret = 0;
            found = true;
          }
        } else {
          found = true;  // we might as well give up!
          ret = 0;
          if (sret < 0) {
            perror("Pgp::read select error: ");
            printf("\tpgpLane(%u), pgpVc(%u)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc);
          } else {
            printf("Pgp::read select timed out!\n");
          }
        }
      }
      return ret;
    }

    unsigned Pgp::readStatus(void* s) {
      PgpCardTx p;

      p.model = sizeof(s);
      p.cmd   = IOCTL_Read_Status;
      p.data  = (__u32*) s;
      return(write(_fd, &p, sizeof(PgpCardTx)));
    }

    unsigned Pgp::stopPolling() {
      PgpCardTx p;

      p.model = sizeof(&p);
      p.cmd   = IOCTL_Clear_Polling;
      p.data  = 0;
      return(write(_fd, &p, sizeof(PgpCardTx)));
    }

    unsigned Pgp::writeRegister(
        Destination* dest,
        unsigned addr,
        uint32_t data,
        bool printFlag,
        Pds::Pgp::PgpRSBits::waitState w) {
        if (addr != 0xffffffff) {
            Pds::Pgp::RegisterSlaveExportFrame rsef = Pds::Pgp::RegisterSlaveExportFrame::RegisterSlaveExportFrame(
                  this,
                  Pds::Pgp::PgpRSBits::write,
                  dest,
                  addr,
                  0x6969,
                  data,
                  w);
            if (printFlag) rsef.print();
            return rsef.post(_fd, sizeof(rsef)/sizeof(uint32_t));
        } else {
          PgpCardTx           tx;
	  tx.model = sizeof(&tx);
	  tx.cmd   = IOCTL_Normal_Write;
	  tx.pgpLane = (dest->lane() & (_G3 ? 3 : 7)) + portOffset();
	  tx.pgpVc = dest->vc() & 3;
	  tx.size = 1;
	  tx.data = &data;
	  write(_fd, &tx, sizeof(tx));
          return Success;
        }
    }

    unsigned Pgp::writeRegisterBlock(
        Destination* dest,
        unsigned addr,
        uint32_t* data,
        unsigned inSize,  // the size of the block to be exported
        Pds::Pgp::PgpRSBits::waitState w,
        bool pf) {
      // the size of the export block plus the size of block to be exported minus the one that's already counted
      unsigned size = (sizeof(Pds::Pgp::RegisterSlaveExportFrame)/sizeof(uint32_t)) +  inSize -1;
      uint32_t myArray[size];
      Pds::Pgp::RegisterSlaveExportFrame* rsef = 0;
      rsef = new (myArray) Pds::Pgp::RegisterSlaveExportFrame::RegisterSlaveExportFrame(
              this,
              Pds::Pgp::PgpRSBits::write,
              dest,
              addr,
              0x6970,
              0,
              w);
      memcpy(rsef->array(), data, inSize*sizeof(uint32_t));
      rsef->array()[inSize] = 0;
      if (pf) rsef->print(0, size);
      return rsef->post(_fd, size);
    }

    unsigned Pgp::readRegister(
        Destination* dest,
        unsigned addr,
        unsigned tid,
        uint32_t* retp,
        unsigned size,
        bool pf) {
      Pds::Pgp::RegisterSlaveImportFrame* rsif;
      Pds::Pgp::RegisterSlaveExportFrame  rsef = Pds::Pgp::RegisterSlaveExportFrame::RegisterSlaveExportFrame(
          this,
          Pds::Pgp::PgpRSBits::read,
          dest,
          addr,
          tid,
          size - 1,  // zero = one uint32_t, etc.
          Pds::Pgp::PgpRSBits::Waiting);
//      if (size>1) {
//        printf("Pgp::readRegister size %u\n", size);
//        rsef.print();
//      }
      if (pf) rsef.print();
      if (rsef.post(_fd, sizeof(Pds::Pgp::RegisterSlaveExportFrame)/sizeof(uint32_t),pf) != Success) {
        printf("Pgp::readRegister failed, export frame follows.\n");
        rsef.print(0, sizeof(Pds::Pgp::RegisterSlaveExportFrame)/sizeof(uint32_t));
        return  Failure;
      }
      unsigned errorCount = 0;
      while (true) {
        rsif = this->read(size + 3);
        if (rsif == 0) {
          printf("Pgp::readRegister _pgp->read failed!\n");
          return Failure;
        }
        if (pf) rsif->print(size + 3);
        if (addr != rsif->addr()) {
          printf("Pds::Pgp::readRegister out of order response lane=%u, vc=%u, addr=0x%x, tid=%u, errorCount=%u\n",
              dest->lane(), dest->vc(), addr, tid, ++errorCount);
          rsif->print(size + 3);
          if (errorCount > 5) return Failure;
        } else {  // copy the data
          memcpy(retp, rsif->array(), size * sizeof(uint32_t));
          return Success;
        }
      }
    }

    int Pgp::IoctlCommand(unsigned c, unsigned a) {
      PgpCardTx p;
      p.model = sizeof(&p);
      p.cmd   = c;
      p.data  = (unsigned*) a;
      return(write(_fd, &p, sizeof(PgpCardTx)));
    }

  }

}
