/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
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

#ifndef TRANSPORT_SELECT_H
#define TRANSPORT_SELECT_H
#include "sys/select.h"
#include "sys/socket.h"
#include "ns3/object.h"
#include "ns3/simulator.h"
#include "ns3/net-device.h"
#include "ns3/realtime-simulator-impl.h"
namespace ns3 {

#define TS_CONTROL_MSG_SZ (1 + sizeof(int))
class TransportSelect
{
  public:
    enum TransportSelectOperation {
      SELECT_ADD = 1,
      SELECT_REMOVE = 2,
      SELECT_ADD_WRITE = 3,
      SELECT_REMOVE_WRITE = 4,
      SELECT_CLOSE = 5,
      SHUTDOWN = 9,
    };

    TransportSelect (int controlFd, Ptr<NetDevice> netDevice);
    TransportSelect ();
    ~TransportSelect ();
    
    void Run (void);
    void ShutDown ();

  private:
    fd_set m_masterReadFds, m_masterWriteFds, m_masterExceptionFds;
    fd_set m_readFds, m_writeFds, m_exceptionFds;
    int m_fdMax;
    int m_controlFd;
    RealtimeSimulatorImpl *m_rtImpl;
    void ReadInt (unsigned char* buf, int& num);
    Ptr<NetDevice> m_l4Device;


};


} // namespace ns3
#endif
