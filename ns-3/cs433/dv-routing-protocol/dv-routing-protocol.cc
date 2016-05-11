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


#include "ns3/dv-routing-protocol.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-route.h"
#include "ns3/uinteger.h"
#include "ns3/test-result.h"
#include <sys/time.h>
#include <vector>
#include <limits>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DVRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED (DVRoutingProtocol);

TypeId
DVRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("DVRoutingProtocol")
  .SetParent<CommRoutingProtocol> ()
  .AddConstructor<DVRoutingProtocol> ()
  .AddAttribute ("DVPort",
                 "Listening port for DV packets",
                 UintegerValue (6000),
                 MakeUintegerAccessor (&DVRoutingProtocol::m_dvPort),
                 MakeUintegerChecker<uint16_t> ())
  .AddAttribute ("PingTimeout",
                 "Timeout value for PING_REQ in milliseconds",
                 TimeValue (MilliSeconds (2000)),
                 MakeTimeAccessor (&DVRoutingProtocol::m_pingTimeout),
                 MakeTimeChecker ())
  .AddAttribute ("HelloTimeout",
                 "Timeout value for HELLO in milliseconds",
                 TimeValue (MilliSeconds (6000)),
                 MakeTimeAccessor (&DVRoutingProtocol::m_helloTimeout),
                 MakeTimeChecker ())
  .AddAttribute ("MaxTTL",
                 "Maximum TTL value for DV packets",
                 UintegerValue (16),
                 MakeUintegerAccessor (&DVRoutingProtocol::m_maxTTL),
                 MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}

DVRoutingProtocol::DVRoutingProtocol ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY), m_auditHellosTimer (Timer::CANCEL_ON_DESTROY)
{
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentSequenceNumber = random.GetInteger ();
  // Setup static routing 
  m_staticRouting = Create<Ipv4StaticRouting> ();
}

DVRoutingProtocol::~DVRoutingProtocol ()
{
}

void 
DVRoutingProtocol::DoDispose ()
{
  // Close sockets
  for (std::map< Ptr<Socket>, Ipv4InterfaceAddress >::iterator iter = m_socketAddresses.begin ();
       iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  m_socketAddresses.clear ();
  
  // Clear static routing
  m_staticRouting = 0;

  // Cancel timers
  m_auditPingsTimer.Cancel ();
  m_auditHellosTimer.Cancel ();

  m_pingTracker.clear (); 

  CommRoutingProtocol::DoDispose ();
}

void
DVRoutingProtocol::SetMainInterface (uint32_t mainInterface)
{
  m_mainAddress = m_ipv4->GetAddress (mainInterface, 0).GetLocal ();
}

void
DVRoutingProtocol::SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap)
{
  m_nodeAddressMap = nodeAddressMap;
}

void
DVRoutingProtocol::SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap)
{
  m_addressNodeMap = addressNodeMap;
}

Ipv4Address
DVRoutingProtocol::ResolveNodeIpAddress (uint32_t nodeNumber)
{
  std::map<uint32_t, Ipv4Address>::iterator iter = m_nodeAddressMap.find (nodeNumber);
  if (iter != m_nodeAddressMap.end ())
    { 
      return iter->second;
    }
  return Ipv4Address::GetAny ();
}

std::string
DVRoutingProtocol::ReverseLookup (Ipv4Address ipAddress)
{
  std::map<Ipv4Address, uint32_t>::iterator iter = m_addressNodeMap.find (ipAddress);
  if (iter != m_addressNodeMap.end ())
    { 
      std::ostringstream sin;
      uint32_t nodeNumber = iter->second;
      sin << nodeNumber;    
      return sin.str();
    }
  return "Unknown";
}

void
DVRoutingProtocol::DoStart ()
{
  // Create sockets
  for (uint32_t i = 0 ; i < m_ipv4->GetNInterfaces () ; i++)
    {
      Ipv4Address ipAddress = m_ipv4->GetAddress (i, 0).GetLocal ();
      if (ipAddress == Ipv4Address::GetLoopback ())
        continue;
      // Create socket on this interface
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
          UdpSocketFactory::GetTypeId ());
      socket->SetAllowBroadcast (true);
      InetSocketAddress inetAddr (m_ipv4->GetAddress (i, 0).GetLocal (), m_dvPort);
      socket->SetRecvCallback (MakeCallback (&DVRoutingProtocol::RecvDVMessage, this));
      if (socket->Bind (inetAddr))
        {
          NS_FATAL_ERROR ("DVRoutingProtocol::DoStart::Failed to bind socket!");
        }
      Ptr<NetDevice> netDevice = m_ipv4->GetNetDevice (i);
      socket->BindToNetDevice (netDevice);
      m_socketAddresses[socket] = m_ipv4->GetAddress (i, 0);
    }
  // Configure timers
  m_auditPingsTimer.SetFunction (&DVRoutingProtocol::AuditPings, this);
  m_auditHellosTimer.SetFunction (&DVRoutingProtocol::AuditHellos, this);

  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);
  m_auditHellosTimer.Schedule (m_helloTimeout);

  m_dv.insert(std::make_pair(ReverseLookup(m_mainAddress), 0));
}

Ptr<Ipv4Route>
DVRoutingProtocol::RouteOutput (Ptr<Packet> packet, const Ipv4Header &header, Ptr<NetDevice> outInterface, Socket::SocketErrno &sockerr)
{
    DEBUG_LOG ("RouteOutput Called");
    // No clue what to do with outInterface or sockerr
    Ptr<Ipv4Route> ipv4Route = 0;
    RoutingTableEntry entry;

    if (header.GetDestination () == m_mainAddress) {
         // DEBUG_LOG ("Found route in table");
        Ipv4Address local = Ipv4Address("127.0.0.1");
        ipv4Route = Create<Ipv4Route> ();
        ipv4Route->SetDestination (header.GetDestination ());
        ipv4Route->SetSource (m_mainAddress);
        ipv4Route->SetGateway (local);
        // Not 100% sure below is correct
        // ipv4Route->SetOutputDevice (outInterface); // ????
        int32_t interface = m_ipv4->GetInterfaceForAddress(local);
        ipv4Route->SetOutputDevice(m_ipv4->GetNetDevice (interface));
    }

    else if (SearchTable (entry, header.GetDestination ())) {
        // DEBUG_LOG ("Found route in table");
        ipv4Route = Create<Ipv4Route> ();
        ipv4Route->SetDestination (header.GetDestination ());
        ipv4Route->SetSource (m_mainAddress);
        ipv4Route->SetGateway (entry.nextHopAddr);
        // Not 100% sure below is correct
        // ipv4Route->SetOutputDevice (outInterface); // ????
        int32_t interface = m_ipv4->GetInterfaceForAddress(entry.interfaceAddr);
        ipv4Route->SetOutputDevice(m_ipv4->GetNetDevice (interface));
    }

    else {
        Ptr<Ipv4Route> ipv4Route = m_staticRouting->RouteOutput (packet, header, outInterface, sockerr);
    }

    if (ipv4Route) {
        DEBUG_LOG ("Found route to: " << ipv4Route->GetDestination () << " via next-hop: " << ipv4Route->GetGateway ()
                   << " with source: " << ipv4Route->GetSource () << " and output device " << ipv4Route->GetOutputDevice());
    } else {
        DEBUG_LOG ("No Route to destination: " << header.GetDestination ());
    }
    return ipv4Route;
}

bool 
DVRoutingProtocol::RouteInput  (Ptr<const Packet> packet, 
                                const Ipv4Header &header, Ptr<const NetDevice> inputDev,
                                UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                LocalDeliverCallback lcb, ErrorCallback ecb)
{
    DEBUG_LOG ("RouteInput Called");
    Ipv4Address destinationAddress = header.GetDestination ();
    Ipv4Address sourceAddress = header.GetSource ();

    // Drop if packet was originated by this node
    if (IsOwnAddress (sourceAddress) == true) {
        return true;
    }

    // Check for local delivery
    uint32_t interfaceNum = m_ipv4->GetInterfaceForDevice (inputDev);
    if (m_ipv4->IsDestinationAddress (destinationAddress, interfaceNum)) {
        DEBUG_LOG ( "[RouteInput] Local Delivery")
        if (!lcb.IsNull ()) {
            lcb (packet, header, interfaceNum);
            return true;
        } else {
            return false;
        }
    }

    // Forward using LS routing table
    Ptr<Ipv4Route> ipv4Route;
    RoutingTableEntry entry;
    if (SearchTable (entry, destinationAddress)) {
        DEBUG_LOG ("[RouteInput] Forwarding packet from " << m_mainAddress << " to " << entry.nextHopAddr);

        ipv4Route = Create<Ipv4Route> ();
        ipv4Route->SetDestination (destinationAddress);
        ipv4Route->SetSource (m_mainAddress);
        ipv4Route->SetGateway (entry.nextHopAddr);
        int32_t interface = m_ipv4->GetInterfaceForAddress(entry.interfaceAddr);
        ipv4Route->SetOutputDevice (m_ipv4->GetNetDevice (interface));

        // UnicastForwardCallback = void ucb(Ptr<Ipv4Route>, Ptr<const Packet>, const Ipv4Header &)
        ucb (ipv4Route, packet, header);
        return true;
    }

    // Check static routing table
    else if (m_staticRouting->RouteInput (packet, header, inputDev, ucb, mcb, lcb, ecb)) {
        return true;
    }

    DEBUG_LOG ("Cannot forward packet. No Route to destination: " << header.GetDestination ());
    return false;
}

void
DVRoutingProtocol::BroadcastPacket (Ptr<Packet> packet)
{
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i =
      m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      Ipv4Address broadcastAddr = i->second.GetLocal ().GetSubnetDirectedBroadcast (i->second.GetMask ());
      i->first->SendTo (packet, 0, InetSocketAddress (broadcastAddr, m_dvPort));
    }
}

void
DVRoutingProtocol::ProcessCommand (std::vector<std::string> tokens)
{
  std::vector<std::string>::iterator iterator = tokens.begin();
  std::string command = *iterator;
  if (command == "PING")
    {
      if (tokens.size() < 3)
        {
          ERROR_LOG ("Insufficient PING params..."); 
          return;
        }
      iterator++;
      std::istringstream sin (*iterator);
      uint32_t nodeNumber;
      sin >> nodeNumber;
      iterator++;
      std::string pingMessage = *iterator;
      Ipv4Address destAddress = ResolveNodeIpAddress (nodeNumber);
      if (destAddress != Ipv4Address::GetAny ())
        {
          uint32_t sequenceNumber = GetNextSequenceNumber ();
          TRAFFIC_LOG ("Sending PING_REQ to Node: " << nodeNumber << " IP: " << destAddress << " Message: " << pingMessage << " SequenceNumber: " << sequenceNumber);
          Ptr<PingRequest> pingRequest = Create<PingRequest> (sequenceNumber, Simulator::Now(), destAddress, pingMessage);
          // Add to ping-tracker
          m_pingTracker.insert (std::make_pair (sequenceNumber, pingRequest));
          Ptr<Packet> packet = Create<Packet> ();
          DVMessage dvMessage = DVMessage (DVMessage::PING_REQ, sequenceNumber, m_maxTTL, m_mainAddress);
          dvMessage.SetPingReq (destAddress, pingMessage);
          packet->AddHeader (dvMessage);
          BroadcastPacket (packet);
        }
    }
  else if (command == "DUMP")
    {
      if (tokens.size() < 2)
        {
          ERROR_LOG ("Insufficient Parameters!");
          return;
        }
      iterator++;
      std::string table = *iterator;
      if (table == "ROUTES" || table == "ROUTING")
        {
          DumpRoutingTable ();
        }
      else if (table == "NEIGHBORS" || table == "NEIGHBOURS")
        {
          DumpNeighbors ();
        }
      else if (table == "DV")
        {
          DumpDV();
        }
    }
}

void
DVRoutingProtocol::SendHello () {
  Ipv4Address destAddress = ResolveNodeIpAddress (0); // TODO remove this and "hello"
  uint32_t sequenceNumber = GetNextSequenceNumber ();
  //TRAFFIC_LOG ("Broadcasting HELLO_REQ, SequenceNumber: " << sequenceNumber);
//    //Ptr<HelloRequest> helloRequest = Create<HelloRequest> (sequenceNumber, Simulator::Now(), destAddress, "hello", m_helloTimeout);
   Ptr<Packet> packet = Create<Packet> ();
   DVMessage dvMessage = DVMessage (DVMessage::HELLO_REQ, sequenceNumber, 1, m_mainAddress);
   dvMessage.SetHelloReq (destAddress, "hello");
   packet->AddHeader (dvMessage);
   BroadcastPacket (packet);
}

void
DVRoutingProtocol::SendDVTableMessage () {
  uint32_t sequenceNumber = GetNextSequenceNumber ();

  std::vector<std::pair<Ipv4Address, uint32_t> > neighborCosts;

  TRAFFIC_LOG("Sending DVTableMessage: sequence num: " << sequenceNumber << ", neighborAddrs.size: " << neighborCosts.size());

  Ptr<Packet> packet = Create<Packet> ();
  DVMessage dvMessage = DVMessage (DVMessage::DV_TABLE_MSG, sequenceNumber, 1, m_mainAddress);


  //REVERSE POISON!!!!
  for (ntEntry i = m_neighborTable.begin(); i != m_neighborTable.end(); i++) {

    //create a fresh DV to send out
      std::vector<std::pair<Ipv4Address, uint32_t> > newdv;

    for (distanceVector::iterator j = m_dv.begin(); j != m_dv.end(); j++) {

      std::string dest = j->first;
      Ipv4Address destAddr = ResolveNodeIpAddress(std::atoi(dest.c_str()));
      rtEntry rte = m_routingTable.find(dest);

      if (rte == m_routingTable.end()) {
//        std::cout <<  "couldn't find dest " << dest << " in routing table..\n";
//        std::cout << "Its cost should then be infinity! Current cost is: ";
//        std::cout << m_dv[dest] << std::endl;
        newdv.push_back(std::make_pair(destAddr, M_INF));
//        std::cout << "Set it to inf anyway.\n"; 


      }
      else if (ReverseLookup(rte->second.nextHopAddr) == i->first) {
       // std::cout << "Next hop for " << dest << " is " << i->first << std::endl;
        //newdv.insert(std::make_pair(dest,M_INF));
        newdv.push_back(std::make_pair(destAddr, M_INF));
        //std::cout << "set it to inf" << std::endl;


      }
      else {
       newdv.push_back(std::make_pair(destAddr, m_dv[dest])); 
        //std::cout << "left it untouched: (original = " << m_dv[dest] << std::endl;
      }


    }

    dvMessage.SetDVTableMsg(newdv);
    packet->AddHeader (dvMessage);
    DVRoutingProtocol::SendPacket(packet, m_neighborTable.find(i->first)->second.interfaceAddr);

  }


}

void
DVRoutingProtocol::SendPacket (Ptr<Packet> packet, Ipv4Address interfaceAddr)
{
  
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i =
      m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      Ipv4Address sendAddr = i->second.GetLocal ();
      Ipv4Address broadcastAddr = i->second.GetLocal ().GetSubnetDirectedBroadcast (i->second.GetMask ());
     
      if (interfaceAddr == sendAddr) {
        i->first->SendTo (packet, 0, InetSocketAddress (broadcastAddr, m_dvPort));
        return;
      }
    }
  ERROR_LOG("Attempted to send packet to non-neighbor. " << ReverseLookup(m_mainAddress) << "=>" << ReverseLookup(interfaceAddr));
}

void
DVRoutingProtocol::DumpNeighborDV(NeighborTableEntry nte) {
    distanceVector dv = nte.dv;
    STATUS_LOG (std::endl << "**************** Neighbor DV: " << ReverseLookup(nte.neighborAddr) << " *******************" << std::endl);
    for (distanceVector::iterator i = dv.begin(); i != dv.end(); i++) {
       std::cout << '(' << i->first << ", " << i->second << ") ";
    }
    std::cout << '\n';
}

void
DVRoutingProtocol::DumpDV() {
  STATUS_LOG (std::endl << "**************** Distance Vector ********************" << std::endl);

  for (distanceVector::iterator i = m_dv.begin (); i != m_dv.end (); i++) {
      std::cout << '(' << i->first << ", " << i->second << ") ";
  }
  std::cout << '\n';
  PRINT_LOG ("");

}

void
DVRoutingProtocol::DumpNeighbors ()
{
  STATUS_LOG (std::endl << "**************** Neighbor List ********************" << std::endl
              << "NeighborNumber\t\tNeighborAddr\t\tInterfaceAddr");

  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end (); i++) {
      NeighborTableEntry entry = i->second;
      PRINT_LOG (i->first << "\t\t\t" << entry.neighborAddr << "\t\t" << entry.interfaceAddr << std::endl);
      checkNeighborTableEntry (i->first, entry.neighborAddr, entry.interfaceAddr);
  }

  PRINT_LOG ("");

  /*NOTE: For purpose of autograding, you should invoke the following function for each
  neighbor table entry. The output format is indicated by parameter name and type.
  */
  //  checkNeighborTableEntry();
}

void
DVRoutingProtocol::DumpRoutingTable ()
{
  STATUS_LOG (std::endl << "**************** Route Table ********************" << std::endl
              << "DestNumber\t\tDestAddr\t\tNextHopNumber\t\tNextHopAddr\t\tInterfaceAddr\t\tCost");
	
      for (rtEntry i = m_routingTable.begin (); i != m_routingTable.end (); i++)
	{
	    RoutingTableEntry entry = i->second;
            std::string fromNode = ReverseLookup (entry.destAddr);

	    PRINT_LOG (fromNode << "\t\t" << entry.destAddr << "\t\t" << entry.nextHopNum << "\t\t"
	       << entry.nextHopAddr << "\t\t" << entry.interfaceAddr << "\t\t\t" << entry.cost << std::endl);

	    checkRouteTableEntry (fromNode, entry.destAddr, entry.nextHopNum, entry.nextHopAddr, 
	        entry.interfaceAddr, entry.cost);
	}

  PRINT_LOG ("");

  /*NOTE: For purpose of autograding, you should invoke the following function for each
  routing table entry. The output format is indicated by parameter name and type.
  */
  //checkRouteTableEntry();
}

void
DVRoutingProtocol::RecvDVMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  DVMessage dvMessage;
  packet->RemoveHeader (dvMessage);

  switch (dvMessage.GetMessageType ())
    {
      case DVMessage::PING_REQ:
        ProcessPingReq (dvMessage);
        break;
      case DVMessage::PING_RSP:
        ProcessPingRsp (dvMessage);
        break;
      case DVMessage::HELLO_REQ:
        ProcessHelloReq (dvMessage, socket);
        break;
      case DVMessage::DV_TABLE_MSG:
        ProcessDVTableMessage(dvMessage, socket);
        break;
      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
DVRoutingProtocol::ProcessPingReq (DVMessage dvMessage)
{
  // Check destination address
  if (IsOwnAddress (dvMessage.GetPingReq().destinationAddress))
    {
      // Use reverse lookup for ease of debug
      std::string fromNode = ReverseLookup (dvMessage.GetOriginatorAddress ());
      TRAFFIC_LOG ("Received PING_REQ, From Node: " << fromNode << ", Message: " << dvMessage.GetPingReq().pingMessage);
      // Send Ping Response
      DVMessage dvResp = DVMessage (DVMessage::PING_RSP, dvMessage.GetSequenceNumber(), m_maxTTL, m_mainAddress);
      dvResp.SetPingRsp (dvMessage.GetOriginatorAddress(), dvMessage.GetPingReq().pingMessage);
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (dvResp);
      BroadcastPacket (packet);
    }
}

void
DVRoutingProtocol::ProcessPingRsp (DVMessage dvMessage)
{
  // Check destination address
  if (IsOwnAddress (dvMessage.GetPingRsp().destinationAddress))
    {
      // Remove from pingTracker
      std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
      iter = m_pingTracker.find (dvMessage.GetSequenceNumber ());
      if (iter != m_pingTracker.end ())
        {
          std::string fromNode = ReverseLookup (dvMessage.GetOriginatorAddress ());
          TRAFFIC_LOG ("Received PING_RSP, From Node: " << fromNode << ", Message: " << dvMessage.GetPingRsp().pingMessage);
          m_pingTracker.erase (iter);
        }
      else
        {
          DEBUG_LOG ("Received invalid PING_RSP!");
        }
    }
}


void
DVRoutingProtocol::ProcessHelloReq (DVMessage dvMessage, Ptr<Socket> socket)
{
  // Use reverse lookup for ease of debug
  //std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
  //TRAFFIC_LOG ("Received HELLO_REQ, From Node: " << fromNode);

  Ipv4Address neighAddr = dvMessage.GetOriginatorAddress();
  std::string fromNode = ReverseLookup (dvMessage.GetOriginatorAddress());
  Ipv4Address interfaceAddr;

  std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.find(socket);
  if (i != m_socketAddresses.end())
    {
      interfaceAddr = i->second.GetLocal();
    }
  else
    {
      ERROR_LOG ("Didn't find socket in m_socketAddresses");
    }

   //TRAFFIC_LOG( "Received HelloReq, From Neighbor: " << fromNode << ", with Addr: " << neighAddr << ", InterfaceAddr: " << interfaceAddr << '\n');

  ntEntry e = m_neighborTable.find( fromNode );

  if ( e != m_neighborTable.end() ) {//neighbor was already in our table
    e->second.lastUpdated = Simulator::Now();
  } else {
    //new neighbor discovered!

    //add it to our immediate neighbors table
    distanceVector emptydv;
    NeighborTableEntry entry = { neighAddr, interfaceAddr , Simulator::Now(), emptydv };
    m_neighborTable.insert(std::make_pair(fromNode, entry));

    //add it to our neigbor and distance vectors
    m_dv.insert( std::make_pair(fromNode, 1) );
    m_costs.insert( std::make_pair(fromNode, 1) );
    //SendDVTableMessage();
    //DumpDV();

    //TODO: run DV Algorithm
    BellmanFord(emptydv);
    //DumpDV();
    SendDVTableMessage();
  }
}

void
DVRoutingProtocol::ProcessDVTableMessage (DVMessage dvMessage, Ptr<Socket> socket) {

    // extract info from dvMessage
    std::vector<std::pair<Ipv4Address, uint32_t> > neighborAddrs = dvMessage.GetDVTableMsg().neighborCosts;
    uint32_t seqNum = dvMessage.GetSequenceNumber();
    Ipv4Address fromAddr = dvMessage.GetOriginatorAddress();
    std::string fromNode = ReverseLookup(fromAddr);

    TRAFFIC_LOG("Received DV Table Message from node " << fromNode);

    // insert it into the neighbor table
    distanceVector newdv;
    for (int i = 0; i < neighborAddrs.size(); i++) {
        newdv[ ReverseLookup(neighborAddrs[i].first) ] = neighborAddrs[i].second;
    } 

    // If the NeighborTableEntry does not exist, add it. (This will only happen
    // if the hello message is lost for some reason.)
    ntEntry entry = m_neighborTable.find(fromNode);
    if (entry == m_neighborTable.end()) {
        Ipv4Address interfaceAddr;
        std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.find(socket);
        if (i != m_socketAddresses.end()) {
            interfaceAddr = i->second.GetLocal();
        }
        else {
            ERROR_LOG ("Didn't find socket in m_socketAddresses");
        }

        NeighborTableEntry entry = { fromAddr, interfaceAddr , Simulator::Now(), newdv };
        m_neighborTable.insert(std::make_pair(fromNode, entry));
    } else {
        //std::cout << "We are substituting our neighbor's distance vector!\n";
        //DumpNeighborDV(entry->second);
        entry->second.dv = newdv;
        //DumpNeighborDV(entry->second);
    }
    
    // run DV algorithm
    //DumpDV();
    BellmanFord(newdv);
    //DumpDV();
    // Send new packet with neighbor info
    //SendDVTableMessage();
}

bool
DVRoutingProtocol::IsOwnAddress (Ipv4Address originatorAddress)
{
  // Check all interfaces
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      Ipv4InterfaceAddress interfaceAddr = i->second;
      if (originatorAddress == interfaceAddr.GetLocal ())
        {
          return true;
        }
    }
  return false;

}

void
DVRoutingProtocol::AuditPings ()
{
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  for (iter = m_pingTracker.begin () ; iter != m_pingTracker.end();)
    {
      Ptr<PingRequest> pingRequest = iter->second;
      if (pingRequest->GetTimestamp().GetMilliSeconds() + m_pingTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds())
        {
          DEBUG_LOG ("Ping expired. Message: " << pingRequest->GetPingMessage () << " Timestamp: " << pingRequest->GetTimestamp().GetMilliSeconds () << " CurrentTime: " << Simulator::Now().GetMilliSeconds ());
          // Remove stale entries
          m_pingTracker.erase (iter++);
        }
      else
        {
          ++iter;
        }
    }
  // Rechedule timer
  m_auditPingsTimer.Schedule (m_pingTimeout); 
}


// Add "last updated" field to Neighbor Table using Simulator::Now(). 
void
DVRoutingProtocol::AuditHellos()
{
  //Broadcast a fresh HELLO message to immediate neighbors
  SendHello ();
  bool sendMsg = false;

  // If "last updated" is more than helloTimeout seconds ago, remove it from the NeighborTable
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end ();) {
      NeighborTableEntry entry = i->second;
      TRAFFIC_LOG ("AUDIT HELLOS: entry.lastUpdated: " << entry.lastUpdated.GetMilliSeconds() << ", timeout: " << m_helloTimeout.GetMilliSeconds() << ", time is now: " << Simulator::Now().GetMilliSeconds());

      if ( entry.lastUpdated.GetMilliSeconds() + m_helloTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds()) {
          std::string nodeName = ReverseLookup(entry.neighborAddr);
          m_dv[nodeName] = M_INF;
          //m_costs[nodeName] = M_INF;
          //m_dv.erase( nodeNumber );
          distanceVector::iterator it = m_costs.find(nodeName);
          m_costs.erase( it );
          m_neighborTable.erase( i++ );
          sendMsg = true;
          //std::cout << "Didn't hear back from " << nodeName;
          
      } else {
         ++i;
      }
  }
  //std::cout << "My new tables:\n";
  //DumpNeighbors();
  //DumpDV();

  if (sendMsg) {
      //std::cout << "AuditHellos is about to run BellmanFord!\n";
      // run DV Algorithm
      BellmanFord(m_dv);

      SendDVTableMessage();
  }

  // Reschedule timer
  m_auditHellosTimer.Schedule (m_helloTimeout);
}

void
DVRoutingProtocol::BellmanFord(distanceVector &ndv) {
    TRAFFIC_LOG("RUNNING BELLMAN FORD");
    m_routingTable.clear();
    for (distanceVector::iterator j = ndv.begin(); j != ndv.end(); j++) {
        std::string name = j->first;
        //std::cout << "Checking node " << name << "...";
        if (m_dv.find(name) == m_dv.end()) {
            m_dv.insert( std::make_pair(name, std::numeric_limits<int>::max()) );
            //std::cout << "Inserted New Node " << name << '\n';
        } else {
          //std::cout << '\n';
        }
    }
 
    bool updated_value;
    bool sendDV = false;

    // Second, iterate through all known nodes and find the minimum distance
    for (distanceVector::iterator i = m_dv.begin(); i != m_dv.end(); i++) {
        uint32_t minCost = std::numeric_limits<int>::max();
        updated_value = false;
        std::string dest = i->first;

        std::string nextHop;

        // create a new routing table entry
       //check if it's ourselves
        if (dest == ReverseLookup(m_mainAddress)) {
            RoutingTableEntry nodeEntry;
            nodeEntry.destAddr = m_mainAddress;
            nodeEntry.nextHopNum = m_mainAddress.Get();
            nodeEntry.nextHopAddr = m_mainAddress;
            nodeEntry.interfaceAddr = m_mainAddress;
            nodeEntry.cost = 0;
            m_routingTable.insert(std::make_pair(ReverseLookup(m_mainAddress), nodeEntry));
            continue;
        }
        //std::cout << "Computing min dist to " << dest << std::endl;

        //uint32_t current = std::numeric_limits<int>::max();
        for (distanceVector::iterator j = m_costs.begin(); j != m_costs.end(); j++) {
            //std::cout << "  Going through " << j->first << "'s DV...\n";
            // (Don't try to calculate a neighbor's distance to a neighbor, since all neighbors
            // have cost = 1)
            //if (dest != j->first) {
            uint32_t cost_to_neighbor = j->second;

	    NeighborTableEntry entry = m_neighborTable.find(j->first)->second;

            if ( entry.dv.empty() ) continue;
            
	    distanceVector::iterator neighDest = entry.dv.find(dest);
            //std::cout << "    trying to find "  << dest << " in " << j->first << " DV...\n";

	    // If the neighbor can reach the destination, calculate the cost
            if (neighDest == entry.dv.end() )
               continue;

            uint32_t current;
            uint32_t neighbor_to_dest = neighDest->second;
	    if (neighbor_to_dest <= std::numeric_limits<int>::max() - cost_to_neighbor) {
		current = cost_to_neighbor + neighbor_to_dest;
            } else { current = std::numeric_limits<int>::max();
            }

	    updated_value = true;
	    //std::cout << "  Found! Dist from " << j->first << " --> cost = " << current;
            
	    if (current < minCost) {
               //std::cout << "(new min)";
	       minCost = current;
               nextHop = j->first;
	    }
            //std::cout << std::endl;
        }

        if (updated_value) {
            if (minCost != i->second) {
                i->second = minCost;
                sendDV = true; 
                //std::cout << "Found a new minCost. New DV:\n";
               // DumpDV();
            }
        }

        if (minCost < std::numeric_limits<int>::max()) {
           // std::cout << "Adding Entry for " << dest << " in routing table\n";
            Ipv4Address nha = ResolveNodeIpAddress(std::atoi(nextHop.c_str()));

            RoutingTableEntry nodeEntry = {
              ResolveNodeIpAddress(std::atoi(dest.c_str())),
              nha.Get(),
              nha,
              m_neighborTable.find(ReverseLookup(nha))->second.interfaceAddr,
              minCost};

            m_routingTable.insert(std::make_pair(dest, nodeEntry));    
            //DumpRoutingTable();
        }

    }
    if (sendDV) {
        //std::cout << "Sending DV table:\n";
        //DumpDV();
        SendDVTableMessage();
    }
    //DumpRoutingTable();
    //SendDVTableMessage();
}

bool
DVRoutingProtocol::SearchTable (RoutingTableEntry& out_entry, Ipv4Address dest)
{
    // Get the iterator at "dest" position
    std::string node = ReverseLookup(dest);
    std::map<std::string, RoutingTableEntry>::iterator it = m_routingTable.find(node);
    if (it == m_routingTable.end()) {
         return false;
    }
    out_entry = it->second;
    return true;
}

uint32_t
DVRoutingProtocol::GetNextSequenceNumber ()
{
  return m_currentSequenceNumber++;
}

void 
DVRoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  m_staticRouting->NotifyInterfaceUp (i);
}
void 
DVRoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  m_staticRouting->NotifyInterfaceDown (i);
}
void 
DVRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyAddAddress (interface, address);
}
void 
DVRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyRemoveAddress (interface, address);
}

void
DVRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  m_ipv4 = ipv4;
  m_staticRouting->SetIpv4 (m_ipv4);
}
