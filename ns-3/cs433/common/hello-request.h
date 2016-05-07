#ifndef TESTFILE_H_
#define TESTFILE_H_

#include <string>
#include "ns3/type-name.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

class HelloRequest : public SimpleRefCount<HelloRequest>
{
  public:
    HelloRequest (uint32_t sequenceNumber, Time timestamp, Ipv4Address destinationAddress, std::string helloMessage, Time timeout);
    ~HelloRequest ();

    Time GetTimestamp ();
    uint32_t GetSequenceNumber ();
    std::string GetHelloMessage ();
    Ipv4Address GetDestinationAddress ();
    Time GetTimeout ();

  private:
    Time m_timestamp;
    uint32_t m_sequenceNumber;
    std::string m_helloMessage;
    Ipv4Address m_destinationAddress;
    Time m_timeout; //interval between HELLO broadcasts -- should be the same for all nodes in the network [cf. RFC 2328]
};

#endif