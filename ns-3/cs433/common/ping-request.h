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

#ifndef PING_REQUEST_H
#define PING_REQUEST_H

#include <string>
#include "ns3/type-name.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"

using namespace ns3;

class PingRequest : public SimpleRefCount<PingRequest>
{
  public: 
    PingRequest (uint32_t sequenceNumber, Time timestamp, Ipv4Address destinationAddress, std::string pingMessage);
    ~PingRequest ();

    Time GetTimestamp ();
    uint32_t GetSequenceNumber ();
    std::string GetPingMessage ();
    Ipv4Address GetDestinationAddress ();

  private:
    Time m_timestamp;
    uint32_t m_sequenceNumber;  
    std::string m_pingMessage;
    Ipv4Address m_destinationAddress;
};

#endif
