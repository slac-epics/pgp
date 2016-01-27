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
#include"PGP_extern.h"

#define MAX_CA_STRING_SIZE 40

using namespace Pds::Pgp;

extern "C" {

static ELLLIST PGPCards = {{ NULL, NULL }, 0};
static epicsMutexId PGPlock = NULL;

static int PGPHandlerThread(void *p);

class PGPCARD;

struct srchandler {
    PGPCARD *pgp;
    int lane;
    int vc;
    IOSCANPVT ioscan;
    DBADDR trigenable;
    void (*rcvfunc)(void *, int, void *);
    void *user;
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
    } *cfg;
    int srcsize;
    int srcmax;
    struct srchandler *src;

    PGPCARD(char *_name, int _lane) {
        name = epicsStrDup(_name);
        pgp = new Pgp(_lane);
        if (pipe(cfgpipe) == -1) {
            printf("pipe creation failed!\n");
            epicsThreadSuspendSelf();
        }
        cfgsize = CFGINC;
        cfgmax  = -1;
        cfg = (struct cfgelem *)malloc(CFGINC*sizeof(struct cfgelem));
        srcsize = SRCINC;
        srcmax  = -1;
        src = (struct srchandler *)malloc(SRCINC*sizeof(struct srchandler));
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
    void *addSrc(int lane, int vc, char *trigger, void (*rcvfunc)(void *, int, void *), void *user) {
        if (srcmax == srcsize) {
            srcsize += SRCINC;
            src = (struct srchandler *)realloc(src, srcsize*sizeof(struct srchandler));
        }
        src[srcmax].pgp     = this;
        src[srcmax].lane    = lane;
        src[srcmax].vc      = vc;
        src[srcmax].rcvfunc = rcvfunc;
        src[srcmax].user    = user;
        src[srcmax].data    = NULL;
        scanIoInit(&src[srcmax].ioscan);
        if (dbNameToAddr(trigger, &src[srcmax].trigenable)) {
            printf("No PV trigger named %s!\n", trigger);
            return NULL;
        } else
            return (void *)&src[srcmax++];
    }
    int findSrc(int lane, int vc) {
        int i;
        for (i = 0; i < srcmax; i++) {
            if (src[i].lane == lane && src[i].vc == vc)
                return i;
        }
        return -1;
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
            /* MCB - Read some data and pass it to the correct handler! */
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

void *PGP_register_data_source(char *pgp, int lane, int vc, char *trigger, void (*rcvfunc)(void *, int, void *), void *user)
{
    PGPCARD  *pdevice = pgpFindDeviceByName(pgp);

    if (!pdevice) {
        printf("No PGP card named %s!\n", pgp);
        return NULL;
    }
    return pdevice->addSrc(lane, vc, trigger, rcvfunc, user);
}

void PGP_receive_data(void *p, pgp_data *data) {
    struct srchandler *src = (struct srchandler *)p;
    src->data = data;
    scanIoRequest(src->ioscan);
}

void PgpRegister(char *name, int lane)
{
    PGPCARD  *pdevice = NULL;

    if (!name || pgpFindDeviceByName(name)) {
        printf("PGP card %s is already registered!\n", name);
        epicsThreadSuspendSelf();
    }

    pdevice = new PGPCARD(name, lane);

    epicsMutexLock(PGPlock);
    ellAdd(&PGPCards, (ELLNODE *)pdevice);
    epicsMutexUnlock(PGPlock);
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
    int lane, vc, i;
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
    i = pgp->findSrc(lane, vc);
    if (i >= 0) {
        r->dpvt = (void *)&pgp->src[i];
    } else {
        printf("Cannot find data source for record %s (lane %d, vc %d)!\n", r->name, lane, vc);
        r->dpvt = NULL;
    }
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
    struct srchandler *src = (struct srchandler *)r->dpvt;
    if (!src || !src->data) {
        r->nsta = UDF_ALARM;
        r->nsev = INVALID_ALARM;
        return -1;
    }
    memcpy(r->bptr, src->data->data, src->data->size * sizeof(unsigned));
    r->nord = src->data->size;
    r->udf = FALSE;
    r->time = src->data->time;
    src->data = NULL;
    return 0;
}

static long get_wf_ioint(int cmd, void *record, IOSCANPVT *iopvt)
{
    waveformRecord* r = reinterpret_cast<waveformRecord*>(record);
    struct srchandler *src = (struct srchandler *)r->dpvt;
    if (!src)
        return -1;
    *iopvt = src->ioscan;
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
static const iocshArg *const PgpRegisterArgs[2] = {
    &PgpRegisterArg0,
    &PgpRegisterArg1,
};
static const iocshFuncDef PgpRegisterDef = {"PgpRegister", 2, PgpRegisterArgs};

static void PgpRegisterCall(const iocshArgBuf * args)
{
    PgpRegister(args[0].sval, args[1].ival);
}

/* Registration APIs */
static void drvPGPRegister()
{
    /* register APIs */
    iocshRegister(	&PgpRegisterDef,	PgpRegisterCall );
}
epicsExportRegistrar(drvPGPRegister);

};
