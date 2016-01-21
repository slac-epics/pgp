#include<epicsExport.h>
#include<registryFunction.h>
#include<epicsStdio.h>
#include<epicsExit.h>
#include<epicsStdlib.h>
#include<epicsThread.h>
#include<devSup.h>
#include<drvSup.h>
#include<iocsh.h> 
#include<link.h>
#include<dbAccess.h>
#include<dbScan.h>
#include<longinRecord.h>
#include<longoutRecord.h>
#include<ellLib.h>
#include<string.h>

#ifdef  __cplusplus
extern "C" {
#endif  /*      __cplusplus     */

static ELLLIST PGPCards = {{ NULL, NULL }, 0};
static epicsMutexId PGPlock = NULL;

typedef struct PGPstruct {
    ELLNODE node;
    char *name;
} PGPCARD;

static void *tmp;

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

void PgpRegister(char *name, int lane, char *trigger)
{
    /* MCB - Declare a PGP card. */
    if (!name || pgpFindDeviceByName(name)) {
        printf("PGP card %s is already registered!\n", name);
        epicsThreadSuspendSelf();
    }
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
    /* MCB - Initialize longin */
    tmp = r;
    return 0;
}

static long init_lo_record(void* record)
{
    longoutRecord* r = reinterpret_cast<longoutRecord*>(record);
    /* MCB - Initialize longout */
    tmp = r;
    return 0;
}

static long get_li_ioint(int cmd, void *record, IOSCANPVT *iopvt)
{
    longinRecord* r = reinterpret_cast<longinRecord*>(record);
    /* MCB - Assign to *iopvt */
    tmp = r;
    return 0;
}

static long read_li(void *record)
{
    longinRecord* r = reinterpret_cast<longinRecord*>(record);
    /* MCB - Read a value! */
    tmp = r;
    return 0;
}

static long write_lo(void* record)
{
    longoutRecord* r = reinterpret_cast<longoutRecord*>(record);
    /* MCB - Write a value */
    tmp = r;
    return 0;
}

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
    long (*get_ioint_info)(int, void *, IOSCANPVT *);
    DEVSUPFUN read_longin;
} devPGPlin = {
    5,
    NULL,
    NULL,
    init_li_record,
    get_li_ioint,
    read_li
};

epicsExportAddress(dset, devPGPlin);

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

epicsExportAddress(dset, devPGPlout);

/* iocsh command: PgpRegister */
static const iocshArg PgpRegisterArg0 = {"name"   , iocshArgString};
static const iocshArg PgpRegisterArg1 = {"lane"   , iocshArgInt};
static const iocshArg PgpRegisterArg2 = {"trigger", iocshArgString};
static const iocshArg *const PgpRegisterArgs[3] = {
    &PgpRegisterArg0,
    &PgpRegisterArg1,
    &PgpRegisterArg2,
};
static const iocshFuncDef PgpRegisterDef = {"PgpRegister", 3, PgpRegisterArgs};

static void PgpRegisterCall(const iocshArgBuf * args)
{
    PgpRegister(args[0].sval, args[1].ival, args[2].sval);
}

/* Registration APIs */
static void drvPGPRegister()
{
    /* register APIs */
    iocshRegister(	&PgpRegisterDef,	PgpRegisterCall );
}
epicsExportRegistrar(drvPGPRegister);

#ifdef  __cplusplus
};
#endif  /*      __cplusplus     */
