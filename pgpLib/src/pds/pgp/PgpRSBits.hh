/*
 * PgpRSBits.h
 *
 *  Created on: Mar 7, 2011
 *      Author: jackp
 */

#ifndef PGPRSBITS_H_
#define PGPRSBITS_H_

namespace Pds {

  namespace Pgp {

    class PgpRSBits {
      public:
        PgpRSBits() {};
        ~PgpRSBits() {};

        enum opcode {read, write, set, clear, numberOfOpCodes};
        enum waitState {notWaiting, Waiting};

      public:
        unsigned tid()                            {return _tid;}
        void tid(unsigned t)                      {_tid = t & 0x7fffff;}
        waitState waiting()                       {return _waiting;}
        void waiting(waitState w)                 {_waiting = w;}
        unsigned lane()                           {return _lane;}
        void lane(unsigned l)                     {_lane = l & 3;}
        unsigned vc()                             {return _vc;}
        void vc(unsigned v)                       {_vc = v & 3;}
        unsigned addr()                           {return _addr;}
        void addr(unsigned a)                     {_addr = a & 0xffffff;}


      public:
        unsigned  _vc:      2;    // 1:0
        unsigned   mbz:     4;    // 5:2
        unsigned  _lane:    2;    // 7:6
        waitState _waiting: 1;    // 8
        unsigned  _tid:    23;    //31:9

        unsigned  _addr:   24;    //23:0
        unsigned   dnc:     6;    //29:24
        unsigned   oc:      2;    //31:30
    };

  }

}

#endif /* PGPRSBITS_H_ */
