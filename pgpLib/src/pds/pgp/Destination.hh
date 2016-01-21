/*
 * Destination.hh
 *
 *  Created on: Apr 13, 2011
 *      Author: jackp
 */

#ifndef DESTINATION_HH_
#define DESTINATION_HH_

namespace Pds {

  namespace Pgp {

    class Destination {
      public:
        Destination() {};
        Destination(unsigned d) { _dest = d; };
        virtual ~Destination() {};

      public:
        void dest(unsigned d) { _dest = d; }
        virtual unsigned lane() { return( (_dest>>2) & 3); }
        virtual unsigned vc() { return _dest & 3; }
        virtual const char*    name() {
          static const char* _names[17] = {
              "Lane 0, VC 0",
              "Lane 0, VC 1",
              "Lane 0, VC 2",
              "Lane 0, VC 3",
              "Lane 1, VC 0",
              "Lane 1, VC 1",
              "Lane 1, VC 2",
              "Lane 1, VC 3",
              "Lane 2, VC 0",
              "Lane 2, VC 1",
              "Lane 2, VC 2",
              "Lane 2, VC 3",
              "Lane 3, VC 0",
              "Lane 3, VC 1",
              "Lane 3, VC 2",
              "Lane 3, VC 3",
              " -- INVALID --"
          };
          return (_dest < 16 ? _names[_dest] : _names[16]);
        }

      protected:
        unsigned _dest;
    };

  }

}

#endif /* DESTINATION_HH_ */
