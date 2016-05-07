#ifndef TESTFILE_H_
#define TESTFILE_H_

#include <string>
#include "ns3/type-name.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"

using namespace ns3;
/* This is the message that is broadcast by each node in the network and contains
 * that node's list of adjacent neighbors. The information about this list is contained in
 * two fields: one is an integer nOfNeighbors specifying the number of neighbors, and the other is an array
 * neighbors containing the Ipv4Addresses of each neighbor.
 */
class LSTableMsg : public SimpleRefCount<HelloRequest>
{
  public:
    LSTableMsg (uint32_t sequenceNumber, Time timestamp, uint32_t nOfNeighbors, std::vector<Ipv4Address> neighbors);
    ~LSTableMsg ();

    Time GetTimestamp ();
    uint32_t GetSequenceNumber ();
    uint32_t GetNOfNeighbors();
    std::vector<Ipv4Address> GetNeighbors ();

  private:
    Time m_timestamp;
    uint32_t m_sequenceNumber;
    std::vector<Ipv4Address> m_destinationAddress;

};

#endif