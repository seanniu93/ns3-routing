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
#ifndef UDP_TRANSPORT_SOCKET_IMPL_H
#define UDP_TRANSPORT_SOCKET_IMPL_H

#include <stdint.h>
#include <queue>
#include "ns3/callback.h"
#include "ns3/traced-callback.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/udp-socket.h"
#include "transport-socket.h"
#include "l4-device.h"

namespace ns3 {

class Node;
class Packet;


class UdpTransportSocketImpl : public TransportSocket
{
public:
  static TypeId GetTypeId (void);
  UdpTransportSocketImpl ();
  virtual ~UdpTransportSocketImpl ();

  virtual enum SocketErrno GetErrno (void) const;
  virtual Ptr<Node> GetNode (void) const;
  virtual int Bind (void);
  virtual int Bind (const Address &address);
  virtual int Close (void);
  virtual int ShutdownSend (void);
  virtual int ShutdownRecv (void);
  virtual int Connect(const Address &address);
  virtual int Listen (void);
  virtual uint32_t GetTxAvailable (void) const;
  virtual int Send (Ptr<Packet> p, uint32_t flags);
  virtual int SendTo (Ptr<Packet> p, uint32_t flags, const Address &address);
  virtual uint32_t GetRxAvailable (void) const;
  virtual Ptr<Packet> Recv (uint32_t maxSize, uint32_t flags);
  virtual Ptr<Packet> RecvFrom (uint32_t maxSize, uint32_t flags,
    Address &fromAddress);
  virtual int GetSockName (Address &address) const; 
  virtual void BindToNetDevice (Ptr<NetDevice> netdevice);
  virtual bool SetAllowBroadcast (bool allowBroadcast);
  virtual bool GetAllowBroadcast () const;

  void SetNode (Ptr<Node> node);
  void DataInd (void);
  void DataWriteInd (void);
  void CloseInd (void);



private:
  void ForwardUp (Ptr<Packet> p, Ipv4Address ipv4, uint16_t port);
  Ipv4Address m_defaultAddress;
  uint16_t m_defaultPort;
  enum SocketErrno m_errno;
  Ptr<Node> m_node;
  int m_socket;
  uint16_t m_port;
  Ptr<L4Device> m_l4Device;
  uint8_t *m_packetBuffer;
};

}//namespace ns3

#endif /* UDP_SOCKET_IMPL_H */
