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
}

Ptr<Ipv4Route>
DVRoutingProtocol::RouteOutput (Ptr<Packet> packet, const Ipv4Header &header, Ptr<NetDevice> outInterface, Socket::SocketErrno &sockerr)
{
  Ptr<Ipv4Route> ipv4Route = m_staticRouting->RouteOutput (packet, header, outInterface, sockerr);
  if (ipv4Route)
    {
      DEBUG_LOG ("Found route to: " << ipv4Route->GetDestination () << " via next-hop: " << ipv4Route->GetGateway () << " with source: " << ipv4Route->GetSource () << " and output device " << ipv4Route->GetOutputDevice());
    }
  else
    {
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
  Ipv4Address destinationAddress = header.GetDestination ();
  Ipv4Address sourceAddress = header.GetSource ();

  // Drop if packet was originated by this node
  if (IsOwnAddress (sourceAddress) == true)
    {
      return true;
    }

  // Check for local delivery
  uint32_t interfaceNum = m_ipv4->GetInterfaceForDevice (inputDev);
  if (m_ipv4->IsDestinationAddress (destinationAddress, interfaceNum))
    {
      if (!lcb.IsNull ())
        {
          lcb (packet, header, interfaceNum);
          return true;
        }
      else
        {
          return false;
        }
    }

  // Check static routing table
  if (m_staticRouting->RouteInput (packet, header, inputDev, ucb, mcb, lcb, ecb))
    {
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
    }
}

void
DVRoutingProtocol::SendHello () {
  Ipv4Address destAddress = ResolveNodeIpAddress (0); // TODO remove this and "hello"
  uint32_t sequenceNumber = GetNextSequenceNumber ();
//  TRAFFIC_LOG ("Broadcasting HELLO_REQ, SequenceNumber: " << sequenceNumber);
//    //Ptr<HelloRequest> helloRequest = Create<HelloRequest> (sequenceNumber, Simulator::Now(), destAddress, "hello", m_helloTimeout);
   Ptr<Packet> packet = Create<Packet> ();
   DVMessage dvMessage = DVMessage (DVMessage::HELLO_REQ, sequenceNumber, 1, m_mainAddress);
   dvMessage.SetHelloReq (destAddress, "hello");
   packet->AddHeader (dvMessage);
   BroadcastPacket (packet);
}

// TODO: How does this need to change?
void
DVRoutingProtocol::SendDVTableMessage () {
  uint32_t sequenceNumber = GetNextSequenceNumber ();

  std::vector<Ipv4Address> neighborAddrs;
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end (); i++) {
    neighborAddrs.push_back( i->second.neighborAddr );
  }

  //TRAFFIC_LOG("Sending LSTableMessage: sequence num: " << sequenceNumber << ", neighborAddrs.size: " << neighborAddrs.size());
  //Ptr<LSTableMessage> lsTableMsg = Create<LSTableMessage> (sequenceNumber, Simulator::Now(), neighborAddrs);
  Ptr<Packet> packet = Create<Packet> ();
  DVMessage dvMessage = DVMessage (DVMessage::DV_TABLE_MSG, sequenceNumber, 1, m_mainAddress);
  dvMessage.SetDVTableMsg(neighborAddrs);
  packet->AddHeader (dvMessage);
  BroadcastPacket (packet);
}


void
DVRoutingProtocol::DumpNeighbors ()
{
  STATUS_LOG (std::endl << "**************** Neighbor List ********************" << std::endl
              << "NeighborNumber\t\tNeighborAddr\t\tInterfaceAddr");
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
        ProcessDVTableMessage(dvMessage);
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
    NeighborTableEntry entry = { neighAddr, interfaceAddr , Simulator::Now() };
    m_neighborTable.insert(std::make_pair(fromNode, entry));

    //also add it to the lsTable (with neighbor information for all nodes in network)
    //see if this node is already in that table
    dvtEntry dvte = m_dvTable.find( ReverseLookup(m_mainAddress) );
    if ( dvte != m_dvTable.end() ) {//if this node is already in there
      //then just add this neighbor to the neighbors vector
      dvte->second.neighborCosts.push_back(std::make_pair(neighAddr, 1));//TODO: change cost here

    } else {
      std::vector<std::pair<Ipv4Address, uint32_t> > nbrCosts;
      nbrCosts.push_back(std::make_pair(neighAddr, 1));  //TODO: change cost here
      DVTableEntry dvtEntry = { nbrCosts, dvMessage.GetSequenceNumber() };
      m_dvTable.insert(std::make_pair(ReverseLookup(m_mainAddress), dvtEntry));

    }

    //TODO: run Bellman Ford

    SendDVTableMessage(); // neighbor table has changed, so resend neighbor info
  }

}

// TODO: how does this need to change?
void
DVRoutingProtocol::ProcessDVTableMessage (DVMessage dvMessage) {
    std::vector<Ipv4Address> neighborAddrs = dvMessage.GetDVTableMsg().neighbors;
    uint32_t seqNum = dvMessage.GetSequenceNumber();

//    TRAFFIC_LOG("Received from: " << ReverseLookup(dvMessage.GetOriginatorAddress()) << "\n Neighbors: " );
//    for (unsigned i = 0; i < neighborAddrs.size(); ++i) {
//        std::cout << neighborAddrs[i] << " " << ReverseLookup(neighborAddrs[i]) << '\n';
//    }
//    std::cout << '\n';

    // if we have not seen the packet before, broadcast it
    Ipv4Address fromAddr = dvMessage.GetOriginatorAddress();
    std::string fromNode = ReverseLookup(fromAddr);

    dvtEntry entry = m_dvTable.find(fromNode);

//    TRAFFIC_LOG("Sequence Number of this entry: " << entry->second.sequenceNumber << " SeqNum of new packet: " << seqNum);

    // Technically, I think we don't need the seqNum for dv because there is not
    // way an old packet will arrive after a newer one if the messages are only neighbor-to-neighbor
    if (entry == m_dvTable.end() || seqNum > entry->second.sequenceNumber) {

      //if it was already in the table, delete it first
      if (entry != m_dvTable.end())
        m_dvTable.erase(entry);

      // Add to DVTable
      std::vector<std::pair<Ipv4Address, uint32_t> > neighborCosts;
      for (int i = 0; i < neighborAddrs.size(); i++) {
        neighborCosts.push_back(std::make_pair(neighborAddrs[i], 1)); // TODO: change cost here
      }
      DVTableEntry newEntry = { neighborCosts, seqNum };
      m_dvTable.insert(std::pair<std::string, DVTableEntry>(fromNode, newEntry));

      // Run Bellman Ford

      // Send new packet with neighbor info
      SendDVTableMessage();
    }
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
  for (ntEntry i = m_neighborTable.begin (); i != m_neighborTable.end (); i++) {
      NeighborTableEntry entry = i->second;
  //    TRAFFIC_LOG ("AUDIT HELLOS: entry.lastUpdated: " << entry.lastUpdated.GetMilliSeconds() << ", timeout: " << m_helloTimeout.GetMilliSeconds() << ", time is now: " << Simulator::Now().GetMilliSeconds());

  if ( entry.lastUpdated.GetMilliSeconds() + m_helloTimeout.GetMilliSeconds() <= Simulator::Now().GetMilliSeconds()) {
         removeDVTableLink( m_mainAddress, entry.neighborAddr );
         m_neighborTable.erase(i);
         sendMsg = true;
      }
  }

  if (sendMsg) {
      SendDVTableMessage(); // neighbor table info has changed, so resend neighbor info
  }

  // Reschedule timer
  m_auditHellosTimer.Schedule (m_helloTimeout);
}

void
DVRoutingProtocol::removeDVTableLink(Ipv4Address node1, Ipv4Address node2) {
    dvtEntry entry1 = m_dvTable.find( ReverseLookup(node1) );
    dvtEntry entry2 = m_dvTable.find( ReverseLookup(node2) );

    if (entry1 != m_dvTable.end()) {
        std::vector<std::pair<Ipv4Address, uint32_t> > pairs1 = entry1->second.neighborCosts;
        for (unsigned i = 0; i < pairs1.size(); i++) {
            if (pairs1[i].first == node2) {
                pairs1.erase(pairs1.begin() + i);
            }
        }
    }

    if (entry2 != m_dvTable.end()) {
        std::vector<std::pair<Ipv4Address, uint32_t> > pairs2 = entry2->second.neighborCosts;
        for (unsigned i = 0; i < pairs2.size(); i++) {
            if (pairs2[i].first == node1) {
                pairs2.erase(pairs2.begin() + i);
            }
        }
    }
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
