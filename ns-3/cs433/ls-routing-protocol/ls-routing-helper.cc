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

#include "ns3/ls-routing-helper.h"
#include "ns3/ls-routing-protocol.h"

using namespace ns3;

LSRoutingHelper::LSRoutingHelper ()
{
  m_lsFactory.SetTypeId ("LSRoutingProtocol"); 
}

LSRoutingHelper::LSRoutingHelper (const LSRoutingHelper &o)
  :  m_lsFactory (o.m_lsFactory)
{
}

LSRoutingHelper*
LSRoutingHelper::Copy (void) const
{
  return new LSRoutingHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
LSRoutingHelper::Create (Ptr<Node> node) const
{
  Ptr<LSRoutingProtocol> lsProto = m_lsFactory.Create<LSRoutingProtocol> ();
  node->AggregateObject (lsProto); 
  return lsProto;
}

void
LSRoutingHelper::Set (std::string name, const AttributeValue &value)
{
  m_lsFactory.Set (name, value);
}

