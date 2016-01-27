#include<dbScan.h>
#include<epicsTime.h>

extern "C" {
    /*
     * PGP_register_data_source is used to register a particular device with the PGP module.
     * The idea is whenever the specified PGP card receives data on the given lane/vc, it should
     * call the receive function, rcvfunc, with the data pointer, size, and the given user pointer.
     *
     * This returns an opaque pointer which should be passed to PGP_receive_data to actually
     * enqueue the received data with a timestamp.
     */

    typedef struct pgp_data_struct {
        void          *token;         /* Value returned from PGP_register_data_source */
        unsigned       data[2048];
        int            size;          /* Number of unsigned, not number of bytes!!! */
        epicsTimeStamp time;
    } pgp_data;

    void *PGP_register_data_source(char *pgp, int lane, int vc, char *trigger, void (*rcvfunc)(void *, int, void *), void *user);
    void  PGP_receive_data(void *, pgp_data *);
};
