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


#include "ns3/ls-routing-protocol.h"
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

NS_LOG_COMPONENT_DEFINE ("LSRoutingProtocol");
NS_OBJECT_ENSURE_REGISTERED (LSRoutingProtocol);

TypeId
LSRoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("LSRoutingProtocol")
  .SetParent<CommRoutingProtocol> ()
  .AddConstructor<LSRoutingProtocol> ()
  .AddAttribute ("LSPort",
                 "Listening port for LS packets",
                 UintegerValue (5000),
                 MakeUintegerAccessor (&LSRoutingProtocol::m_lsPort),
                 MakeUintegerChecker<uint16_t> ())
  .AddAttribute ("PingTimeout",
                 "Timeout value for PING_REQ in milliseconds",
                 TimeValue (MilliSeconds (2000)),
                 MakeTimeAccessor (&LSRoutingProtocol::m_pingTimeout),
                 MakeTimeChecker ())
  .AddAttribute ("HelloTimeout",
                 "Timeout value for HELLO in milliseconds",
                 TimeValue (MilliSeconds (6000)),
                 MakeTimeAccessor (&LSRoutingProtocol::m_helloTimeout),
                 MakeTimeChecker ())
  .AddAttribute ("MaxTTL",
                 "Maximum TTL value for LS packets",
                 UintegerValue (16),
                 MakeUintegerAccessor (&LSRoutingProtocol::m_maxTTL),
                 MakeUintegerChecker<uint8_t> ())
  ;
  return tid;
}

LSRoutingProtocol::LSRoutingProtocol ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY), m_auditHellosTimer (Timer::CANCEL_ON_DESTROY)
{
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentSequenceNumber = random.GetInteger ();
  // Setup static routing 
  m_staticRouting = Create<Ipv4StaticRouting> ();
}

LSRoutingProtocol::~LSRoutingProtocol ()
{
}

void 
LSRoutingProtocol::DoDispose ()
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
LSRoutingProtocol::SetMainInterface (uint32_t mainInterface)
{
  m_mainAddress = m_ipv4->GetAddress (mainInterface, 0).GetLocal ();
}

void
LSRoutingProtocol::SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap)
{
  m_nodeAddressMap = nodeAddressMap;
}

void
LSRoutingProtocol::SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap)
{
  m_addressNodeMap = addressNodeMap;
}

Ipv4Address
LSRoutingProtocol::ResolveNodeIpAddress (uint32_t nodeNumber)
{
  std::map<uint32_t, Ipv4Address>::iterator iter = m_nodeAddressMap.find (nodeNumber);
  if (iter != m_nodeAddressMap.end ())
    { 
      return iter->second;
    }
  return Ipv4Address::GetAny ();
}

std::string
LSRoutingProtocol::ReverseLookup (Ipv4Address ipAddress)
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
LSRoutingProtocol::DoStart ()
{

//  std::cout << m_ipv4->GetNInterfaces() << '\n';

  // Create sockets
  for (uint32_t i = 0 ; i < m_ipv4->GetNInterfaces () ; i++)
    {
      Ipv4Address ipAddress = m_ipv4->GetAddress (i, 0).GetLocal ();

      //std::cout << i << ", " << ipAddress << '\n';

      if (ipAddress == Ipv4Address::GetLoopback ())
        continue;
      // Create socket on this interface
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
          UdpSocketFactory::GetTypeId ());
      socket->SetAllowBroadcast (true);
      InetSocketAddress inetAddr (m_ipv4->GetAddress (i, 0).GetLocal (), m_lsPort);
      socket->SetRecvCallback (MakeCallback (&LSRoutingProtocol::RecvLSMessage, this));
      if (socket->Bind (inetAddr))
        {
          NS_FATAL_ERROR ("LSRoutingProtocol::DoStart::Failed to bind socket!");
        }
      Ptr<NetDevice> netDevice = m_ipv4->GetNetDevice (i);
      socket->BindToNetDevice (netDevice);
      m_socketAddresses[socket] = m_ipv4->GetAddress (i, 0);

//      std::cout << i << ", " << ipAddress << ", " << socket << ", " << inetAddr << ", " << m_ipv4->GetAddress(i, 0) << '\n';


    }
  // Configure timers
  m_auditPingsTimer.SetFunction (&LSRoutingProtocol::AuditPings, this);
  m_auditHellosTimer.SetFunction (&LSRoutingProtocol::AuditHellos, this);

  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);
  m_auditHellosTimer.Schedule (m_helloTimeout);

}

Ptr<Ipv4Route>
LSRoutingProtocol::RouteOutput (Ptr<Packet> packet, const Ipv4Header &header, Ptr<NetDevice> outInterface, Socket::SocketErrno &sockerr)
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
LSRoutingProtocol::RouteInput  (Ptr<const Packet> packet,
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
LSRoutingProtocol::BroadcastPacket (Ptr<Packet> packet)
{
  for (std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i =
      m_socketAddresses.begin (); i != m_socketAddresses.end (); i++)
    {
      //TRAFFIC_LOG( "Interface Addr: " << i->second.GetLocal());
      Ipv4Address broadcastAddr = i->second.GetLocal ().GetSubnetDirectedBroadcast (i->second.GetMask ());
      i->first->SendTo (packet, 0, InetSocketAddress (broadcastAddr, m_lsPort));
    }
}

void
LSRoutingProtocol::ProcessCommand (std::vector<std::string> tokens)
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
          LSMessage lsMessage = LSMessage (LSMessage::PING_REQ, sequenceNumber, m_maxTTL, m_mainAddress);
          lsMessage.SetPingReq (destAddress, pingMessage);
          packet->AddHeader (lsMessage);
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
      else if (table == "LSA")
        {
          DumpLSTable ();
        }
    }
  else if (command == "HELLO")
    {
    	SendHello ();
    }

  else if (command == "LSTABLEMSG")
    {
      SendLSTableMessage ();
    }
}

void
LSRoutingProtocol::SendHello () {
  Ipv4Address destAddress = ResolveNodeIpAddress (0); // TODO remove this and "hello"
  uint32_t sequenceNumber = GetNextSequenceNumber ();
//  TRAFFIC_LOG ("Broadcasting HELLO_REQ, SequenceNumber: " << sequenceNumber);
  //Ptr<HelloRequest> helloRequest = Create<HelloRequest> (sequenceNumber, Simulator::Now(), destAddress, "hello", m_helloTimeout);
  // Add to hello-tracker
//  m_helloTracker.insert (std::make_pair (sequenceNumber, helloRequest));
  Ptr<Packet> packet = Create<Packet> ();
  //TTL for HELLO msgs is 1
  LSMessage lsMessage = LSMessage (LSMessage::HELLO_REQ, sequenceNumber, 1, m_mainAddress);

  //TRAFFIC_LOG("MY TTL IS: " << (unsigned)lsMessage.GetTTL() << " and my seqNum is " << lsMessage.GetSequenceNumber());

  lsMessage.SetHelloReq (destAddress, "hello");
  packet->AddHeader (lsMessage);
  BroadcastPacket (packet);
}

void
LSRoutingProtocol::SendLSTableMessage () {
  uint32_t sequenceNumber = GetNextSequenceNumber ();

  nbrCostsVec neighborAddrs;
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end (); i++) {
    neighborAddrs.push_back( std::make_pair(i->second.neighborAddr, (uint32_t) 1 )); //TODO change cost here
  }

//  TRAFFIC_LOG("Sending LSTableMessage: sequence num: " << sequenceNumber << ", neighborAddrs.size: " << neighborAddrs.size());

  //Ptr<LSTableMessage> lsTableMsg = Create<LSTableMessage> (sequenceNumber, Simulator::Now(), neighborAddrs);
  Ptr<Packet> packet = Create<Packet> ();
  LSMessage lsMessage = LSMessage (LSMessage::LS_TABLE_MSG, sequenceNumber, m_maxTTL, m_mainAddress);
  TRAFFIC_LOG("Sending LSTableMessage: " << lsMessage);
  lsMessage.SetLSTableMsg(neighborAddrs);
  packet->AddHeader (lsMessage);
  BroadcastPacket (packet);

}



void
LSRoutingProtocol::DumpLSA ()
{
  STATUS_LOG (std::endl << "**************** LSA DUMP ********************" << std::endl
              << "Node\t\tNeighbor(s)");
  PRINT_LOG ("");
}

void
LSRoutingProtocol::DumpNeighbors ()
{
  STATUS_LOG (std::endl << "**************** Neighbor List ********************" << std::endl
              << "NeighborNumber\t\tNeighborAddr\t\tInterfaceAddr");
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end (); i++)
    {
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
LSRoutingProtocol::DumpRoutingTable ()
{
	STATUS_LOG (std::endl << "**************** Route Table ********************" << std::endl
			  << "DestNumber\tDestAddr\t\tNextHopNumber\t\tNextHopAddr\t\tInterfaceAddr\t\tCost");

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
LSRoutingProtocol::DumpLSTable ()
{
	STATUS_LOG (std::endl << "**************** LS Table ********************" << std::endl)

	for (lstEntry i = m_lsTable.begin (); i != m_lsTable.end (); i++)
	{
	    LSTableEntry entry = i->second;
            std::cout << "\nNode " << i->first << '\n';
            nbrCostsVec nc = entry.neighborCosts;
            for (int i = 0; i < nc.size(); i++) {
                std::cout << ReverseLookup(nc[i].first) << " " << nc[i].second << '\n';
            }
	}
	PRINT_LOG ("");
}



void
LSRoutingProtocol::RecvLSMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  LSMessage lsMessage;
  packet->RemoveHeader (lsMessage);

//  TRAFFIC_LOG( "SourceAddr: " << sourceAddress << '\n');

  switch (lsMessage.GetMessageType ())
    {
      case LSMessage::PING_REQ:
        ProcessPingReq (lsMessage);
        break;
      case LSMessage::PING_RSP:
        ProcessPingRsp (lsMessage);
        break;
      case LSMessage::HELLO_REQ:
        ProcessHelloReq (lsMessage, socket);
        break;
      //case LSMessage::HELLO_RSP:
      //  ProcessHelloRsp (lsMessage, socket);
      //  break;
      case LSMessage::LS_TABLE_MSG:
        ProcessLSTableMessage(lsMessage);
        //TRAFFIC_LOG( "SourceAddr: " << sourceAddress << '\n');
        break;
      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
LSRoutingProtocol::ProcessPingReq (LSMessage lsMessage)
{
  // Check destination address
  if (IsOwnAddress (lsMessage.GetPingReq().destinationAddress))
    {
      // Use reverse lookup for ease of debug
      std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
      TRAFFIC_LOG ("Received PING_REQ, From Node: " << fromNode << ", OrigAddr: " << lsMessage.GetOriginatorAddress() << ", Message: " << lsMessage.GetPingReq().pingMessage);
      // Send Ping Response
      LSMessage lsResp = LSMessage (LSMessage::PING_RSP, lsMessage.GetSequenceNumber(), m_maxTTL, m_mainAddress);
      lsResp.SetPingRsp (lsMessage.GetOriginatorAddress(), lsMessage.GetPingReq().pingMessage);
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (lsResp);
      BroadcastPacket (packet);
    }
}

void
LSRoutingProtocol::ProcessPingRsp (LSMessage lsMessage)
{
  // Check destination address
  if (IsOwnAddress (lsMessage.GetPingRsp().destinationAddress))
    {
      // Remove from pingTracker
      std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
      iter = m_pingTracker.find (lsMessage.GetSequenceNumber ());
      if (iter != m_pingTracker.end ())
        {
          std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
          TRAFFIC_LOG ("Received PING_RSP, From Node: " << fromNode << ", OrigAddr: " << lsMessage.GetOriginatorAddress() << ", Message: " << lsMessage.GetPingRsp().pingMessage);
          m_pingTracker.erase (iter);
        }
      else
        {
          DEBUG_LOG ("Received invalid PING_RSP!");
        }
    }
}

void
LSRoutingProtocol::ProcessHelloReq (LSMessage lsMessage, Ptr<Socket> socket)
{
  // Use reverse lookup for ease of debug
  Ipv4Address neighAddr = lsMessage.GetOriginatorAddress();
  std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress());
  Ipv4Address interfaceAddr;

  // TRAFFIC_LOG ("Received HELLO_REQ, From Node: " << fromNode << " with TTL " << (unsigned)lsMessage.GetTTL());

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
    NeighborTableEntry entry = { neighAddr, interfaceAddr , Simulator::Now() };
    m_neighborTable.insert(std::make_pair(fromNode, entry));

    //also add it to the lsTable (with neighbor information for all nodes in network)
    //see if this node is already in that table
    lstEntry lste = m_lsTable.find( ReverseLookup(m_mainAddress) );
    if ( lste != m_lsTable.end() ) {//if this node is already in there
      //then just add this neighbor to the neighbors vector
      lste->second.neighborCosts.push_back(std::make_pair(neighAddr, (uint32_t) 1));//TODO: change cost here

    } else {
      nbrCostsVec nbrCosts;
      nbrCosts.push_back(std::make_pair(neighAddr, 1));  //TODO: change cost here
      LSTableEntry lstEntry = { nbrCosts, lsMessage.GetSequenceNumber() };
      m_lsTable.insert(std::make_pair(ReverseLookup(m_mainAddress), lstEntry));

    }


    //run Dijkstra to update costs
    Dijkstra();

    //DumpRoutingTable();

    SendLSTableMessage(); // neighbor table has changed, so resend neighbor info
  }

  // Send Hello Response
//  LSMessage lsResp = LSMessage (LSMessage::HELLO_RSP, lsMessage.GetSequenceNumber(), 1, m_mainAddress);
//  lsResp.SetHelloRsp (lsMessage.GetOriginatorAddress(), lsMessage.GetHelloReq().helloMessage);
//  Ptr<Packet> packet = Create<Packet> ();
//  packet->AddHeader (lsResp);
//  DEBUG_LOG ("Sending HELLO_RSP, To Node: " << fromNode);
//  SendPacket (packet, socket);
  //BroadcastPacket (packet);

}

/*
void
LSRoutingProtocol::ProcessHelloRsp (LSMessage lsMessage, Ptr<Socket> socket)
{
  Ipv4Address neighAddr = lsMessage.GetOriginatorAddress();
  std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress());
  Ipv4Address interfaceAddr;

  std::map<Ptr<Socket> , Ipv4InterfaceAddress>::const_iterator i = m_socketAddresses.find(socket);
  if (i != m_socketAddresses.end())
    {
      interfaceAddr = i->second.GetLocal();
      //Ipv4Address broadcastAddr = i->second.GetLocal ().GetSubnetDirectedBroadcast (i->second.GetMask ());
      //i->first->SendTo (packet, 0, InetSocketAddress (broadcastAddr, m_lsPort));
    }
  else
    {
      ERROR_LOG ("Didn't find socket in m_socketAddresses");
    }
 
   TRAFFIC_LOG( "Received HelloRsp, From Neighbor: " << fromNode << ", with Addr: " << neighAddr << ", InterfaceAddr: " << interfaceAddr << '\n');

  // Remove from HelloTracker
  // std::map<uint32_t, Ptr<HelloRequest> >::iterator iter;
  // iter = m_helloTracker.find (lsMessage.GetSequenceNumber ());
  // if (iter != m_helloTracker.end ())
  //   {
//      std::string fromNode = ReverseLookup (lsMessage.GetOriginatorAddress ());
//      TRAFFIC_LOG ("Received Hello_RSP, From Node: " << fromNode << ", Message: " << lsMessage.GetHelloRsp().helloMessage);
  //     m_helloTracker.erase (iter);
  //   }
  // else
  //   {
  //     DEBUG_LOG ("Received invalid Hello_RSP!");
  //   }
}
*/

void
LSRoutingProtocol::ProcessLSTableMessage (LSMessage lsMessage) {
    nbrCostsVec neighborAddrs = lsMessage.GetLSTableMsg().neighborCosts;
    uint32_t seqNum = lsMessage.GetSequenceNumber();

    // TRAFFIC_LOG("Received LSTableMessage. " << lsMessage);

    // if we have not seen the packet before, broadcast it
    Ipv4Address fromAddr = lsMessage.GetOriginatorAddress();
    std::string fromNode = ReverseLookup(fromAddr);

    lstEntry entry = m_lsTable.find(fromNode);

//    TRAFFIC_LOG("Sequence Number of this entry: " << entry->second.sequenceNumber << " SeqNum of new packet: " << seqNum);
    if (entry == m_lsTable.end() || seqNum > entry->second.sequenceNumber) {
 //     std::cout << "Sequence Number is new.\n";

      //if it was already in the table, delete it first
      if (entry != m_lsTable.end())
        m_lsTable.erase(entry);

      // Add to LSTable
      nbrCostsVec neighborCosts;
      for (int i = 0; i < neighborAddrs.size(); i++) {
        neighborCosts.push_back(std::make_pair(neighborAddrs[i].first, neighborAddrs[i].second));
      }
      LSTableEntry newEntry = { neighborCosts, seqNum };
      m_lsTable.insert(std::pair<std::string, LSTableEntry>(fromNode, newEntry));

      //DumpLSTable();
      // Run Dijkstra
      Dijkstra();
      //DumpLSTable();

      // Send Packet
      Ptr<Packet> p = Create<Packet> ();
      lsMessage.SetTTL( lsMessage.GetTTL() - 1 ); // we need to decrement TTL ourselves
      p->AddHeader (lsMessage);
      BroadcastPacket (p);
    }
}

bool
LSRoutingProtocol::IsOwnAddress (Ipv4Address originatorAddress)
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
LSRoutingProtocol::AuditPings ()
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
LSRoutingProtocol::AuditHellos()
{
  //Broadcast a fresh HELLO message to immediate neighbors
  SendHello ();
  bool sendMsg = false;

  // If "last updated" is more than helloTimeout seconds ago, remove it from the NeighborTable
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end ();) {
      NeighborTableEntry entry = i->second;
  //    TRAFFIC_LOG ("AUDIT HELLOS: entry.lastUpdated: " << entry.lastUpdated.GetMilliSeconds() << ", timeout: " << m_helloTimeout.GetMilliSeconds() << ", time is now: " << Simulator::Now().GetMilliSeconds());

    if ( entry.lastUpdated.GetMilliSeconds() + m_helloTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds()) {
        // std::cout << "Before removeLSTableLink " << ReverseLookup(m_mainAddress) << " to " << ReverseLookup(entry.neighborAddr);
        // DumpLSTable();

        //remove missing neighbor from my my own entry in LSTable:
        //1) get my entry
        lstEntry entry1 = m_lsTable.find( ReverseLookup(m_mainAddress) );
        //2) remove neighbor form the neighborCosts vector in my entry
        removeLSTableLink( entry.neighborAddr, entry1->second.neighborCosts );

        //now, remove myself from the neighbor's entry in LSTable:
        lstEntry entry2 = m_lsTable.find( ReverseLookup(entry.neighborAddr) );
        removeLSTableLink( m_mainAddress, entry2->second.neighborCosts );

        //remove the neighbor from my Neighbors table
        m_neighborTable.erase(i++);
        sendMsg = true;

        // std::cout << "AFter removeLSTableLink " << ReverseLookup(m_mainAddress) << " to " << ReverseLookup(entry.neighborAddr);
        // DumpLSTable();
    } else {
        ++i;
    }
  }

  if (sendMsg) {
      Dijkstra();
      SendLSTableMessage(); // neighbor table info has changed, so resend neighbor info
  }

  // Reschedule timer
  m_auditHellosTimer.Schedule (m_helloTimeout);

}

void
LSRoutingProtocol::removeLSTableLink(Ipv4Address nodeToRemove, nbrCostsVec& vectorToChange) {
  for (unsigned i = 0; i < vectorToChange.size(); i++) {
      if (vectorToChange[i].first == nodeToRemove) {
          vectorToChange.erase(vectorToChange.begin() + i);
          break;
        }
  }
}

void
LSRoutingProtocol::removeLSTableLink_old(Ipv4Address node1, Ipv4Address node2) {
    lstEntry entry1 = m_lsTable.find( ReverseLookup(node1) );
    lstEntry entry2 = m_lsTable.find( ReverseLookup(node2) );

    if (entry1 != m_lsTable.end()) {
        nbrCostsVec pairs1 = entry1->second.neighborCosts;
        for (unsigned i = 0; i < pairs1.size(); i++) {
            if (pairs1[i].first == node2) {
                pairs1.erase(pairs1.begin() + i);
                break;
            }
        }
    }

    if (entry2 != m_lsTable.end()) {
        nbrCostsVec pairs2 = entry2->second.neighborCosts;
        for (unsigned i = 0; i < pairs2.size(); i++) {
            if (pairs2[i].first == node1) {
                pairs2.erase(pairs2.begin() + i);
                break;
            }
        }
    }

}

void
LSRoutingProtocol::GetNeighbors(const std::string node, std::vector<Ipv4Address>& neighAddrs) {
    neighAddrs.clear();
    lstEntry entry = m_lsTable.find(node);
    // check if does not exist, return NULL
    if (entry == m_lsTable.end()) {
        return;
    }

    nbrCostsVec pairs = entry->second.neighborCosts;
        
    for (unsigned i = 0; i < pairs.size(); i++) {
        neighAddrs.push_back(pairs[i].first);
    }
    
}

uint32_t
LSRoutingProtocol::DistanceToNeighbor(const std::string node1, const std::string node2) {
    if (node1 == node2) {
        return 0;
    }

    lstEntry entry = m_lsTable.find(node1);

    nbrCostsVec pairs = entry->second.neighborCosts;
    for (unsigned i = 0; i < pairs.size(); i++) {
        if (ReverseLookup(pairs[i].first) == node2) {
            return pairs[i].second;
        }
    }

    return std::numeric_limits<int>::max();
}

std::string
LSRoutingProtocol::GetMinCostNode( const std::map<std::string, bool>& leastCostFound ) {
    uint32_t min = std::numeric_limits<int>::max();
    uint32_t cost;
    std::string closest;

    for (rtEntry i = m_routingTable.begin(); i != m_routingTable.end(); i++) {
        std::string node = i->first;
//std::cout << "LOOP node: " << node << '\n';

        if ( leastCostFound.find(node)->second == false ) {
            cost = i->second.cost;
//std::cout << "LOOP cost: " << cost << '\n';
            if (cost < min) {
                min = cost;
                closest = node;
            }
        }
    }
    //leastCostFound.find(closest)->second = true;

    //std::cout << "CLOSEST: " << closest << '\n';
    return closest;
}

void
LSRoutingProtocol::Dijkstra() {
  //we modify the routing table directly
  //wipe it clean first
  m_routingTable.clear();

  //create a map from each node to a bool: if true, means already found least cost to that node
  std::map<std::string, bool> leastCostFound;

  //for each node in our m_lsTable keys, add it to leastCostFound
  //TRAFFIC_LOG("DIJKSTRA: Iterating through LSTABLE");
  for(lstEntry it = m_lsTable.begin(); it != m_lsTable.end(); it++) {
    //std::cout << it->first << '\n';
    leastCostFound.insert(std::make_pair(it->first, false));
  }

  //INITIALIZATION
  //name for this node
  std::string thisNode = ReverseLookup(m_mainAddress);
  //get neighbor for this node
  std::vector<Ipv4Address> neighbors;
  GetNeighbors(thisNode, neighbors);

  //PRINT OUT
  //std::cout << "My own neighbors: ";
  //for (int i = 0; i < neighbors.size(); i++) {
  //  std::cout << neighbors[i] << ", ";
 // }
  //std::cout << "\b\b\n";

  //insert this node into the routing table
  RoutingTableEntry entry = { m_mainAddress, m_mainAddress.Get(), m_mainAddress,
    m_mainAddress, 0}; 
  m_routingTable.insert(std::make_pair(thisNode, entry));

  for (int i = 0; i < neighbors.size(); i++) {
    Ipv4Address nbr = neighbors[i];
    RoutingTableEntry entry = { nbr, nbr.Get(), nbr,
      m_neighborTable.find(ReverseLookup(nbr))->second.interfaceAddr,
      DistanceToNeighbor(thisNode, ReverseLookup(nbr))};
    //add the new entry to our routing table
    m_routingTable.insert(std::make_pair(ReverseLookup(nbr), entry));
  }

  //set this node to true in leastCostFound
  leastCostFound.find(thisNode)->second = true;

  //MAIN LOOP! woohooooooo
  for (unsigned i = 0; i < m_lsTable.size()-1; i++) {
    //pick node with least cost so far
    std::string current = GetMinCostNode(leastCostFound);
    leastCostFound.find(current)->second = true;

    //get its neighbors
    GetNeighbors(current, neighbors);

    //TRAFFIC_LOG("DIJKSTRA: Current node: " << current << " with neighbors:");
    

    for (unsigned j = 0; j < neighbors.size(); j++) {
      std::string nbrName = ReverseLookup(neighbors[j]);

      //std::cout << nbrName << " ";

      uint32_t old_cost;

      if (m_routingTable.find(nbrName) != m_routingTable.end()) {

        old_cost = m_routingTable.find(nbrName)->second.cost;

      } else {
        old_cost = std::numeric_limits<int>::max();
      }

      uint32_t new_cost = m_routingTable.find(current)->second.cost + 
        DistanceToNeighbor(current, nbrName);

      //std::cout << "old_cost: " << old_cost << " new_cost: " << new_cost << '\n';

      if (new_cost < old_cost) {
        Ipv4Address nextHopAddr = m_routingTable.find(current)->second.nextHopAddr;

        rtEntry e = m_routingTable.find(nbrName);
        if (e != m_routingTable.end()) {
        //    std::cout << "Updating cost\n";
            e->second.nextHopAddr = nextHopAddr;
            e->second.nextHopNum = nextHopAddr.Get();
            e->second.interfaceAddr = m_neighborTable.find(ReverseLookup(nextHopAddr))->second.interfaceAddr;
            e->second.cost = new_cost;
        } else {
         //   std::cout << "new rt entry\n";
            RoutingTableEntry rte = { neighbors[j], nextHopAddr.Get(), nextHopAddr,
               m_neighborTable.find(ReverseLookup(nextHopAddr))->second.interfaceAddr, new_cost};
               //add the new entry to our routing table
            m_routingTable.insert(std::make_pair(ReverseLookup(neighbors[j]), rte));
        }

      }

    }

  }
  //DumpRoutingTable();

}

bool
LSRoutingProtocol::SearchTable (RoutingTableEntry& out_entry, Ipv4Address dest)
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
LSRoutingProtocol::GetNextSequenceNumber ()
{
  return m_currentSequenceNumber++;
}

void 
LSRoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  m_staticRouting->NotifyInterfaceUp (i);
}
void 
LSRoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  m_staticRouting->NotifyInterfaceDown (i);
}
void 
LSRoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyAddAddress (interface, address);
}
void 
LSRoutingProtocol::NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address)
{
  m_staticRouting->NotifyRemoveAddress (interface, address);
}

void
LSRoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  m_ipv4 = ipv4;
  m_staticRouting->SetIpv4 (m_ipv4);
}

/******************************************************************************
 * Hello
 ******************************************************************************/
