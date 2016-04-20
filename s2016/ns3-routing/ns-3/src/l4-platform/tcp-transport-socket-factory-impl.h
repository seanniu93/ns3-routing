/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#ifndef TCP_TRANSPORT_SOCKET_FACTORY_IMPL_H
#define TCP_TRANSPORT_SOCKET_FACTORY_IMPL_H

#include "ns3/tcp-socket-factory.h"
#include "ns3/ptr.h"
#include "ns3/node.h"

namespace ns3 {

class TcpTransportSocketFactoryImpl : public TcpSocketFactory
{
public:
  TcpTransportSocketFactoryImpl ();
  virtual ~TcpTransportSocketFactoryImpl ();

  virtual Ptr<Socket> CreateSocket (void);
  void SetNode (Ptr<Node> node);

protected:
  virtual void DoDispose (void);

private:
  Ptr<Node> m_node;
};

} // namespace ns3
#endif
