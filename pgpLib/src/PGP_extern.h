#include<dbScan.h>

extern "C" {
    IOSCANPVT *PGP_register_data_source(char *pgp, int lane, int vc, char *trigger, void (*rcvfunc)(void *, int, void *), void *user);
};
