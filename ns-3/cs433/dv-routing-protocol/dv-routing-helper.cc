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

#include "ns3/dv-routing-helper.h"
#include "ns3/dv-routing-protocol.h"

using namespace ns3;

DVRoutingHelper::DVRoutingHelper ()
{
  m_dvFactory.SetTypeId ("DVRoutingProtocol"); 
}

DVRoutingHelper::DVRoutingHelper (const DVRoutingHelper &o)
  :  m_dvFactory (o.m_dvFactory)
{
}

DVRoutingHelper*
DVRoutingHelper::Copy (void) const
{
  return new DVRoutingHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
DVRoutingHelper::Create (Ptr<Node> node) const
{
  Ptr<DVRoutingProtocol> dvProto = m_dvFactory.Create<DVRoutingProtocol> ();
  node->AggregateObject (dvProto); 
  return dvProto;
}

void
DVRoutingHelper::Set (std::string name, const AttributeValue &value)
{
  m_dvFactory.Set (name, value);
}

