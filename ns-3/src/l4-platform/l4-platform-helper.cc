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

#include "l4-platform-helper.h"
#include "l4-device.h"

namespace ns3 {

L4PlatformHelper::L4PlatformHelper ()
{
}

L4PlatformHelper::~L4PlatformHelper ()
{
}

void
L4PlatformHelper::Install (NodeContainer c) const
{
  for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
  {
    Install (*i);
  }
}

void
L4PlatformHelper::InstallAll (void) const
{
  Install (NodeContainer::GetGlobal ());
}

void
L4PlatformHelper::CreateAndAggregateObjectFromTypeId (Ptr<Node> node, const std::string typeId)
{
  ObjectFactory factory;
  factory.SetTypeId (typeId);
  Ptr<Object> protocol = factory.Create<Object> ();
  node->AggregateObject (protocol);
}

void
L4PlatformHelper::Install (Ptr<Node> node) const
{
  // Install TCP factory
  Ptr<TcpTransportSocketFactoryImpl> tcpFactory = CreateObject<TcpTransportSocketFactoryImpl> ();
  tcpFactory->SetNode (node);
  node->AggregateObject (tcpFactory);
  // Install UDP factory
  Ptr<UdpTransportSocketFactoryImpl> udpFactory = CreateObject<UdpTransportSocketFactoryImpl> ();
  udpFactory->SetNode (node);
  node->AggregateObject (udpFactory);
  // Install L4-Device
  Ptr<L4Device> device = Create<L4Device> ();
  node->AddDevice (device);
  device->Start ();
}

} // namespace ns3
