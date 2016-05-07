#include "ns3/hello-request.h"

using namespace ns3;

    LSTableMsg (uint32_t sequenceNumber, Time timestamp,  
      int nOfNeighbors, std::vector<Ipv4Address> neighbors);
{
  m_sequenceNumber = sequenceNumber;
  m_timestamp = timestamp;
  m_nOfNeighbors = nOfNeighbors;
  m_neighbors = neighbors;
}

LSTableMsg::~LSTableMsg ()
{
}

Time
LSTableMsg::GetTimestamp ()
{
  return m_timestamp;
}

uint32_t
LSTableMsg::GetSequenceNumber ()
{
  return m_sequenceNumber;
}

uint32_t
LSTableMsg::GetNOfNeighbors ()
{
  return m_nOfNeighbors;
}

std::vector<Ipv4Address>
LSTableMsg::GetNeighbors ()
{
  return m_neighbors;
}
