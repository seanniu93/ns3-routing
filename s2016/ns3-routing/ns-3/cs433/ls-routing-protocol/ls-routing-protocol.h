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

#ifndef LS_ROUTING_H
#define LS_ROUTING_H

#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/timer.h"

#include "ns3/ping-request.h"
#include "ns3/comm-routing-protocol.h"
#include "ns3/ls-message.h"

#include <vector>
#include <map>

using namespace ns3;

class LSRoutingProtocol : public CommRoutingProtocol
{
  public:
    static TypeId GetTypeId (void);

    LSRoutingProtocol ();
    virtual ~LSRoutingProtocol ();
    /**
     * \brief Process command issued from the scenario file or interactively issued from keyboard.
     *
     * This method is called by the simulator-main whenever a command is issued to this module.
     *
     * \param tokens String tokens for processing.
     */

    virtual void ProcessCommand (std::vector<std::string> tokens);
    /**
     * \brief Set the main interface of a node.
     *
     * This method is called by the simulator-main when this node is created.
     *
     * \param mainInterface Interface Index.
     */
    virtual void SetMainInterface (uint32_t mainInterface);
    /**
     * \brief Save the mapping from Inet topology node numbers to main addresses.
     *
     * This method is called by the simulator-main when this node is created.
     *
     * \param nodeAddressMap Mapping.
     */

    virtual void SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap); 
    /**
     * \brief Save the mapping from IP addresses to Inet topology node numbers.
     *
     * This method is called by the simulator-main when this node is created.
     *
     * \param addressNodeMap Mapping.
     */

    virtual void SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap);

    // Message Handling
    /**
     * \brief Data Receive Callback function for UDP control plane sockets.
     *
     * \param socket Socket on which data is received.
     */

    void RecvLSMessage (Ptr<Socket> socket);
    void ProcessPingReq (LSMessage lsMessage);
    void ProcessPingRsp (LSMessage lsMessage);

    // Periodic Audit
    void AuditPings ();
  
    // From Ipv4RoutingProtocol

    /**
     * \brief Query routing cache for an existing route, for an outbound packet
     *
     * This lookup is used by transport protocols.  It does not cause any
     * packet to be forwarded, and is synchronous.  Can be used for
     * multicast or unicast.  The Linux equivalent is ip_route_output()
     *
     * \param p packet to be routed.  Note that this method may modify the packet.
     *          Callers may also pass in a null pointer. 
     * \param header input parameter (used to form key to search for the route)
     * \param oif Output interface Netdevice.  May be zero, or may be bound via
     *            socket options to a particular output interface.
     * \param sockerr Output parameter; socket errno 
     *
     * \returns a code that indicates what happened in the lookup
     */
    virtual Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
    /**
     * \brief Route an input packet (to be forwarded or locally delivered)
     *
     * This lookup is used in the forwarding process.  The packet is
     * handed over to the Ipv4RoutingProtocol, and will get forwarded onward
     * by one of the callbacks.  The Linux equivalent is ip_route_input().
     * There are four valid outcomes, and a matching callbacks to handle each.
     *
     * \param p received packet
     * \param header input parameter used to form a search key for a route
     * \param idev Pointer to ingress network device
     * \param ucb Callback for the case in which the packet is to be forwarded
     *            as unicast
     * \param mcb Callback for the case in which the packet is to be forwarded
     *            as multicast
     * \param lcb Callback for the case in which the packet is to be locally
     *            delivered
     * \param ecb Callback to call if there is an error in forwarding
     * \returns true if the Ipv4RoutingProtocol takes responsibility for 
     *          forwarding or delivering the packet, false otherwise
     */ 
     virtual bool RouteInput  (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                               UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                 LocalDeliverCallback lcb, ErrorCallback ecb); 
    /**
     * \param interface the index of the interface we are being notified about
     *
     * Protocols are expected to implement this method to be notified of the state change of
     * an interface in a node.
     * cs433: Skip this implementation.
     */
    virtual void NotifyInterfaceUp (uint32_t interface);
    /**
     * \param interface the index of the interface we are being notified about
     *
     * Protocols are expected to implement this method to be notified of the state change of
     * an interface in a node.
     * cs433: Skip this implementation.
     */
    virtual void NotifyInterfaceDown (uint32_t interface);
    /**
     * \param interface the index of the interface we are being notified about
     * \param address a new address being added to an interface
     *
     * Protocols are expected to implement this method to be notified whenever
     * a new address is added to an interface. Typically used to add a 'network route' on an
     * interface. Can be invoked on an up or down interface.
     * cs433: Skip this implementation.
     */
    virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
    /**
     * \param interface the index of the interface we are being notified about
     * \param address a new address being added to an interface
     *
     * Protocols are expected to implement this method to be notified whenever
     * a new address is removed from an interface. Typically used to remove the 'network route' of an
     * interface. Can be invoked on an up or down interface.
     * cs433: Skip this implementation.
     */
    virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
    /**
     * \param ipv4 the ipv4 object this routing protocol is being associated with
     * 
     * Typically, invoked directly or indirectly from ns3::Ipv4::SetRoutingProtocol
     */
    virtual void SetIpv4 (Ptr<Ipv4> ipv4);

    void DoDispose ();

  private:
    /**
     * \brief Broadcast a packet on all interfaces.
     *
     * \param packet Packet to be sent.
     */
    void BroadcastPacket (Ptr<Packet> packet);
    /**
     * \brief Returns the main IP address of a node in Inet topology.
     *
     * Useful when using commands like PING etc.
     *
     * \param nodeNumber Node Number as in Inet topology.
     */
    virtual Ipv4Address ResolveNodeIpAddress (uint32_t nodeNumber);    
    /**
     * \brief Returns the node number which is using the specified IP.
     *
     * Useful when printing out debugging messages etc.
     *
     * \param ipv4Address IP address of node.
     */

    virtual std::string ReverseLookup (Ipv4Address ipv4Address); 
    
    // Status 
    void DumpLSA ();
    void DumpNeighbors ();
    void DumpRoutingTable ();

  protected:
    virtual void DoStart (void);
    uint32_t GetNextSequenceNumber ();
    /**
     * \brief Check whether the specified IP is owned by this node.
     * 
     * \param ipv4Address IP address.
     */
    bool IsOwnAddress (Ipv4Address originatorAddress);


  private:
    std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
    Ipv4Address m_mainAddress;
    Ptr<Ipv4StaticRouting> m_staticRouting;
    Ptr<Ipv4> m_ipv4;
    Time m_pingTimeout;
    uint8_t m_maxTTL;
    uint16_t m_lsPort;
    uint32_t m_currentSequenceNumber;
    std::map<uint32_t, Ipv4Address> m_nodeAddressMap;
    std::map<Ipv4Address, uint32_t> m_addressNodeMap;
    // Timers
    Timer m_auditPingsTimer;
    // Ping tracker
    std::map<uint32_t, Ptr<PingRequest> > m_pingTracker;
};

#endif
