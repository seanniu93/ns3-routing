#include "ns3/ls-table-msg.h"

using namespace ns3;

LSTableMessage::LSTableMessage (uint32_t sequenceNumber, Time timestamp, std::vector<Ipv4Address> neighbors)
{
  m_sequenceNumber = sequenceNumber;
  m_timestamp = timestamp;
  m_neighbors = neighbors;
}

LSTableMessage::~LSTableMessage ()
{
}

Time
LSTableMessage::GetTimestamp ()
{
  return m_timestamp;
}

uint32_t
LSTableMessage::GetSequenceNumber ()
{
  return m_sequenceNumber;
}

std::vector<Ipv4Address>
LSTableMessage::GetNeighbors ()
{
  return m_neighbors;
}
