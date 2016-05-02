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

#ifndef COMM_APPLICATION_H
#define COMM_APPLICATION_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include <map>

#include "ns3/comm-log.h"

using namespace ns3; 

class CommApplication : public Application, public CommLog
{
  public:
   static TypeId GetTypeId (void);
   CommApplication ();
   virtual ~CommApplication ();

   // Interface for CommApplication(s)
   virtual void ProcessCommand (std::vector<std::string> tokens) = 0;
   virtual void SetNodeAddressMap (std::map<uint32_t, Ipv4Address> nodeAddressMap) = 0;
   virtual void SetAddressNodeMap (std::map<Ipv4Address, uint32_t> addressNodeMap) = 0;
  private:
    virtual Ipv4Address ResolveNodeIpAddress (uint32_t nodeNumber) = 0;
    virtual std::string ReverseLookup (Ipv4Address ipv4Address) = 0; 

  protected:
    virtual void DoDispose ();

  private:
    virtual void StartApplication (void) = 0;
    virtual void StopApplication (void) = 0; 

};    

#endif
