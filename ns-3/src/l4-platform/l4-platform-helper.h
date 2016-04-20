/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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

#ifndef L4_PLATFORM_HELPER_H
#define L4_PLATFORM_HELPER_H

#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/node.h"
#include "ns3/object-factory.h"
#include "tcp-transport-socket-factory-impl.h"
#include "udp-transport-socket-factory-impl.h"

namespace ns3 {

class L4PlatformHelper
{
public:  
  L4PlatformHelper(void);
  virtual ~L4PlatformHelper(void);

  void Install (std::string nodeName) const; 

  void Install (Ptr<Node> node) const;

  void Install (NodeContainer c) const;

  void InstallAll (void) const;
private:
  ObjectFactory m_tcpFactory, m_udpFactory;
  
  static void CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId);
  static void Cleanup (void);
  static uint32_t GetNodeIndex (std::string context);
};
}
#endif
