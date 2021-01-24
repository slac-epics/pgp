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
#include <sys/ioctl.h>

namespace Pds {

  namespace Pgp {

    // MCB - No longer passing a fd, now just the port/lane/VC/G3 information.
    Pgp::Pgp(int mask, int vcm, bool srpV3, bool pf) :
      _fd(-1),
      _srpV3(srpV3),
      _type(Unknown),
      _portOffset(((mask >> 4) & 0xf) - 1),
      _proto(new SrpV3::Protocol(_fd, _portOffset))
    {
      char devName[128];
      char err[128];
      sprintf(devName, "/dev/pgpcard_%u", mask & 0xf);
      if ( access( devName, F_OK ) != -1 ) {
        _type = G2;
      } else {
        sprintf(devName, "/dev/datadev_%u", mask & 0xf);
        if ( access( devName, F_OK ) != -1 ) {
          _type = DataDev;
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
      // check if it is a G3 card
      if (usesPgpDriver()) {
        struct PgpInfo info;
        if (pgpGetInfo(_fd, &info) < 0) {
          printf("Pgp::Pgp() Can't determine if pgpcard is a G3 card!\n");
          throw "Can't determine if pgpcard is a G3 card";
        } else if (info.type == 3) {
          _type=G3;
        }
      }
      if (dmaCheckVersion(_fd) < 0) {
        printf("Pgp::Pgp() DMA API Version (%d) does not match the driver (%d)!\n",
               DMA_VERSION, ioctl(_fd, DMA_Get_Version));
        throw "DMA API Version mismatch";
      }
      if (vcm) {
        unsigned char maskBytes[DMA_MASK_SIZE];
        dmaInitMaskBytes(maskBytes);
        for(unsigned i=0; i<4; i++) {
          if ((1<<i) & vcm)
            pgpAddMaskBytes(maskBytes, _portOffset, i);
        }
        dmaSetMaskBytes(_fd, maskBytes);
      }
      memset(_directWrites, 0, sizeof(_directWrites));
      // if using SrpV3 create the proto object
      if (_srpV3) {
        _proto = new SrpV3::Protocol(_fd, _portOffset);
      }
      // End MCB changes.
      printf("Pgp::Pgp(fd(%d)), offset(%u) %u\n", _fd, _portOffset, vcm);
      if (pf) printf("Pgp::Pgp(fd(%d)), offset(%u)\n", _fd, _portOffset);
      for (int i=0; i<BufferWords; i++) _readBuffer[i] = i;
    }

    Pgp::~Pgp() {
      if (_proto) delete _proto;
      close(_fd);
    }

    Pds::Pgp::RegisterSlaveImportFrame* Pgp::do_read(unsigned *buffer, int *size) {
      Pds::Pgp::RegisterSlaveImportFrame* ret = (Pds::Pgp::RegisterSlaveImportFrame*)buffer;
      memset(_readBuffer, 0, sizeof(_readBuffer));
      ssize_t         retSize   = 0;
      DmaReadData     dmaReadData;
      dmaReadData.is32   = sizeof(&dmaReadData) == 4;
      dmaReadData.size   = (*size) * sizeof(unsigned);
      dmaReadData.data   = (uint64_t)buffer;
      dmaReadData.dest   = 0;
      dmaReadData.flags  = 0;
      dmaReadData.index  = 0;
      dmaReadData.error  = 0;
      dmaReadData.ret    = 0;
      if ((retSize = ::read(_fd, &dmaReadData, sizeof(DmaReadData))) >= 0) {
          *size = (dmaReadData.ret / sizeof(uint32_t));
          if (dmaReadData.error) {
            printf("Pgp::read error fifoErr(%u), lengthErr(%u), maxErr(%u), busErr(%u)\n",
                    dmaReadData.error&DMA_ERR_FIFO, dmaReadData.error&DMA_ERR_LEN, dmaReadData.error&DMA_ERR_MAX, dmaReadData.error&DMA_ERR_BUS);
            printf("\tpgpLane(%u), pgpVc(%u), size(%d)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest), *size);
            ret = 0;
          } else {
              bool hardwareFailure = false;
              uint32_t* u = (uint32_t*)ret + *size -1;
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
      size_t          retSize = 0;
      DmaReadData     dmaReadData;
      dmaReadData.is32   = sizeof(&dmaReadData) == 4;
      dmaReadData.size   = sizeof(_readBuffer);
      dmaReadData.data   = (uint64_t)_readBuffer;
      dmaReadData.dest   = 0;
      dmaReadData.flags  = 0;
      dmaReadData.index  = 0;
      dmaReadData.error  = 0;
      dmaReadData.ret    = 0;
      retSize   = size * sizeof(unsigned);
      fd_set          fds;
      FD_ZERO(&fds);
      FD_SET(_fd,&fds);
      int readRet = 0;
      bool   found  = false;
      while (found == false) {
        if ((sret = select(_fd+1,&fds,NULL,NULL,&timeout)) > 0) {
          if ((readRet = ::read(_fd, &dmaReadData, sizeof(DmaReadData))) >= 0) {
            readRet = dmaReadData.ret;
            if ((ret->waiting() == Pds::Pgp::PgpRSBits::Waiting) || (ret->opcode() == Pds::Pgp::PgpRSBits::read)) {
              found = true;
              if (dmaReadData.error) {
                printf("Pgp::read error fifoErr(%u), lengthErr(%u), maxErr(%u), busErr(%u)\n",
                       dmaReadData.error&DMA_ERR_FIFO, dmaReadData.error&DMA_ERR_LEN, dmaReadData.error&DMA_ERR_MAX, dmaReadData.error&DMA_ERR_BUS);
                printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest));
                ret = 0;
              } else {
                if (readRet != (int)retSize) {
                  printf("Pgp::read read returned %u, we were looking for %zu\n", readRet, retSize);
                  ret->print();
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
            printf("\tpgpLane(%u), pgpVc(%u)\n", pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest));
          } else {
            printf("Pgp::read select timed out!\n");
          }
        }
      }
      return ret;
    }

    unsigned Pgp::enableRunTrigger(uint32_t enable, uint32_t runCode, uint32_t runDelay, bool printFlag) {
      if (_type == G3) { 
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
      }
      return Success;
    }

    unsigned Pgp::readStatus(void* s) {
      if (usesPgpDriver()) {
        PgpStatus* status = (PgpStatus*) s;
        for (int lane = 0; lane < (isG3() ? 8 : 4); lane++) {
          pgpGetStatus(_fd,lane,&status[lane]);
        }
        return Success;
      } else {
        return Success;
      }
    }

    unsigned Pgp::writeData(Destination* dest, uint32_t data, bool printFlag) {
      DmaWriteData  tx;
      tx.is32   = sizeof(&tx) == 4;
      tx.dest   = (dest->vc() & 3) | (((dest->lane() & (isG3() ? 7: 3)) + portOffset())<<2);
      tx.size   = sizeof(data);
      tx.data   = (uint64_t)&data;
      tx.flags  = 0;
      tx.index  = 0;
      if (printFlag) printf("Pgp::write of value %u to lane(%u), vc(%u)\n",
                            data,
                            (dest->lane() & (isG3() ? 7: 3)) + portOffset(),
                            dest->vc() & 3);
      write(_fd, &tx, sizeof(tx));
      _directWrites[dest->vc() & 3] = data;
      return Success;
    }

    unsigned Pgp::writeDataBlock(Destination* dest, uint32_t* data, unsigned size, bool printFlag) {
      DmaWriteData  tx;
      tx.is32   = sizeof(&tx) == 4;
      tx.dest   = (dest->vc() & 3) | (((dest->lane() & (isG3() ? 7: 3)) + portOffset())<<2);
      tx.size   = sizeof(uint32_t) * size;
      tx.data   = (uint64_t)data;
      tx.flags  = 0;
      tx.index  = 0;
      if (printFlag) {
        printf("Pgp::write of data to lane(%u), vc(%u):",
               (dest->lane() & (isG3() ? 7: 3)) + portOffset(),
               dest->vc() & 3);
        for(unsigned i=0; i<size; i++) {
          printf(" %u", data[i]);
        }
        printf("\n");
      }
      write(_fd, &tx, sizeof(tx));
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
      if (_srpV3) {
        return _proto->writeRegister(dest, addr, data);
      }
      Pds::Pgp::RegisterSlaveExportFrame rsef = Pds::Pgp::RegisterSlaveExportFrame(
          this,
          Pds::Pgp::PgpRSBits::write,
          dest,
          addr,
          0x6969,
          data,
          w);
      if (printFlag) rsef.print();
      return rsef.post(_fd, sizeof(rsef)/sizeof(uint32_t));
    }

    unsigned Pgp::writeRegisterBlock(
        Destination* dest,
        unsigned addr,
        uint32_t* data,
        unsigned inSize,  // the size of the block to be exported
        Pds::Pgp::PgpRSBits::waitState w,
        bool pf) {
      if (_srpV3) {
        return _proto->writeRegisterBlock(dest, addr, data, inSize);
      }
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
      return rsef->post(_fd, size);
    }

    unsigned Pgp::readRegister(
        Destination* dest,
        unsigned addr,
        unsigned tid,
        uint32_t* retp,
        unsigned size,
        bool pf) {
      if (_srpV3) {
        return _proto->readRegister(dest, addr, tid, retp);
      }
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

    unsigned Pgp::flushInputQueue(bool printFlag) {
      fd_set          fds;
      struct timeval  timeout;
      timeout.tv_sec  = 0;
      timeout.tv_usec = 2500;
      int ret;
      int read_ret;
      unsigned count = 0;
      DmaReadData     dmaReadData;
      dmaReadData.is32   = sizeof(&dmaReadData) == 4;
      dmaReadData.size   = sizeof(_dummyBuffer);
      dmaReadData.data   = (uint64_t)_dummyBuffer;
      dmaReadData.dest   = 0;
      dmaReadData.flags  = 0;
      dmaReadData.index  = 0;
      dmaReadData.error  = 0;
      dmaReadData.ret    = 0;
      do {
        FD_ZERO(&fds);
        FD_SET(_fd,&fds);
        ret = select( _fd+1, &fds, NULL, NULL, &timeout);
        if (ret>0) {
          count += 1;
          read_ret = ::read(_fd, &dmaReadData, sizeof(DmaReadData));
          if (read_ret > 0) {
            count += 1;
          } else {
            perror("\tflushInputQueue: error on read from device: ");
          }
        }
      } while ((ret > 0) && (count < 100));
      if (count && printFlag) {
          printf("\tflushInputQueue: pgpCardRx count(%u) lane(%u) vc(%u) rxSize(%u) fifo(%s) lengthErr(%s)\n",
                 count, pgpGetLane(dmaReadData.dest), pgpGetVc(dmaReadData.dest), dmaReadData.size,
                 dmaReadData.error&DMA_ERR_FIFO ? "true" : "false", dmaReadData.error&DMA_ERR_LEN ? "true" : "false");
          printf("\t\t");
          printf("%u\n", dmaReadData.size);
          for (unsigned i=0; i<DummyWords; i++)
              printf("0x%08x ", _dummyBuffer[i]);
          printf("\n");
      }
      return count;
    }

  }
}
