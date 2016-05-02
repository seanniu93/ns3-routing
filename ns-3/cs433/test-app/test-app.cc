/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 University of Pennsylvania
 *
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


#include "test-app.h"

#include "ns3/random-variable.h"
#include "ns3/inet-socket-address.h"

using namespace ns3;

TypeId
TestApp::GetTypeId ()
{
  static TypeId tid = TypeId ("TestApp")
    .SetParent<CommApplication> ()
    .AddConstructor<TestApp> ()
    .AddAttribute ("AppPort",
                   "Listening port for Application",
                   UintegerValue (10000),
                   MakeUintegerAccessor (&TestApp::m_appPort),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("PingTimeout",
                   "Timeout value for PING_REQ in milliseconds",
                   TimeValue (MilliSeconds (2000)),
                   MakeTimeAccessor (&TestApp::m_pingTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("TrafficPeriod",
                   "Traffic Period in milliseconds",
                   TimeValue (MilliSeconds (2)),
                   MakeTimeAccessor (&TestApp::m_trafficPeriod),
                   MakeTimeChecker ());


    ;
  return tid;
}

TestApp::TestApp ()
  : m_auditPingsTimer (Timer::CANCEL_ON_DESTROY)
{
  RandomVariable random;
  SeedManager::SetSeed (time (NULL));
  random = UniformVariable (0x00000000, 0xFFFFFFFF);
  m_currentTransactionId = random.GetInteger ();
  m_trafficRunning = false;
}

TestApp::~TestApp ()
{

}

void
TestApp::DoDispose ()
{
  StopApplication ();
  CommApplication::DoDispose ();
}

void
TestApp::StartApplication (void)
{
  if (m_socket == 0)
    { 
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny(), m_appPort);
      m_socket->Bind (local);
      m_socket->SetRecvCallback (MakeCallback (&TestApp::RecvMessage, this));
    }  
  
  // Configure timers
  m_auditPingsTimer.SetFunction (&TestApp::AuditPings, this);
  // Start timers
  m_auditPingsTimer.Schedule (m_pingTimeout);
}

void
TestApp::StopApplication (void)
{
  // Close socket
  if (m_socket)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
      m_socket = 0;
    }

  // Cancel timers
  m_auditPingsTimer.Cancel ();

  m_pingTracker.clear ();
}


void
TestApp::SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap)
{
  m_nodeAddressMap = nodeAddressMap;
}

void
TestApp::SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap)
{
  m_addressNodeMap = addressNodeMap;
}

Ipv4Address
TestApp::ResolveNodeIpAddress (uint32_t nodeNumber)
{
  std::map<uint32_t, Ipv4Address>::iterator iter = m_nodeAddressMap.find (nodeNumber);
  if (iter != m_nodeAddressMap.end ())
    { 
      return iter->second;
    }
  return Ipv4Address::GetAny ();
}

std::string
TestApp::ReverseLookup (Ipv4Address ipAddress)
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
TestApp::ProcessCommand (std::vector<std::string> tokens)
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
      if (*iterator != "*")
        {
          std::istringstream sin (*iterator);
          uint32_t nodeNumber;
          sin >> nodeNumber;
          iterator++;
          std::string pingMessage = *iterator;
          SendPing (nodeNumber, pingMessage);
        }
      else
        {
          iterator++;
          std::string pingMessage = *iterator;
          std::map<uint32_t, Ipv4Address>::iterator iter;
          for (iter = m_nodeAddressMap.begin () ; iter != m_nodeAddressMap.end (); iter++)  
            {
              SendPing (iter->first, pingMessage);
            }
        }
    }
  else if (command == "TRAFFIC")
    {
      if (tokens.size() < 3)
        {
          ERROR_LOG ("Insufficient TRAFFIC params...");
          return;
        }
      iterator++;
      std::string option = (*iterator);
      iterator++;
      if (*iterator != "*")
        {
          std::istringstream sin (*iterator);
          uint32_t nodeNumber;
          sin >> nodeNumber;
          if (option == "START")
            {
              DEBUG_LOG ("Starting Traffic for node: " << nodeNumber);
              StartTraffic (nodeNumber);
            }
          else if (option == "STOP")
            {
              DEBUG_LOG ("Starting Traffic for node: " << nodeNumber);
              StopTraffic (nodeNumber);
            }
        }
      else
        {
          std::map<uint32_t, Ipv4Address>::iterator iter;
          for (iter = m_nodeAddressMap.begin () ; iter != m_nodeAddressMap.end (); iter++)  
            {
              if (option == "START")
                {
                  DEBUG_LOG ("<*>Starting Traffic for node: " << iter->first);
                  StartTraffic (iter->first);
                }
              else if (option == "STOP")
                {
                  DEBUG_LOG ("<*>Stop Traffic for node: " << iter->first);
                  StopTraffic (iter->first);
                }
            }
        }
    }
}

void 
TestApp::StartTraffic (uint32_t nodeNumber)
{
  Ipv4Address destAddress = ResolveNodeIpAddress (nodeNumber);
  if (destAddress == Ipv4Address::GetAny ())
    {   
      ERROR_LOG ("Invalid node number.");
      return;
    }
  // Add to traffic set
  m_trafficSet.insert (nodeNumber);
  if (!m_trafficRunning)
    {
      m_trafficRunning = true;
      Simulator::ScheduleNow (&TestApp::SendTraffic, this);
    }
}

void
TestApp::StopTraffic (uint32_t nodeNumber)
{
  std::set<uint32_t>::iterator iterator = m_trafficSet.find (nodeNumber);
  if (iterator == m_trafficSet.end ())
    return;

  m_trafficSet.erase (iterator);
  if (m_trafficSet.size () != 0)
    m_trafficRunning = false;
}

void
TestApp::SendTraffic ()
{
  if (!m_trafficRunning)
    return;
  std::set<uint32_t>::iterator iterator;
  for (iterator = m_trafficSet.begin () ; iterator != m_trafficSet.end () ; iterator++)
    {
      SendPing (*iterator, "****TRAFFIC_MSG****");
    }
  // Reschedule
  Simulator::Schedule (m_trafficPeriod, &TestApp::SendTraffic, this);
}

void
TestApp::SendPing (uint32_t nodeNumber, std::string pingMessage)
{
  Ipv4Address destAddress = ResolveNodeIpAddress (nodeNumber);
  if (destAddress != Ipv4Address::GetAny ())
    {
      uint32_t transactionId = GetNextTransactionId ();
      TRAFFIC_LOG ("Sending PING_REQ to Node: " << nodeNumber << " IP: " << destAddress << " Message: " << pingMessage << " transactionId: " << transactionId);
      Ptr<PingRequest> pingRequest = Create<PingRequest> (transactionId, Simulator::Now(), destAddress, pingMessage);
      // Add to ping-tracker
      m_pingTracker.insert (std::make_pair (transactionId, pingRequest));
      Ptr<Packet> packet = Create<Packet> ();
      TestAppMessage message = TestAppMessage (TestAppMessage::PING_REQ, transactionId);
      message.SetPingReq (pingMessage);
      packet->AddHeader (message);
      m_socket->SendTo (packet, 0 , InetSocketAddress (destAddress, m_appPort));
    }
}

void
TestApp::RecvMessage (Ptr<Socket> socket)
{
  Address sourceAddr;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddr);
  InetSocketAddress inetSocketAddr = InetSocketAddress::ConvertFrom (sourceAddr);
  Ipv4Address sourceAddress = inetSocketAddr.GetIpv4 ();
  uint16_t sourcePort = inetSocketAddr.GetPort ();
  TestAppMessage message;
  packet->RemoveHeader (message);

  switch (message.GetMessageType ())
    {
      case TestAppMessage::PING_REQ:
        ProcessPingReq (message, sourceAddress, sourcePort);
        break;
      case TestAppMessage::PING_RSP:
        ProcessPingRsp (message, sourceAddress, sourcePort);
        break;
      default:
        ERROR_LOG ("Unknown Message Type!");
        break;
    }
}

void
TestApp::ProcessPingReq (TestAppMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{

    // Use reverse lookup for ease of debug
    std::string fromNode = ReverseLookup (sourceAddress);
    TRAFFIC_LOG ("Received PING_REQ, From Node: " << fromNode << ", Message: " << message.GetPingReq().pingMessage);
    // Send Ping Response
    TestAppMessage resp = TestAppMessage (TestAppMessage::PING_RSP, message.GetTransactionId());
    resp.SetPingRsp (message.GetPingReq().pingMessage);
    Ptr<Packet> packet = Create<Packet> ();
    packet->AddHeader (resp);
    m_socket->SendTo (packet, 0 , InetSocketAddress (sourceAddress, sourcePort));
}

void
TestApp::ProcessPingRsp (TestAppMessage message, Ipv4Address sourceAddress, uint16_t sourcePort)
{
  // Remove from pingTracker
  std::map<uint32_t, Ptr<PingRequest> >::iterator iter;
  iter = m_pingTracker.find (message.GetTransactionId ());
  if (iter != m_pingTracker.end ())
    {
      std::string fromNode = ReverseLookup (sourceAddress);
      TRAFFIC_LOG ("Received PING_RSP, From Node: " << fromNode << ", Message: " << message.GetPingRsp().pingMessage);
      m_pingTracker.erase (iter);
    }
  else
    {
      DEBUG_LOG ("Received invalid PING_RSP!");
    }
}

void
TestApp::AuditPings ()
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

uint32_t
TestApp::GetNextTransactionId ()
{
  return m_currentTransactionId++;
}


