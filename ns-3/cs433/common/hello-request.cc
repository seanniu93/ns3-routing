#include "ns3/hello-request.h"

using namespace ns3;

HelloRequest::HelloRequest (uint32_t sequenceNumber, Time timestamp, Ipv4Address destinationAddress, std::string helloMessage)
{
  m_sequenceNumber = sequenceNumber;
  m_timestamp = timestamp;
  m_helloMessage = helloMessage;
  m_destinationAddress = destinationAddress;
}

HelloRequest::~HelloRequest ()
{
}

Time
HelloRequest::GetTimestamp ()
{
  return m_timestamp;
}

uint32_t
HelloRequest::GetSequenceNumber ()
{
  return m_sequenceNumber;
}

std::string
HelloRequest::GetHelloMessage ()
{
  return m_helloMessage;
}

Ipv4Address
HelloRequest::GetDestinationAddress ()
{
  return m_destinationAddress;
}
