#define USE_TYPED_RSET
#include<epicsExport.h>
#include<registryFunction.h>
#include<epicsStdio.h>
#include<epicsExit.h>
#include<epicsStdlib.h>
#include<epicsString.h>
#include<epicsThread.h>
#include<epicsTypes.h>
#include<devSup.h>
#include<drvSup.h>
#include<iocsh.h> 
#include<link.h>
#include<dbAccess.h>
#include<dbScan.h>
#include<dbEvent.h>
#include<aSubRecord.h>  // Sigh.  A copy of aSubRecord.h, with not -> not2 so it is legal C++.
#include<longinRecord.h>
#include<longoutRecord.h>
#include<waveformRecord.h>
#include<recGbl.h>
#include<alarm.h>
#include<ellLib.h>
#include<string.h>
#include<strings.h>
#include<unistd.h>
#include"evrTime.h"
#include"pds/pgp/Pgp.hh"
#include<sys/select.h>
#include"PGP_extern.h"

#define MAX_CA_STRING_SIZE 40

using namespace Pds::Pgp;

extern "C" {

static ELLLIST PGPCards = {{ NULL, NULL }, 0};
static epicsMutexId PGPlock = NULL;
int PGP_reg_debug = false;

static int PGPHandlerThread(void *p);

class PGPCARD;

struct srchandler {
    unsigned lane;
    unsigned vc;
    IOSCANPVT ioscan;
    DBADDR trigenable;
    epicsEnum16 trigstate;
    PGP_rcvfunc rcvfunc;
    PGP_enfunc enfunc;
    void *dev_token;
    pgp_data *data;
};

class PGPCARD {
public:
    enum Params { CFGINC = 10, SRCINC = 10 };
    ELLNODE node;
    char *name;
    Pgp *pgp;
    int cfgpipe[2];
    int cfgsize;
    int cfgmax;
    struct cfgelem {
        struct longinRecord  *rbv;
        struct longoutRecord *val;
        int lane;
        int vc;
        unsigned addr;
        Destination *dest;
    } *cfg;
    enum CfgState { CfgIdle, CfgActive, CfgDone} cfgstate;
    int cfgerrs;
    struct srchandler src;

    PGPCARD(char *_name, int _lane, int _vcm, int _srpV3) {
        cfgstate = CfgIdle;
        name = epicsStrDup(_name);
        pgp = new Pgp(_lane, _vcm, _srpV3);
        switch (pgp->type()) {
        case Pgp::G3:
          {
            PgpStatus status[8];

            pgp->readStatus(&status[0]);
            printf("Checking link status of %s:\n", name);
            printf("    Local: ");
            for (int i=0; i<8; i++) {
              printf(" %2d", status[i].locLinkReady);
            }
            printf("\n");
            printf("    Remote:");
            for (int i=0; i<8; i++) {
              printf(" %2d", status[i].remLinkReady);
            }
            printf("\n");
          }
          break;
        case Pgp::G2:
          {
            PgpStatus status[8];

            pgp->readStatus(&status[0]);
            printf("Checking link status of %s:\n", name);
            printf("    Local: ");
            for (int i=0; i<4; i++) {
              printf(" %2d", status[i].locLinkReady);
            }
            printf("\n");
            printf("    Remote:");
            for (int i=0; i<4; i++) {
              printf(" %2d", status[i].remLinkReady);
            }
            printf("\n");
          }
          break;
        case Pgp::DataDev:
          {
            PgpStatus status[8];

            pgp->readStatus(&status[0]);
            printf("Checking link status of %s:\n", name);
            printf("    Local: ");
            for (int i=0; i<4; i++) {
              printf(" %2d", status[i].locLinkReady);
            }
            printf("\n");
            printf("    Remote:");
            for (int i=0; i<4; i++) {
              printf(" %2d", status[i].remLinkReady);
            }
            printf("\n");
          }
          break;
        default:
          printf("Checking the link status of cards of type %d is unsupported!",
                 pgp->type());
        }
        if (pipe(cfgpipe) == -1) {
            printf("pipe creation failed!\n");
            epicsThreadSuspendSelf();
        }
        cfgsize = CFGINC;
        cfgmax  = -1;
        cfg = (struct cfgelem *)calloc(CFGINC, sizeof(struct cfgelem));
        epicsThreadMustCreate("PGPHandler", epicsThreadPriorityHigh,
                              epicsThreadGetStackSize(epicsThreadStackMedium),
                              (EPICSTHREADFUNC)PGPHandlerThread,this);
    };
    ~PGPCARD() {
        free(name);
        delete pgp;
    }
private:
    void addCfg(int seq) {
        if (seq >= cfgsize) {
            int old = cfgsize;
            while (seq >= cfgsize) {
                cfgsize = cfgsize + CFGINC;
            }
            cfg = (struct cfgelem *)realloc(cfg, cfgsize*sizeof(struct cfgelem));
            memset(&cfg[old], 0, sizeof(cfgelem)*(cfgsize - old));
        }
        if (seq > cfgmax) {
            cfgmax = seq;
        }
    }
public:
    void addCfgIn(struct longinRecord *r, int lane, int vc, int addr, int seq) {
        addCfg(seq);
        cfg[seq].rbv = r;
        if (cfg[seq].dest == NULL) {
            cfg[seq].lane = lane;
            cfg[seq].vc   = vc;
            cfg[seq].addr = addr;
            cfg[seq].dest = new Destination((lane << 2) | (vc & 3));
        }
    }
    void addCfgOut(struct longoutRecord *r, int lane, int vc, int addr, int seq) {
        addCfg(seq);
        cfg[seq].val = r;
        if (cfg[seq].dest == NULL) {
            cfg[seq].lane = lane;
            cfg[seq].vc   = vc;
            cfg[seq].addr = addr;
            cfg[seq].dest = new Destination((lane << 2) | (vc & 3));
        }
    }
    void *addSrc(int lane, int vc, char *trigger, PGP_rcvfunc rcvfunc, PGP_enfunc enfunc, void *dev_token) {
        src.lane      = lane;
        src.vc        = vc;
        src.rcvfunc   = rcvfunc;
        src.enfunc    = enfunc;
        src.dev_token = dev_token;
        src.data      = NULL;
        scanIoInit(&src.ioscan);
        if (dbNameToAddr(trigger, &src.trigenable)) {
            printf("No PV trigger named %s!\n", trigger);
            return NULL;
        } else
            return this;
    }
    void configure(struct aSubRecord *r) {
        switch (cfgstate) {
        case CfgIdle:
            cfgstate = CfgActive;              // Start configuring!
            r->pact = TRUE;
            write(cfgpipe[1], &r, sizeof(r));  // Wake up the thread, and tell him who is calling!
            break;
        case CfgActive:
            break;                             // Do nothing!
        case CfgDone:
            *(epicsInt32 *)r->vala = cfgerrs;
            *(epicsInt32 *)r->valb = 1;
            cfgstate = CfgIdle;                // Finished!
            break;
        }
    }
    void disableSrc(void) {
        epicsEnum16 disable = 0;
        src.trigstate = *(epicsEnum16 *)src.trigenable.pfield;
        dbPutField(&src.trigenable, DBR_ENUM, &disable, sizeof(src.trigstate));
        dbScanLock(src.trigenable.precord);
        dbProcess(src.trigenable.precord);
        dbScanUnlock(src.trigenable.precord);
        if (src.enfunc)
            (*src.enfunc)(0, src.dev_token);
    }
    void enableSrc(void) {
        if (src.enfunc)
            (*src.enfunc)(1, src.dev_token);
        pgp->flushInputQueue(PGP_reg_debug);
        dbPutField(&src.trigenable, DBR_ENUM, &src.trigstate, sizeof(src.trigstate));
    }
    void doConfigure(void) {
        int i;
        unsigned val;
        cfgerrs = 0;
        for (i = 0; i <= cfgmax; i++) {
            if (cfg[i].val) {
                if (cfg[i].addr == Pgp::DirectWrite) {
                  printf("%d: Writing 0x%x directly to vc %d\n", i, cfg[i].val->val, cfg[i].vc);
                  pgp->writeData(cfg[i].dest, cfg[i].val->val, PGP_reg_debug);
                } else {
                  printf("%d: Writing 0x%x to address %d\n", i, cfg[i].val->val, cfg[i].addr);
                  pgp->writeRegister(cfg[i].dest, cfg[i].addr, cfg[i].val->val, PGP_reg_debug, PgpRSBits::notWaiting);
                }
            } else {
                printf("%d: WARNING: No write entry!!\n", i);
            }
        }
        for (i = 0; i <= cfgmax; i++) {
            if (cfg[i].rbv && cfg[i].addr != Pgp::DirectWrite) {
                if (cfg[i].addr == Pgp::DirectWrite) {
                  pgp->lastWriteData(cfg[i].dest, &val);
                } else {
                  pgp->readRegister(cfg[i].dest, cfg[i].addr, 0x4200 + i, &val, 1, PGP_reg_debug);
                }
                printf("Read 0x%x from address %d\n", val, cfg[i].addr);
                cfg[i].rbv->val = val;
                cfg[i].rbv->udf = 0;
                recGblGetTimeStamp(cfg[i].rbv);
                recGblResetAlarms(cfg[i].rbv);
                if (cfg[i].val && cfg[i].rbv->val != cfg[i].val->val) {
                    printf("Mismatch on address %d!\n", cfg[i].addr);
                    cfgerrs++;
                }
                db_post_events(cfg[i].rbv, &cfg[i].rbv->val, DBE_VALUE);
            } else {
                printf("%d: WARNING: No readback entry!!\n", i);
            }
        }
    }
};

static int PGPHandlerThread(void *p)
{
    PGPCARD *pgp = (PGPCARD *)p;
    fd_set orig, fds;
    int cfgfd, pgpfd, mxfd;
    struct aSubRecord *cfgrec;

    FD_ZERO(&orig);
    cfgfd = pgp->cfgpipe[0];
    pgpfd = pgp->pgp->fd();
    if (pgpfd > cfgfd)
        mxfd = pgpfd + 1;
    else
        mxfd = cfgfd + 1;
    FD_SET(cfgfd, &orig);
    FD_SET(pgpfd, &orig);

    pgp->pgp->flushInputQueue(false);
    for (;;) {
        fds = orig;
        if (select(mxfd, &fds, NULL, NULL, NULL) <= 0) {
            printf("Select failed?\n");
            continue;
        }
        if (FD_ISSET(cfgfd, &fds)) {
            read(cfgfd, &cfgrec, sizeof(cfgrec));
            if (cfgrec) {
                pgp->disableSrc();
                pgp->pgp->flushInputQueue(false);
                pgp->doConfigure();
                pgp->enableSrc();
                pgp->cfgstate = PGPCARD::CfgDone;
                cfgrec->pact = FALSE;
                dbScanLock((dbCommon *)cfgrec);
                dbProcess((dbCommon *)cfgrec);
                dbScanUnlock((dbCommon *)cfgrec);
            } else {
                sleep(1);  // This is for debugging!
            }
        }
        if (FD_ISSET(pgpfd, &fds)) {
            pgp_data *d = (*pgp->src.rcvfunc)(PGP_Alloc, pgp->src.dev_token);
            d->pgp_token = pgp;
            d->size = d->maxsize;
            RegisterSlaveImportFrame *f = pgp->pgp->do_read(d->data, &d->size);

            if (!f) {
                (*pgp->src.rcvfunc)(PGP_Free, d);
                continue;
            }
#if 0
            printf("DATA %d @%x (acq=%d, seq=%d)\n", d->size, evrGetLastFiducial(), d->data[1], d->data[2]);
#endif
            if (PGP_reg_debug)
                f->print(d->size);
            (*pgp->src.rcvfunc)(PGP_Receive, d);
        }
    }
    return 0; // Never gets here!
}

PGPCARD *pgpFindDeviceByName(char * name)
{
    PGPCARD  *pdevice = NULL;

    epicsMutexLock(PGPlock);
    for(pdevice = (PGPCARD *)ellFirst((ELLLIST *)&PGPCards);
        pdevice;
        pdevice = (PGPCARD *)ellNext((ELLNODE *)pdevice)) {
        if ( 0 == strcmp(name, pdevice->name) )
            break;
    }
    epicsMutexUnlock(PGPlock);

    return pdevice;
}

void *PGP_register_data_source(char *pgp, int lane, int vc, char *trigger, 
                               PGP_rcvfunc rcvfunc, PGP_enfunc enfunc, void *dev_token)
{
    PGPCARD  *pdevice = pgpFindDeviceByName(pgp);

    if (!pdevice) {
        printf("No PGP card named %s!\n", pgp);
        return NULL;
    }
    return pdevice->addSrc(lane, vc, trigger, rcvfunc, enfunc, dev_token);
}

void PGP_receive_data(void *pgp_token, pgp_data *data) {
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    struct srchandler *src = &pgp->src;
    src->data = data;
    scanIoRequest(src->ioscan);
}

unsigned PGP_register_write(void *pgp_token, int lane, int vc, unsigned addr, unsigned val)
{
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    Destination d((lane << 2) | (vc & 3));

    printf("Writing 0x%x to address %d\n", val, addr);
    return pgp->pgp->writeRegister(&d, addr, val, PGP_reg_debug, PgpRSBits::notWaiting);
}

unsigned PGP_register_read(void *pgp_token, int lane, int vc, unsigned addr, unsigned *val)
{
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    Destination d((lane << 2) | (vc & 3));
    unsigned result;

    result = pgp->pgp->readRegister(&d, addr, (lane << 12) + (vc << 8) + addr, val, 1, PGP_reg_debug);
    if (!result)
        printf("Read 0x%x from address %d\n", *val, addr);
    else
        printf("Read from address %d failed!\n", addr);
    return result;
}

unsigned PGP_write_data(void *pgp_token, int lane, int vc, unsigned val)
{
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    Destination d((lane << 2) | (vc & 3));

    printf("Writing 0x%x to vc %d\n", val, vc);
    return pgp->pgp->writeData(&d, val, PGP_reg_debug);
}

unsigned PGP_write_data_bulk(void *pgp_token, int lane, int vc, unsigned* data, unsigned size)
{
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    Destination d((lane << 2) | (vc & 3));

    printf("Writing");
    for (unsigned i=0; i<size; i++) {
      printf(" 0x%x", data[i]);
    }
    printf(" to vc %d\n", vc);
    return pgp->pgp->writeDataBlock(&d, data, size, PGP_reg_debug);
}

void PGP_pause(void *pgp_token)
{
    PGPCARD *pgp = (PGPCARD *)pgp_token;
    void *r = NULL;
    write(pgp->cfgpipe[1], &r, sizeof(r));  // Wake up the thread and make him sleep!
    usleep(5000);
}

static int bit[16] = {
    -1,  0,  1, -1,  2, -1, -1, -1,
     3, -1, -1, -1, -1, -1, -1, -1,
};

void PGP_readreg(int mask, int vcm, int srpV3, unsigned int addr, unsigned int* value)
{
    if (bit[vcm] < 0) {
        printf("PGP_writereg: illegal vcm = %x\n", vcm);
        return;
    }
    Destination d(bit[vcm]); // Lane is always zero! */
    Pgp *pgp = NULL;
    try {
        pgp = new Pgp(mask, vcm, srpV3);
        pgp->readRegister(&d, addr, 0x4200, value, 1, PGP_reg_debug);
        delete pgp;
    }
    catch(char const *s) {
        printf("PGP_readreg: %s\n", s);
    }
}

void PGP_readreg_bulk(int mask, int vcm, int srpV3, pgp_reg_rdata* reg_data, unsigned int size)
{
    if (bit[vcm] < 0) {
        printf("PGP_writereg: illegal vcm = %x\n", vcm);
        return;
    }
    Destination d(bit[vcm]); // Lane is always zero! */
    Pgp *pgp = NULL;
    unsigned int i;
    try {
        pgp = new Pgp(mask, vcm, srpV3);
        for(i=0; i<size; i++) {
            pgp->readRegister(&d, reg_data[i].addr, 0x4200 + i, reg_data[i].value, reg_data[i].size, PGP_reg_debug);
        }
        delete pgp;
    }
    catch(char const *s) {
        printf("PGP_readreg: %s\n", s);
    }
}

void PGP_writereg(int mask, int vcm, int srpV3, unsigned int addr, unsigned int value)
{
    if (bit[vcm] < 0) {
        printf("PGP_writereg: illegal vcm = %x\n", vcm);
        return;
    }
    Destination d(bit[vcm]); // Lane is always zero! */
    Pgp *pgp = NULL;
    try {
        pgp = new Pgp(mask, vcm, srpV3);
        pgp->writeRegister(&d, addr, value, PGP_reg_debug, PgpRSBits::notWaiting);
        delete pgp;
    }
    catch(char const *s) {
        printf("PGP_writereg: %s\n", s);
    }
}

void PGP_writereg_bulk(int mask, int vcm, int srpV3, pgp_reg_data* reg_data, unsigned int size)
{
    if (bit[vcm] < 0) {
        printf("PGP_writereg: illegal vcm = %x\n", vcm);
        return;
    }
    Destination d(bit[vcm]); // Lane is always zero! */
    Pgp *pgp = NULL;
    unsigned int i;
    try {
        pgp = new Pgp(mask, vcm, srpV3);
        for (i=0; i<size; i++) {
          pgp->writeRegister(&d, reg_data[i].addr, reg_data[i].value, PGP_reg_debug, PgpRSBits::notWaiting);
        }
        delete pgp;
    }
    catch(char const *s) {
        printf("PGP_writereg: %s\n", s);
    }
}

void PGP_writedata(int mask, int vcm, unsigned int* data, unsigned int size)
{
  if (bit[vcm] < 0) {
    printf("PGP_writereg: illegal vcm = %x\n", vcm);
    return;
  }
  Destination d(bit[vcm]); // Lane is always zero! */
  Pgp *pgp = NULL;
  try {
    pgp = new Pgp(mask, vcm, false);
    pgp->writeDataBlock(&d, data, size, PGP_reg_debug);
    delete pgp;
  }
  catch(char const *s) {
      printf("PGP_writedata: %s\n", s);
  }
}

int PGP_has_evr(int mask)
{
  int is_g3 = 0;
  Pgp *pgp = NULL;
  try {
    pgp = new Pgp(mask, 0, false);
    is_g3 = pgp->isG3();
    delete pgp;
  }
  catch(char const *s) {
    printf("PGP_has_evr: %s\n", s);
  }
  return is_g3;
}

void PGP_configure_evr(int mask, unsigned int enable, unsigned int runCode, unsigned int runDelay)
{
    Pgp *pgp = NULL;
    try {
        pgp = new Pgp(mask, 0, false);
        pgp->enableRunTrigger(enable, runCode, runDelay, PGP_reg_debug);
        delete pgp;
    }
    catch(char const *s) {
        printf("PGP_configure_evr: %s\n", s);
    }
}

epicsStatus PgpReport(int level)
{
    /* Print some debug stuff. */
    return 0;
}

void PgpShutdownFunc (void *arg)
{
    /* Shutdown all cards! */
}

epicsStatus PgpInit (void)
{
    if (!PGPlock) {
        ellInit((ELLLIST *) &PGPCards);
        PGPlock = epicsMutexMustCreate();
        epicsAtExit (&PgpShutdownFunc, NULL);
    }
    return 0;
}

static void PgpStatusCall(const iocshArgBuf * args)
{
    char *name = args[0].sval;
    PGPCARD *pdevice = pgpFindDeviceByName(name);
    Pgp *pgp;
    if (!pdevice) {
	printf("No PGP card %s!\n", name);
	return;
    }
    pgp = pdevice->pgp;
    switch (pgp->type()) {
    case Pgp::G3:
	{
	    PgpStatus status[8];
	    
	    pgp->readStatus(&status[0]);
	    printf("Checking link status of %s:\n", name);
	    printf("    Local: ");
	    for (int i=0; i<8; i++) {
		printf(" %2d", status[i].locLinkReady);
	    }
	    printf("\n");
	    printf("    Remote:");
	    for (int i=0; i<8; i++) {
		printf(" %2d", status[i].remLinkReady);
	    }
	    printf("\n");
	}
	break;
    case Pgp::G2:
	{
	    PgpStatus status[8];
	    
	    pgp->readStatus(&status[0]);
	    printf("Checking link status of %s:\n", name);
	    printf("    Local: ");
	    for (int i=0; i<4; i++) {
		printf(" %2d", status[i].locLinkReady);
	    }
	    printf("\n");
	    printf("    Remote:");
	    for (int i=0; i<4; i++) {
		printf(" %2d", status[i].remLinkReady);
	    }
	    printf("\n");
	}
	break;
    case Pgp::DataDev:
	{
	    PgpStatus status[8];
	    
	    pgp->readStatus(&status[0]);
	    printf("Checking link status of %s:\n", name);
	    printf("    Local: ");
	    for (int i=0; i<4; i++) {
		printf(" %2d", status[i].locLinkReady);
	    }
	    printf("\n");
	    printf("    Remote:");
	    for (int i=0; i<4; i++) {
		printf(" %2d", status[i].remLinkReady);
	    }
	    printf("\n");
	}
    default:
	printf("Checking the link status of cards of type %d is unsupported!",
	       pgp->type());
    }
}

void PgpRegister(char *name, int lane, int vcm, int srpV3)
{
    PGPCARD  *pdevice = NULL;

    PgpInit();
    if (!name || pgpFindDeviceByName(name)) {
        printf("PGP card %s is already registered!\n", name);
        epicsThreadSuspendSelf();
    }

    pdevice = new PGPCARD(name, lane, vcm, srpV3);

    epicsMutexLock(PGPlock);
    ellAdd(&PGPCards, (ELLNODE *)pdevice);
    epicsMutexUnlock(PGPlock);
    printf("PGP card %s is registered!\n", name);
}

static long init_li_record(void* record)
{
    longinRecord* r = reinterpret_cast<longinRecord*>(record);
    char boxname[MAX_CA_STRING_SIZE];
    int lane, vc, addr, seq;
    PGPCARD *pgp = NULL;

    if (r->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)r, "devPGP init_li_record, Illegal INP");
        r->pact=TRUE;
        return (S_db_badField);
    }
    if (sscanf(r->inp.value.instio.string, "%[^ ] %i %i %i %i", boxname, &lane, &vc, &addr, &seq) != 5) {
        printf("Badly formated INP for longin record %s: %s\n", r->name, r->inp.value.instio.string);
        r->pact=TRUE;
        return -1;
    }
    pgp = pgpFindDeviceByName(boxname);
    if (!pgp) {
        printf("Cannot find PGP card %s for record %s\n", boxname, r->name);
        r->pact=TRUE;
        return -1;
    }
    r->dpvt = (void *)pgp;
    pgp->addCfgIn(r, lane, vc, addr, seq);
    return 0;
}

static long init_lo_record(void* record)
{
    longoutRecord* r = reinterpret_cast<longoutRecord*>(record);
    char boxname[MAX_CA_STRING_SIZE];
    int lane, vc, addr, seq;
    PGPCARD *pgp = NULL;

    if (r->out.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)r, "devPGP init_lo_record, Illegal OUT");
        r->pact=TRUE;
        return (S_db_badField);
    }
    if (sscanf(r->out.value.instio.string, "%[^ ] %i %i %i %i", boxname, &lane, &vc, &addr, &seq) != 5) {
        printf("Badly formated OUT for longout record %s: %s\n", r->name, r->out.value.instio.string);
        r->pact=TRUE;
        return -1;
    }
    pgp = pgpFindDeviceByName(boxname);
    if (!pgp) {
        printf("Cannot find PGP card %s for record %s\n", boxname, r->name);
        r->pact=TRUE;
        return -1;
    }
    r->dpvt = (void *)pgp;
    pgp->addCfgOut(r, lane, vc, addr, seq);
    return 0;
}

static long init_wf_record(void* record)
{
    waveformRecord* r = reinterpret_cast<waveformRecord*>(record);
    char boxname[MAX_CA_STRING_SIZE], data[MAX_CA_STRING_SIZE];
    int lane, vc;
    PGPCARD *pgp = NULL;

    if (r->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)r, "devPGP init_wf_record, Illegal INP");
        r->pact=TRUE;
        return (S_db_badField);
    }
    if (sscanf(r->inp.value.instio.string, "%[^ ] %i %i %[^ ]", boxname, &lane, &vc, data) != 4 ||
        strcmp(data, "DATA")) {
        printf("Badly formated INP for waveform record %s: %s\n", r->name, r->inp.value.instio.string);
        r->pact=TRUE;
        return -1;
    }
    pgp = pgpFindDeviceByName(boxname);
    if (!pgp) {
        printf("Cannot find PGP card %s for record %s\n", boxname, r->name);
        r->pact=TRUE;
        return -1;
    }
    r->dpvt = (void *)pgp;
    return 0;
}

static long read_li(void *record)
{
    /* Don't actually *do* anything!!! */
    return 0;
}

static long write_lo(void* record)
{
    /* Don't actually *do* anything!!! */
    return 0;
}

static long read_wf(void *record)
{
    waveformRecord* r = reinterpret_cast<waveformRecord*>(record);
    PGPCARD *pgp = (PGPCARD *)r->dpvt;
    if (!pgp->src.data) {
        r->nsta = UDF_ALARM;
        r->nsev = INVALID_ALARM;
        return -1;
    }
    if (pgp->src.data->size > (int) r->nelm) {
	printf("PGP size = %d, nelm = %d?\n", pgp->src.data->size, r->nelm);
	pgp->src.data->size = r->nelm;
    }
    memcpy(r->bptr, pgp->src.data->data, pgp->src.data->size * sizeof(unsigned));
    r->nord = pgp->src.data->size;
    r->udf = FALSE;
    r->time = pgp->src.data->time;
    pgp->src.data = NULL;
    return 0;
}

static long get_wf_ioint(int cmd, void *record, IOSCANPVT *iopvt)
{
    waveformRecord* r = reinterpret_cast<waveformRecord*>(record);
    PGPCARD *pgp = (PGPCARD *)r->dpvt;
    *iopvt = pgp->src.ioscan;
    return 0;
}

long PGPDoConfigure(struct aSubRecord *psub)
{
    if (psub->fta != DBF_STRING) {
        recGblSetSevr(psub, LINK_ALARM, INVALID_ALARM);
        printf("There is illegal input for INPs of [%s]!\n", psub->name);
        return -1;
    }
    char *boxname = (char *)psub->a;
    PGPCARD *pgp = pgpFindDeviceByName(boxname);
    if (!pgp) {
        printf("Cannot find PGP card %s for record %s\n", boxname, psub->name);
        psub->pact=TRUE;
        return -1;
    }
    pgp->configure(psub);
    return 0;
}

epicsRegisterFunction(PGPDoConfigure);

drvet drvPGP =
{
    2,                   /* Number of entries in the table                     */
    (DRVSUPFUN)PgpReport,/* Driver Support Layer device report routine         */
    (DRVSUPFUN)PgpInit   /* Driver Support layer device initialization routine */
};

epicsExportAddress (drvet, drvPGP);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_longin;
} devPGPlin = {
    5,
    NULL,
    NULL,
    init_li_record,
    NULL,
    read_li
};

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write_longout;
} devPGPlout = {
    5,
    NULL,
    NULL,
    init_lo_record,
    NULL,
    write_lo
};

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    long (*get_ioint_info)(int, void *, IOSCANPVT *);
    DEVSUPFUN read_wf;
} devPGPwave = {
    5,
    NULL,
    NULL,
    init_wf_record,
    get_wf_ioint,
    read_wf
};

epicsExportAddress(dset, devPGPlin);
epicsExportAddress(dset, devPGPlout);
epicsExportAddress(dset, devPGPwave);

/* iocsh command: PgpRegister */
static const iocshArg PgpRegisterArg0 = {"name"   ,    iocshArgString};
static const iocshArg PgpRegisterArg1 = {"lane"   ,    iocshArgInt};
static const iocshArg PgpRegisterArg2 = {"vcm"    ,    iocshArgInt};
static const iocshArg PgpRegisterArg3 = {"srpV3"  ,    iocshArgInt};
static const iocshArg *const PgpRegisterArgs[4] = {
    &PgpRegisterArg0,
    &PgpRegisterArg1,
    &PgpRegisterArg2,
    &PgpRegisterArg3,
};
static const iocshFuncDef PgpRegisterDef = {"PgpRegister", 4, PgpRegisterArgs};

static void PgpRegisterCall(const iocshArgBuf * args)
{
    PgpRegister(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

static const iocshArg PgpStatusArg0 = {"name"   ,    iocshArgString};
static const iocshArg *const PgpStatusArgs[1] = {
    &PgpStatusArg0,
};
static const iocshFuncDef PgpStatusDef = {"PgpStatus", 1, PgpStatusArgs};

/* Registration APIs */
static void drvPGPRegister()
{
    /* register APIs */
    iocshRegister(	&PgpRegisterDef,	PgpRegisterCall );
    iocshRegister(	&PgpStatusDef,   	PgpStatusCall   );
}
epicsExportRegistrar(drvPGPRegister);
epicsExportAddress(int, PGP_reg_debug);
};
