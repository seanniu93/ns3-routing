/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Pennsylvania
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

#ifndef L4_DEVICE_H
#define L4_DEVICE_H

#include <string.h>
#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/event-id.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "ns3/system-thread.h"
#include "ns3/realtime-simulator-impl.h"
#include "ns3/socket.h"
#include <map>
#include "transport-socket.h"
#include "transport-select.h"

namespace ns3 {

class Node;

class FDType 
{
  public:

  enum Type
  {
    SOCKET = 1,
    GENERIC = 2,
  };

  Type m_type;
  Ptr<Socket> m_socket;
  Callback<void, int> m_dataIndFn; 
  Callback<void, int> m_dataWriteIndFn; 
  Callback<void, int> m_closeIndFn; 
};

class L4Device : public NetDevice
{
public:
  static TypeId GetTypeId (void);

  L4Device ();
  virtual ~L4Device ();

  virtual void SetIfIndex(const uint32_t index);
  virtual uint32_t GetIfIndex(void) const;
  virtual Ptr<Channel> GetChannel (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);
  virtual bool NeedsArp (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual void SetPromiscReceiveCallback (NetDevice::PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom () const;
  virtual Address GetMulticast (Ipv6Address addr) const;

  //Start/Stop
  void Start ();
  void Stop ();
  void AddReadFd (int fd, Ptr<Socket> socket);
  void AddGenericReadFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn);
  void AddGenericWriteFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn);
  void RemoveReadFd (int fd);
  void AddWriteFd (int fd, Ptr<Socket> socket);
  void RemoveWriteFd (int fd);
  void FdOperation (TransportSelect::TransportSelectOperation op, int fd);
  void ReadFdReady (int fd);
  void WriteFdReady (int fd);
  void ExceptionFd (int fd);
  void CloseFd (int fd);
  void AddFd (int fd, Ptr<Socket> socket);
  void AddGenericFd (int fd,
                        Callback<void, int> dataIndFn,
                        Callback<void, int> dataWriteIndFn,
                        Callback<void, int> closeIndFn);

private:

  Ptr<Node> m_node;
  int m_selectNotifyFd [2];
  TransportSelect m_transportSelect;
  Ptr<SystemThread> m_transportSelectThread;


  uint32_t m_ifIndex;

  uint16_t m_mtu;
  Address m_address;

  typedef std::map<int, FDType> FDMap;
  FDMap m_fdMap;

};

} // namespace ns3

#endif /* L4_DEVICE_H */
