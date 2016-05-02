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

#ifndef TEST_APP_H
#define TEST_APP_H

#include "ns3/comm-application.h"
#include "ns3/test-app-message.h"
#include "ns3/ping-request.h"

#include "ns3/ipv4-address.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include "ns3/socket.h"
#include "ns3/nstime.h"
#include "ns3/timer.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"

using namespace ns3;

class TestApp : public CommApplication
{
  public:
    static TypeId GetTypeId (void);
    TestApp ();
    virtual ~TestApp ();

    void SendPing (uint32_t nodeNumber, std::string pingMessage);
    void StartTraffic (uint32_t nodeNumber);
    void StopTraffic (uint32_t nodeNumber);
    void SendTraffic ();
    void RecvMessage (Ptr<Socket> socket);
    void ProcessPingReq (TestAppMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void ProcessPingRsp (TestAppMessage message, Ipv4Address sourceAddress, uint16_t sourcePort);
    void AuditPings ();
    uint32_t GetNextTransactionId ();

    // From CommApplication
    virtual void ProcessCommand (std::vector<std::string> tokens);
    virtual void SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap);
    virtual void SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap);
  private:
    virtual Ipv4Address ResolveNodeIpAddress (uint32_t nodeNumber);
    virtual std::string ReverseLookup (Ipv4Address ipv4Address); 

  protected:
    virtual void DoDispose ();
    
  private:
    virtual void StartApplication (void);
    virtual void StopApplication (void);

    uint32_t m_currentTransactionId;
    Ptr<Socket> m_socket;
    Time m_pingTimeout;
    uint16_t m_appPort;
    std::map<uint32_t, Ipv4Address> m_nodeAddressMap;
    std::map<Ipv4Address, uint32_t> m_addressNodeMap;
    // Timers
    Timer m_auditPingsTimer;
    // Ping tracker
    std::map<uint32_t, Ptr<PingRequest> > m_pingTracker;
    // For traffic
    std::set<uint32_t> m_trafficSet;
    bool m_trafficRunning;
    Time m_trafficPeriod;
};

#endif


