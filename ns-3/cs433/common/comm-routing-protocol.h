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

#ifndef COMM_ROUTING_PROTOCOL_H
#define COMM_ROUTING_PROTOCOL_H

#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-address.h"
#include "ns3/comm-log.h"

#include <vector>
#include <map>

using namespace ns3;


class CommRoutingProtocol : public Ipv4RoutingProtocol, public CommLog
{
  public:
    static TypeId GetTypeId (void);
    CommRoutingProtocol ();
    virtual ~CommRoutingProtocol ();

    // From Ipv4RoutingProtocol
    virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr) = 0;
     virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                               UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                               LocalDeliverCallback lcb, ErrorCallback ecb) = 0;
    virtual void NotifyInterfaceUp (uint32_t interface) = 0;
    virtual void NotifyInterfaceDown (uint32_t interface) = 0;
    virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address) = 0;
    virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address) = 0;
    virtual void SetIpv4 (Ptr<Ipv4> ipv4) = 0;

    virtual void DoDispose ();

    // Interface for protocols
    virtual void ProcessCommand (std::vector<std::string> tokens) = 0;
    virtual void SetMainInterface (uint32_t mainInterface) = 0;
    virtual void SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap) = 0;
    virtual void SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap) = 0;

  private:
    virtual Ipv4Address ResolveNodeIpAddress (uint32_t nodeNumber) = 0;
    virtual std::string ReverseLookup (Ipv4Address ipv4Address) = 0; 
};

#endif

