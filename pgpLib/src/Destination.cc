#include "pds/pgp/Destination.hh"

static const unsigned VC_MASK = 0x3;
static const unsigned LANE_MASK = 0x7;
static const unsigned PGP_DEST_OFFSET = 2;
static const unsigned DATADEV_DEST_OFFSET = 8;

static std::string LANE_NAMES[] = {
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
static std::string VC_NAMES[] = {
  "VC 0",
  "VC 1",
  "VC 2",
  "VC 3",
  "--INVALID--"
};

using namespace Pds::Pgp;

Destination::Destination() :
  _offset(PGP_DEST_OFFSET),
  _dest(0)
{}

Destination::Destination(bool datadev, unsigned d) :
  _offset(datadev ? DATADEV_DEST_OFFSET : PGP_DEST_OFFSET),
  _dest(d)
{}

Destination::Destination(bool datadev, unsigned lane, unsigned vc) :
  _offset(datadev ? DATADEV_DEST_OFFSET : PGP_DEST_OFFSET),
  _dest(0)
{
  dest(lane, vc);
}

Destination::~Destination()
{}

void Destination::offset(unsigned o)
{
  _offset = o;
}

void Destination::dest(unsigned d)
{
  _dest = d;
}

void Destination::dest(unsigned lane, unsigned vc)
{
  _dest = (lane & LANE_MASK)<<_offset | (vc & VC_MASK);
}

unsigned Destination::dest() const
{
  return _dest;
}

unsigned Destination::lane() const
{
  return (_dest>>_offset) & LANE_MASK; 
}

unsigned Destination::vc() const
{
  return _dest & VC_MASK;
}

std::string Destination::name() const
{
  return LANE_NAMES[lane()] + VC_NAMES[vc()];
}
