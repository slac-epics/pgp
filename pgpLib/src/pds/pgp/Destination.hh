/*
 * Destination.hh
 *
 *  Created on: Apr 13, 2011
 *      Author: jackp
 */

#ifndef DESTINATION_HH_
#define DESTINATION_HH_

#include <string.h>

namespace Pds {

  namespace Pgp {

    class Destination {
      public:
        Destination() { _dest = 0; };
        Destination(unsigned d) { _dest = d; };
        virtual ~Destination() {};

      public:
        void dest(unsigned d) { _dest = d; }
        unsigned dest() { return _dest; }
        virtual unsigned lane() { return( (_dest>>2) & 0x7); }
        virtual unsigned vc() { return _dest & 0x3; }
        virtual const char*    name() {
          static char _ret[80];
          static const char* _lanes[9] = {
              "Lane 0, ",
              "Lane 1, ",
              "Lane 2, ",
              "Lane 3, ",
              "Lane 4, ",
              "Lane 5, ",
              "Lane 6, ",
              "Lane 7, ",
              "--INVALID--"
          };
          static const char* _vcs[5] = {
              "VC 0",
              "VC 1",
              "VC 2",
              "VC 3",
              "--INVALID--"
          };
          strcpy(_ret, _lanes[lane()]);
          return strcat(_ret, _vcs[vc()]);
        }

      protected:
        unsigned _dest;
    };

  }

}

#endif /* DESTINATION_HH_ */
