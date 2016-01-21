#include<epicsExport.h>
#include<registryFunction.h>
#include<epicsStdio.h>
#include<epicsExit.h>
#include<epicsStdlib.h>
#include<epicsString.h>
#include<epicsThread.h>
#include<devSup.h>
#include<drvSup.h>
#include<iocsh.h> 
#include<link.h>
#include<dbAccess.h>
#include<dbScan.h>
#include<aSubRecord2.h>  // Sigh.  A copy of aSubRecord.h, with not -> not2 so it is legal C++
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

#define MAX_CA_STRING_SIZE 40

using namespace Pds::Pgp;

extern "C" {

static ELLLIST PGPCards = {{ NULL, NULL }, 0};
static epicsMutexId PGPlock = NULL;

static int PGPHandlerThread(void *p);

class PGPCARD {
public:
    enum Params { CFGINC = 10 };
    ELLNODE node;
    char *name;
    epicsUInt32 *trig;
    epicsUInt32 *gen;
    DBADDR addr;
    Pgp *pgp;
    IOSCANPVT ioscan;
    int cfgpipe[2];
    int cfgsize;
    int cfgmax;
    struct cfgelem {
        struct longinRecord  *rbv;
        struct longoutRecord *val;
        int lane;
        int vc;
        unsigned addr;
    } *cfg;

    PGPCARD(char *_name, int _lane, char *_trigger, char *_enable) {
        name = epicsStrDup(_name);
        if (dbNameToAddr(_trigger, &addr)) {
            printf("No PV trigger named %s!\n", _trigger);
            epicsThreadSuspendSelf();
        }
        trig = (epicsUInt32 *) addr.pfield;
        gen = trig + MAX_EV_TRIGGERS;
        printf("Found PV trigger for PGP %s\n", name);
        if (dbNameToAddr(_enable, &addr)) {
            printf("No PV trigger enable named %s!\n", _enable);
            epicsThreadSuspendSelf();
        }
        pgp = new Pgp(_lane);
        scanIoInit(&ioscan);
        if (pipe(cfgpipe) == -1) {
            printf("pipe creation failed!\n");
            epicsThreadSuspendSelf();
        }
        cfgsize = CFGINC;
        cfgmax  = -1;
        cfg = (struct cfgelem *)malloc(CFGINC*sizeof(struct cfgelem));
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
            cfgsize = (seq + CFGINC - 1) / CFGINC;
            cfgsize *= CFGINC;
            cfg = (struct cfgelem *)realloc(cfg, cfgsize*sizeof(struct cfgelem));
        }
        while (seq > cfgmax) {
            cfgmax++;
            cfg[cfgmax].rbv = NULL;
            cfg[cfgmax].val = NULL;
        }
    }
public:
    void addCfgIn(struct longinRecord *r, int seq) {
        addCfg(seq);
        cfg[seq].rbv = r;
    }
    void addCfgOut(struct longoutRecord *r, int lane, int vc, int seq) {
        addCfg(seq);
        cfg[seq].val = r;
        cfg[seq].lane = lane;
        cfg[seq].vc = vc;
    }
    void configure(struct aSubRecord *r) {
        r->pact = TRUE;
        write(cfgpipe[1], &r, sizeof(r));  // Wake up the thread, and tell him who is calling!
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
        mxfd = pgpfd;
    else
        mxfd = cfgfd;
    FD_SET(cfgfd, &orig);
    FD_SET(pgpfd, &orig);
    for (;;) {
        fds = orig;
        if (select(mxfd, &fds, NULL, NULL, NULL) <= 0)
            continue;
        if (FD_ISSET(cfgfd, &fds)) {
            read(cfgfd, &cfgrec, sizeof(cfgrec));
            /* MCB - Configure the card! */
            cfgrec->pact = FALSE;
            /* MCB - Re-process cfgrec! */
        }
        if (FD_ISSET(pgpfd, &fds)) {
            /* MCB - Read some data! */
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

void PgpRegister(char *name, int lane, char *trigger, char *enable)
{
    PGPCARD  *pdevice = NULL;

    if (!name || pgpFindDeviceByName(name)) {
        printf("PGP card %s is already registered!\n", name);
        epicsThreadSuspendSelf();
    }

    pdevice = new PGPCARD(name, lane, trigger, enable);

    epicsMutexLock(PGPlock);
    ellAdd(&PGPCards, (ELLNODE *)pdevice);
    epicsMutexUnlock(PGPlock);
}

epicsStatus PgpReport(int level)
{
    /* MCB - Print some debug stuff. */
    return 0;
}

void PgpShutdownFunc (void *arg)
{
    /* MCB - Shutdown all cards! */
}

epicsStatus PgpInit (void)
{
    ellInit((ELLLIST *) &PGPCards);
    PGPlock = epicsMutexMustCreate();
    epicsAtExit (&PgpShutdownFunc, NULL);
    return 0;
}

static long init_li_record(void* record)
{
    longinRecord* r = reinterpret_cast<longinRecord*>(record);
    char boxname[MAX_CA_STRING_SIZE];
    int seq;
    PGPCARD *pgp = NULL;

    if (r->inp.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)r, "devPGP init_li_record, Illegal INP");
        r->pact=TRUE;
        return (S_db_badField);
    }
    if (sscanf(r->inp.value.instio.string, "%[^ ] %i", boxname, &seq) != 2) {
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
    pgp->addCfgIn(r, seq);
    return 0;
}

static long init_lo_record(void* record)
{
    longoutRecord* r = reinterpret_cast<longoutRecord*>(record);
    char boxname[MAX_CA_STRING_SIZE];
    int lane, vc, seq;
    PGPCARD *pgp = NULL;

    if (r->out.type != INST_IO) {
        recGblRecordError(S_db_badField, (void *)r, "devPGP init_lo_record, Illegal OUT");
        r->pact=TRUE;
        return (S_db_badField);
    }
    if (sscanf(r->out.value.instio.string, "%[^ ] %i %i %i", boxname, &lane, &vc, &seq) != 4) {
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
    pgp->addCfgOut(r, lane, vc, seq);
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
    /* MCB - Read the waveform? */
    return 0;
}

static long get_wf_ioint(int cmd, void *record, IOSCANPVT *iopvt)
{
    waveformRecord* r = reinterpret_cast<waveformRecord*>(record);
    PGPCARD *pgp = (PGPCARD *)r->dpvt;
    *iopvt = pgp->ioscan;
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
static const iocshArg PgpRegisterArg2 = {"trigger",    iocshArgString};
static const iocshArg PgpRegisterArg3 = {"trigenable", iocshArgString};
static const iocshArg *const PgpRegisterArgs[4] = {
    &PgpRegisterArg0,
    &PgpRegisterArg1,
    &PgpRegisterArg2,
    &PgpRegisterArg3,
};
static const iocshFuncDef PgpRegisterDef = {"PgpRegister", 4, PgpRegisterArgs};

static void PgpRegisterCall(const iocshArgBuf * args)
{
    PgpRegister(args[0].sval, args[1].ival, args[2].sval, args[3].sval);
}

/* Registration APIs */
static void drvPGPRegister()
{
    /* register APIs */
    iocshRegister(	&PgpRegisterDef,	PgpRegisterCall );
}
epicsExportRegistrar(drvPGPRegister);

};
