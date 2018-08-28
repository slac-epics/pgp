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
      if ( access( devName, F_OK ) != -1 ) {
        _useAesDriver = false;
      } else {
        sprintf(devName, "/dev/pgpcard_%u", f & 0xf);
        if ( access( devName, F_OK ) != -1 ) {
          _useAesDriver = true;
        } else {
          printf("Pgp::Pgp() can't find any pgpcards!\n");
          throw "Can't determine pgpcard driver type";
        }
      }
      printf("Opening %s\n", devName);
      _fd = open( devName,  O_RDWR | O_NONBLOCK);
      if (_fd < 0) {
        sprintf(err, "Pgp::Pgp() opening %s failed", devName);
        perror(err);
        throw "Can't open file";
      }
      if (G3 || _useAesDriver) {
        _portOffset = ports - 1;
      } else {
        _portOffset = 0;
        while ((((ports>>_portOffset) & 1) == 0) && (_portOffset < 4)) {
          _portOffset += 1;
        }
      }
      if (vcm) {
        if (_useAesDriver) {
          unsigned char maskBytes[32];
          dmaInitMaskBytes(maskBytes);
          for(unsigned i=0; i<4; i++) {
            if ((1<<i) & vcm)
              dmaAddMaskBytes(maskBytes, (_portOffset<<2) + i);
          }
          dmaSetMaskBytes(_fd, maskBytes);
        } else {
          IoctlCommand(IOCTL_Set_VC_Mask, (vcm<<8) | _portOffset);
        }
      }
      memset(_directWrites, 0, sizeof(_directWrites));
      // End MCB changes.
      if (pf) printf("Pgp::Pgp(fd(%d)), offset(%u)\n", _fd, _portOffset);
      for (int i=0; i<BufferWords; i++) _readBuffer[i] = i;
    }

    Pgp::~Pgp() {
      close(_fd);
    }

    Pds::Pgp::RegisterSlaveImportFrame* Pgp::do_read(unsigned *buffer, int *size) {
      Pds::Pgp::RegisterSlaveImportFrame* ret = (Pds::Pgp::RegisterSlaveImportFrame*)buffer;
      memset(_readBuffer, 0, sizeof(_readBuffer));
      void*           pgpRxBuff = 0;
      size_t          pgpRxSize = 0;
      ssize_t         retSize   = 0;
      PgpCardRx       pgpCardRx;
      DmaReadData     dmaReadData;
      if (_useAesDriver) {
        dmaReadData.is32   = sizeof(&dmaReadData) == 4;
        dmaReadData.size   = (*size) * sizeof(unsigned);
        dmaReadData.data   = (uint64_t)buffer;
        dmaReadData.dest   = 0;
        dmaReadData.flags  = 0;
        dmaReadData.index  = 0;
        dmaReadData.error  = 0;
        pgpRxBuff = &dmaReadData;
        pgpRxSize = sizeof(DmaReadData);
      } else {
        pgpCardRx.model   = sizeof(&pgpCardRx);
        pgpCardRx.maxSize = *size;
        pgpCardRx.data    = (__u32*)buffer;
        pgpRxBuff = &pgpCardRx;
        pgpRxSize = sizeof(PgpCardRx);
      }
      if ((retSize = ::read(_fd, pgpRxBuff, pgpRxSize)) >= 0) {
          if (_useAesDriver ? dmaReadData.error : (pgpCardRx.eofe || pgpCardRx.fifoErr || pgpCardRx.lengthErr)) {
            if (_useAesDriver) {
              printf("Pgp::read error fifoErr(%u), lengthErr(%u), maxErr(%u), busErr(%u)\n",
                     dmaReadData.error&DMA_ERR_FIFO, dmaReadData.error&DMA_ERR_LEN, dmaReadData.error&DMA_ERR_MAX, dmaReadData.error&DMA_ERR_BUS);
              printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest));
            } else {
              printf("Pgp::do_read error eofe(%u), fifoErr(%u), lengthErr(%u)\n",
                     pgpCardRx.eofe, pgpCardRx.fifoErr, pgpCardRx.lengthErr);
              printf("\tpgpLane(%u), pgpVc(%u), size(%d)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc, *size);
            }
            ret = 0;
          } else {
              bool hardwareFailure = false;
              uint32_t* u = (uint32_t*)ret;
              if (_useAesDriver) {
                u += ((retSize / sizeof(uint32_t)) - 1);
              } else {
                u += (retSize - 1);
              }
              if (ret->failed((Pds::Pgp::LastBits*)(u))) {
                  printf("Pgp::do_read received HW failure\n");
                  ret->print();
                  hardwareFailure = true;
              }
              if (ret->timeout((Pds::Pgp::LastBits*)(u))) {
                  printf("Pgp::do_read received HW timed out\n");
                  ret->print();
                  hardwareFailure = true;
              }
              if (hardwareFailure) ret = 0;
          }
      } else {
        *size = retSize;
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
      void*           pgpRxBuff = 0;
      size_t          pgpRxSize = 0;
      size_t          retSize = 0;
      PgpCardRx       pgpCardRx;
      DmaReadData     dmaReadData;
      if (_useAesDriver) {
        dmaReadData.is32   = sizeof(&dmaReadData) == 4;
        dmaReadData.size   = sizeof(_readBuffer);
        dmaReadData.data   = (uint64_t)_readBuffer;
        dmaReadData.dest   = 0;
        dmaReadData.flags  = 0;
        dmaReadData.index  = 0;
        dmaReadData.error  = 0;
        retSize   = size * sizeof(unsigned);
        pgpRxBuff = &dmaReadData;
        pgpRxSize = sizeof(DmaReadData);
      } else {
        pgpCardRx.model   = sizeof(&pgpCardRx);
        pgpCardRx.maxSize = BufferWords;
        pgpCardRx.data    = (__u32*)_readBuffer;
        retSize   = size;
        pgpRxBuff = &pgpCardRx;
        pgpRxSize = sizeof(PgpCardRx);
      }
      fd_set          fds;
      FD_ZERO(&fds);
      FD_SET(_fd,&fds);
      int readRet = 0;
      bool   found  = false;
      while (found == false) {
        if ((sret = select(_fd+1,&fds,NULL,NULL,&timeout)) > 0) {
          if ((readRet = ::read(_fd, &pgpRxBuff, pgpRxSize)) >= 0) {
            if ((ret->waiting() == Pds::Pgp::PgpRSBits::Waiting) || (ret->opcode() == Pds::Pgp::PgpRSBits::read)) {
              found = true;
              if (_useAesDriver ? dmaReadData.error : (pgpCardRx.eofe || pgpCardRx.fifoErr || pgpCardRx.lengthErr)) {
                if (_useAesDriver) {
                  printf("Pgp::read error fifoErr(%u), lengthErr(%u), maxErr(%u), busErr(%u)\n",
                      dmaReadData.error&DMA_ERR_FIFO, dmaReadData.error&DMA_ERR_LEN, dmaReadData.error&DMA_ERR_MAX, dmaReadData.error&DMA_ERR_BUS);
                  printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest));
                } else {
                  printf("Pgp::read error eofe(%u), fifoErr(%u), lengthErr(%u)\n",
                      pgpCardRx.eofe, pgpCardRx.fifoErr, pgpCardRx.lengthErr);
                  printf("\tpgpLane(%u), pgpVc(%u)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc);
                }
                ret = 0;
              } else {
                if (readRet != (int)retSize) {
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
            if (_useAesDriver) {
              printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest));
            } else {
              printf("\tpgpLane(%u), pgpVc(%u)\n", pgpCardRx.pgpLane, pgpCardRx.pgpVc);
            }
          } else {
            printf("Pgp::read select timed out!\n");
          }
        }
      }
      return ret;
    }

    unsigned Pgp::enableRunTrigger(uint32_t enable, uint32_t runCode, uint32_t runDelay, bool printFlag) {
      if (_G3) { 
        if (_useAesDriver) {
          ssize_t res = 0;
          PgpEvrControl cntl;
          res |= pgpGetEvrControl(_fd, _portOffset, &cntl);
          if (printFlag) {
            printf("Pgp::enableRunTrigger (pre-config):  \tevrEnabled(%u), laneRunMask(%u), "
                   "runCode(%u), runDelay(%u)\n",
                   cntl.evrEnable, cntl.laneRunMask, cntl.runCode, cntl.runDelay);
          }
          cntl.evrEnable = enable;
          cntl.laneRunMask = enable ? 0 : 1;
          cntl.runCode = runCode;
          cntl.runDelay = runDelay;
          res |= pgpSetEvrControl(_fd, _portOffset, &cntl);
          if (printFlag) {
            printf("Pgp::enableRunTrigger (post-config): \tevrEnabled(%u), laneRunMask(%u), "
                   "runCode(%u), runDelay(%u)\n",
                   cntl.evrEnable, cntl.laneRunMask, cntl.runCode, cntl.runDelay);
          }
          return res;
        } else {
          int ret = 0;
          unsigned arg = 0;
          unsigned mask = 1 << _portOffset;
          // set the run code
          arg = (_portOffset << 28) | (runCode & 0xff);
          ret |= IoctlCommand(IOCTL_Evr_RunCode, arg);
          // set the run delay
          arg = (_portOffset << 28) | (runDelay & 0xfffffff);
          ret |= IoctlCommand(IOCTL_Evr_RunDelay, arg);
          // enable run trigger
          arg = (mask<<24) | (enable ? 0 : 1);
          ret |= IoctlCommand(IOCTL_Evr_RunMask, arg);
          // enable the evr itself
          arg = 0;
          if (enable) {
            ret |= IoctlCommand( IOCTL_Evr_Enable, arg);
          } else {
            ret |= IoctlCommand( IOCTL_Evr_Disable, arg);
          }
          return Success;
        }
      }
      return Success;
    }

    unsigned Pgp::readStatus(void* s) {
      if (_useAesDriver) {
        PgpStatus* status = (PgpStatus*) s;
        for (int lane = 0; lane < (_G3 ? 8 : 4); lane++) {
          pgpGetStatus(_fd,lane,&status[lane]);
        }
        return Success;
      } else {
        PgpCardTx p;

        p.model = sizeof(s);
        p.cmd   = IOCTL_Read_Status;
        p.data  = (__u32*) s;
        return(write(_fd, &p, sizeof(PgpCardTx)));
      }
    }

    unsigned Pgp::stopPolling() {
      if (!_useAesDriver) {
        PgpCardTx p;

        p.model = sizeof(&p);
        p.cmd   = IOCTL_Clear_Polling;
        p.data  = 0;
        return(write(_fd, &p, sizeof(PgpCardTx)));
      } else {
        return Success;
      }
    }

    unsigned Pgp::writeData(Destination* dest, uint32_t data, bool printFlag) {
      if (_useAesDriver) {
        DmaWriteData  tx;
        tx.is32   = sizeof(&tx) == 4;
        tx.dest   = (dest->vc() & 3) | (((dest->lane() & (_G3 ? 7: 3)) + portOffset())<<2);
        tx.size   = sizeof(data);
        tx.data   = (uint64_t)&data;
        tx.flags  = 0;
        tx.index  = 0;
      } else {
        PgpCardTx           tx;
        tx.model = sizeof(&tx);
        tx.cmd   = IOCTL_Normal_Write;
        tx.pgpLane = (dest->lane() & (_G3 ? 7 : 3)) + portOffset();
        tx.pgpVc = dest->vc() & 3;
        tx.size = 1;
        tx.data = &data;
        if (printFlag) printf("Pgp::write of value %u to lane(%u), vc(%u)\n", data, tx.pgpLane, tx.pgpVc);
        write(_fd, &tx, sizeof(tx));
      }
      _directWrites[dest->vc() & 3] = data;
      return Success;
    }

    unsigned Pgp::lastWriteData(Destination* dest, uint32_t* retp) {
      *retp = _directWrites[dest->vc() & 3];
      return Success;
    }

    unsigned Pgp::writeRegister(
        Destination* dest,
        unsigned addr,
        uint32_t data,
        bool printFlag,
        Pds::Pgp::PgpRSBits::waitState w) {
      Pds::Pgp::RegisterSlaveExportFrame rsef = Pds::Pgp::RegisterSlaveExportFrame(
          this,
          Pds::Pgp::PgpRSBits::write,
          dest,
          addr,
          0x6969,
          data,
          w);
      if (printFlag) rsef.print();
      return rsef.post(_useAesDriver, _fd, sizeof(rsef)/sizeof(uint32_t));
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
      rsef = new (myArray) Pds::Pgp::RegisterSlaveExportFrame(
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
      return rsef->post(_useAesDriver, _fd, size);
    }

    unsigned Pgp::readRegister(
        Destination* dest,
        unsigned addr,
        unsigned tid,
        uint32_t* retp,
        unsigned size,
        bool pf) {
      Pds::Pgp::RegisterSlaveImportFrame* rsif;
      Pds::Pgp::RegisterSlaveExportFrame  rsef = Pds::Pgp::RegisterSlaveExportFrame(
        this,
        Pds::Pgp::PgpRSBits::read,
        dest,
        addr,
        tid,
        size - 1,  // zero = one uint32_t, etc.
        Pds::Pgp::PgpRSBits::Waiting);
      if (pf) rsef.print();
      if (rsef.post(_useAesDriver, _fd, sizeof(Pds::Pgp::RegisterSlaveExportFrame)/sizeof(uint32_t),pf) != Success) {
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

    unsigned Pgp::flushInputQueue(bool printFlag) {
      fd_set          fds;
      struct timeval  timeout;
      timeout.tv_sec  = 0;
      timeout.tv_usec = 2500;
      int ret;
      unsigned count = 0;
      void*           pgpRxBuff = 0;
      size_t          pgpRxSize = 0;
      PgpCardRx       pgpCardRx;
      DmaReadData     dmaReadData;
      if (_useAesDriver) {
        dmaReadData.is32   = sizeof(&dmaReadData) == 4;
        dmaReadData.size   = sizeof(_dummy);
        dmaReadData.data   = (uint64_t)_dummy;
        dmaReadData.dest   = 0;
        dmaReadData.flags  = 0;
        dmaReadData.index  = 0;
        dmaReadData.error  = 0;
        pgpRxBuff = &dmaReadData;
        pgpRxSize = sizeof(DmaReadData);
      } else {
        pgpCardRx.model   = sizeof(&pgpCardRx);
        pgpCardRx.maxSize = DummyWords;
        pgpCardRx.data    = _dummy;
        pgpRxBuff = &pgpCardRx;
        pgpRxSize = sizeof(PgpCardRx);
      }
      do {
        FD_ZERO(&fds);
        FD_SET(_fd,&fds);
        ret = select( _fd+1, &fds, NULL, NULL, &timeout);
        if (ret>0) {
          count += 1;
          ::read(_fd, pgpRxBuff, pgpRxSize);
        }
      } while ((ret > 0) && (count < 100));
      if (count && printFlag) {
          if (_useAesDriver) {
            printf("\tflushInputQueue: pgpCardRx count(%u) lane(%u) vc(%u) rxSize(%u) fifo(%s) lengthErr(%s)\n",
                   count, pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest), dmaReadData.size,
                   dmaReadData.error&DMA_ERR_FIFO ? "true" : "false", dmaReadData.error&DMA_ERR_LEN ? "true" : "false");
            printf("\t\t");
            for (unsigned i=0; i<(dmaReadData.size/sizeof(unsigned)); i++)
                printf("0x%08x ", _dummy[i]);
            printf("\n");
          } else {
            printf("\tflushInputQueue: pgpCardRx count(%u) lane(%u) vc(%u) rxSize(%u) eofe(%s) lengthErr(%s)\n",
                  count, pgpCardRx.pgpLane, pgpCardRx.pgpVc, pgpCardRx.rxSize,
                  pgpCardRx.eofe ? "true" : "false", pgpCardRx.lengthErr ? "true" : "false");
            printf("\t\t");
            for (unsigned i=0; i<pgpCardRx.rxSize; i++)
                printf("0x%08x ", _dummy[i]);
            printf("\n");
          }
      }
      return count;
    }

    int Pgp::IoctlCommand(unsigned c, unsigned a) {
      if (!_useAesDriver) {
        PgpCardTx p;
        p.model = sizeof(&p);
        p.cmd   = c;
        p.data  = (unsigned*) a;
        return(write(_fd, &p, sizeof(PgpCardTx)));
      } else {
        return -1;
      }
    }

  }

}
