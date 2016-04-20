/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/ping-request.h"

using namespace ns3;

PingRequest::PingRequest (uint32_t sequenceNumber, Time timestamp, Ipv4Address destinationAddress, std::string pingMessage)
{
  m_sequenceNumber = sequenceNumber;
  m_timestamp = timestamp;
  m_pingMessage = pingMessage;
  m_destinationAddress = destinationAddress;
}

PingRequest::~PingRequest ()
{
}

Time
PingRequest::GetTimestamp ()
{
  return m_timestamp;
}

uint32_t 
PingRequest::GetSequenceNumber ()
{
  return m_sequenceNumber;
}

std::string
PingRequest::GetPingMessage ()
{
  return m_pingMessage;
}

Ipv4Address
PingRequest::GetDestinationAddress ()
{
  return m_destinationAddress;
}
