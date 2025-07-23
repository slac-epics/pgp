/*
 * Destination.hh
 *
 *  Created on: Apr 13, 2011
 *      Author: jackp
 */

#ifndef DESTINATION_HH_
#define DESTINATION_HH_

#include <string>

namespace Pds {

  namespace Pgp {

    class Destination {
      public:
        Destination();
        Destination(bool datadev, unsigned d);
        Destination(bool datadev, unsigned lane, unsigned vc);
        virtual ~Destination();

      public:
        void offset(unsigned o);
        void dest(unsigned d);
        void dest(unsigned lane, unsigned vc);
        unsigned dest() const;
        virtual unsigned lane() const;
        virtual unsigned vc() const;
        virtual std::string name() const;

      protected:
        unsigned _offset;
        unsigned _dest;
    };

  }

}

#endif /* DESTINATION_HH_ */
