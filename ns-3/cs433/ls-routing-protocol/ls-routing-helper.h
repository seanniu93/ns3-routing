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


#ifndef LS_ROUTING_HELPER_H
#define LS_ROUTING_HELPER_H

#include "ns3/ipv4-routing-helper.h"
#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ls-routing-protocol.h"

using namespace ns3;

class LSRoutingHelper : public Ipv4RoutingHelper
{
  public:
    LSRoutingHelper ();

    LSRoutingHelper (const LSRoutingHelper &);

    LSRoutingHelper* Copy (void) const;
  
    virtual Ptr<Ipv4RoutingProtocol> Create (Ptr<Node> node) const;

    void Set (std::string name, const AttributeValue &value);

  private:
    LSRoutingHelper &operator = (const LSRoutingHelper &o);
    ObjectFactory m_lsFactory;
};

#endif
