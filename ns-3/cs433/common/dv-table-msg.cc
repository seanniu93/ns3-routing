#include "ns3/dv-table-msg.h"

using namespace ns3;

DVTableMessage::DVTableMessage (uint32_t sequenceNumber, Time timestamp, std::vector<Ipv4Address> neighbors)
{
  m_sequenceNumber = sequenceNumber;
  m_timestamp = timestamp;
  m_neighbors = neighbors;
}

DVTableMessage::~DVTableMessage ()
{
}

Time
DVTableMessage::GetTimestamp ()
{
  return m_timestamp;
}

uint32_t
DVTableMessage::GetSequenceNumber ()
{
  return m_sequenceNumber;
}

std::vector<Ipv4Address>
DVTableMessage::GetNeighbors ()
{
  return m_neighbors;
}
