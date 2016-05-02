/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Universita' di Firenze, Italy
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
 *
 * Author: Tommaso Pecorella (tommaso.pecorella@unifi.it)
 * Author: Valerio Sartini (Valesar@gmail.com)
 */

#include <fstream>
#include <cstdlib>
#include <sstream>

#include "ns3/log.h"
#include "ns3/canvas-location.h"
#include "inet-topology-reader.h"
#include "ns3/vector.h"

using namespace std;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("InetTopologyReader");

NS_OBJECT_ENSURE_REGISTERED (InetTopologyReader);

TypeId InetTopologyReader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::InetTopologyReader")
    .SetParent<Object> ()
  ;
  return tid;
}

InetTopologyReader::InetTopologyReader ()
{
  NS_LOG_FUNCTION (this);
}

InetTopologyReader::~InetTopologyReader ()
{
  NS_LOG_FUNCTION (this);
}

NodeContainer
InetTopologyReader::Read (void)
{
  ifstream topgen;
  topgen.open (GetFileName ().c_str ());
  map<string, Ptr<Node> > nodeMap;
  map<string, string> xCoord;
  map<string, string> yCoord;
  map<string, Ptr<Node> > containerMap;
  NodeContainer nodes;

  if ( !topgen.is_open () )
    {
      return nodes;
    }

  string from;
  string to;
  string linkAttr;

  int linksNumber = 0;
  int nodesNumber = 0;

  int totnode = 0;
  int totlink = 0;

  istringstream lineBuffer;
  string line;

  getline (topgen,line);
  lineBuffer.str (line);

  lineBuffer >> totnode;
  lineBuffer >> totlink;
  NS_LOG_INFO ("Inet topology should have " << totnode << " nodes and " << totlink << " links");

  for (int i = 0; i < totnode; i++)
    {
      getline (topgen,line);
      lineBuffer.clear ();
      lineBuffer.str (line);

      string x, y, n;
      lineBuffer >> n;
      lineBuffer >> x;
      lineBuffer >> y;
      xCoord[n] = x;
      yCoord[n] = y;
      // Create here
      Ptr<Node> tmpNode = CreateObject<Node> ();
      Ptr<CanvasLocation> loc = CreateObject<CanvasLocation> ();
      tmpNode->AggregateObject (loc);
      std::istringstream strX (xCoord[n]); 
      std::istringstream strY (yCoord[n]);
      int xCoordinate, yCoordinate;
      strX >> xCoordinate;
      strY >> yCoordinate;
      Vector locVec (xCoordinate, yCoordinate, 0);
      loc->SetLocation (locVec);
      containerMap[n] = tmpNode;
      nodes.Add (tmpNode);
    }

  for (int i = 0; i < totlink && !topgen.eof (); i++)
    {
      getline (topgen,line);
      lineBuffer.clear ();
      lineBuffer.str (line);

      lineBuffer >> from;
      lineBuffer >> to;
      lineBuffer >> linkAttr;

      if ( (!from.empty ()) && (!to.empty ()) )
        {
          NS_LOG_INFO ( linksNumber << " From: " << from << " to: " << to );

          if ( nodeMap[from] == 0 )
            {
              /*Ptr<Node> tmpNode = CreateObject<Node> ();
              Ptr<CanvasLocation> loc = CreateObject<CanvasLocation> ();
              tmpNode->AggregateObject (loc);
              std::istringstream strX (xCoord[from]); 
              std::istringstream strY (yCoord[from]);
              int xCoordinate, yCoordinate;
              strX >> xCoordinate;
              strY >> yCoordinate;
              Vector locVec (xCoordinate, yCoordinate, 0);
              loc->SetLocation (locVec);
              nodeMap[from] = tmpNode;
              nodes.Add (tmpNode);*/
              Ptr<Node> tmpNode = containerMap[from];
              nodeMap[from] = tmpNode;
              nodesNumber++;
            }

          if (nodeMap[to] == 0)
            {
              /*Ptr<Node> tmpNode = CreateObject<Node> ();
              nodeMap[to] = tmpNode;
              Ptr<CanvasLocation> loc = CreateObject<CanvasLocation> ();
              tmpNode->AggregateObject (loc);
              std::istringstream strX (xCoord[to]); 
              std::istringstream strY (yCoord[to]);
              int xCoordinate, yCoordinate;
              strX >> xCoordinate;
              strY >> yCoordinate;
              Vector locVec (xCoordinate, yCoordinate, 0);
              loc->SetLocation (locVec);
              nodes.Add (tmpNode);*/
              Ptr<Node> tmpNode = containerMap[to];
              nodeMap[to] = tmpNode;
              nodesNumber++;
            }
          Link link ( nodeMap[from], from, nodeMap[to], to );
          if ( !linkAttr.empty () )
            {
              link.SetAttribute ("Weight", linkAttr);
            }
          AddLink (link);

          linksNumber++;
        }
    }

  NS_LOG_INFO ("Inet topology created with " << nodesNumber << " nodes and " << linksNumber << " links");
  topgen.close ();

  return nodes;
}

} /* namespace ns3 */
