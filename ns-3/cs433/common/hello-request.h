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
    HelloRequest (uint32_t sequenceNumber, Time timestamp, Ipv4Address destinationAddress, std::string helloMessage);
    ~HelloRequest ();

    Time GetTimestamp ();
    uint32_t GetSequenceNumber ();
    std::string GetHelloMessage ();
    Ipv4Address GetDestinationAddress ();

  private:
    Time m_timestamp;
    uint32_t m_sequenceNumber;
    std::string m_helloMessage;
    Ipv4Address m_destinationAddress;
};

#endif