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
        void          *pgp_token;     /* Value returned from PGP_register_data_source */
        void          *dev_token;     /* Value used by device driver */
        unsigned      *data;
        int            size;          /* Number of unsigned, not number of bytes!!! */
        int            maxsize;
        epicsTimeStamp time;
    } pgp_data;

    typedef void (*PGP_enfunc)(int, void *);
    enum PGP_rcvfunc_op { PGP_Alloc, PGP_Free, PGP_Receive };
    typedef pgp_data *(*PGP_rcvfunc)(enum PGP_rcvfunc_op, void *);

    //
    // void enfunc(int enable, void *dev_token)
    //       - Call back to the device to enable/disable it.
    // pgp_data *rcvfunc(enum PGP_rcvfunc op, void *dev_token_or_pgp_data)
    //       - Operation depends on value of op:
    //           PGP_Alloc   - Allocate a new pgp_data structure, and return it. (NULL = Failure.)
    //           PGP_Free    - Free a pgp_data structure. (Always returns NULL.)
    //           PGP_Receive - Receive the data in the pgp_data structure. (Always returns NULL.)

    void *PGP_register_data_source(char *pgp, int lane, int vc, char *trigger,
                                   PGP_rcvfunc rcvfunc, PGP_enfunc enfunc, void *dev_token);
    void  PGP_receive_data(void *token, pgp_data *data);
    unsigned PGP_register_write(void *token, int lane, int vc, unsigned addr, unsigned value);
    unsigned PGP_register_read(void *token, int lane, int vc, unsigned addr, unsigned *value);
    void PGP_pause(void *token);  // For debugging!

    // One-off register write function.
    void PGP_writereg(int mask, int vcm, int g3, unsigned int addr, unsigned int value);
};
