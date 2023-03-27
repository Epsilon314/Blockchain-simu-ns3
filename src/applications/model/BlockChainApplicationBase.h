// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#ifndef BLOCKCHAINAPPLICATIONBASE_H
#define BLOCKCHAINAPPLICATIONBASE_H

#include <vector>
#include <set>
#include <map>
#include <list>
#include <queue>
#include <functional>
#include <random>
#include <chrono>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/application.h"

#include "ns3/ConsensusMessage.h"
#include "ns3/MessageRecvPool.h"



namespace ns3 {

// class Socket;
// class Address;

/*
 * transport policy for blockchain messages
 *
 * DIRECT : knows the ip address and route of all peers
 * RELAY : follow a application-layer relay table to broadcast messages
 */


/**
 * entry of the application-layer relay table
 * src : the origin of the message, currectly is same to message signer 
 * TODO: use a proxy to simulate applications that require ip privacy
 * from : previous hop 
 * 
 */
struct RelayEntry {

  int src;
  int from;

  RelayEntry();

  RelayEntry(int s, int f);

  bool operator<(const RelayEntry &that) const;

};


struct SendQuest {

  Ptr<Packet> mPkt;
  std::vector<int> mReceivers;

  SendQuest(Ptr<Packet> pkt, std::vector<int> receivers) 
    : mPkt(pkt), mReceivers(receivers) {}

  SendQuest() {}

};

typedef std::map<RelayEntry, std::vector<int> > RelayMap;

class SendQuestBuffer {

private:

  std::list<SendQuest> sendBuffer;
  SendQuest currentQuest;
  int currentIndex;

public:

  SendQuestBuffer(void);
  ~SendQuestBuffer() {}
  bool empty();

  std::pair<Ptr<Packet>, int > getNext();
  void insert(SendQuest quest);

};


/**
 * Base abstract of a blockchain application
 * contain basic communication primitives 
 * TODO: abstract the common consensus process 
 */
template<typename MessageType>
class BlockChainApplicationBase : public Application {

public:

  enum REPLICA_STAT : uint8_t {
    RUNNING,
    CRASH,
    FAULT
  };

  enum RELAY_TYPE : uint8_t {
    SEQUENCIAL,
    PARALLEL
  };

  BlockChainApplicationBase(void);
  virtual ~BlockChainApplicationBase (void);

  void AddPeer(int id, const Address add);

  void AddDirectPeer(int id);
  void AddPeerMetric(int id, double metric);

  void acceptPeer(int id, int src, const Address add);
  void joinPeer(int id, const Address add);

  std::pair<RelayMap, RelayMap> disablePeer();
  void handoverPeer(std::pair<RelayMap, RelayMap>, int id, int source);

  void insertRelayLargePacket(int src, int from, int to);
  void insertRelaySmallPacket(int src, int from, int to);

  void AddCorePeer(int id);
  void AddCorePeerMetric(int id, double distance);

  void changeStatus(uint8_t type);

  void dateSentCallback(Ptr<Socket> sock, uint32_t sent);

  void onMessageCallback(MessageType msg);

  void sendToPeer(Ptr<Packet> pkt, int recv);
  void sendToPeer(Ptr<Packet> pkt, std::vector<int> recv);

  void BroadcastToPeers(Ptr<Packet> pkt);
  void BroadcastToPeers(Ptr<Packet> pkt, RelayEntry k);
  void BroadcastToPeers(Ptr<Packet> pkt, double delay);
  void BroadcastToPeers(Ptr<Packet> pkt, double delay, RelayEntry k);

  void relay(MessageType msg);
  void flood(MessageType msg);
  void flood(MessageType msg, double delay);
  void floodAnyway(MessageType msg);

  inline int getNodeId() {return nodeId;}

  void setNodeId(int id) {nodeId = id;}
  void setDelay(double d) {mDelay = d;}
  void setTimeout(double t) {timeout = t;}
  void setRelayType(int t) {relayType = t;}
  void setTransferModel(int t) {transferModel = t;}

  void setDefaultTTL(int ttl) {defaultTTL = ttl;}
  void setDefaultFloodN(int n) {defaultFloodN = n;}
  void setFloodRandomization(bool b) {floodRandomization = b;}

  void installShortestPathRoute(std::vector<int> route);

  void clearDelaySendEvent();

  double getActiveRate();
  double getNodeStress();
  std::map<std::pair<u_int32_t, u_int32_t>, double> getLinkStress();

  void printState();

  void setOutboundBandwidth(double bw) {outboundBandwidth = bw;}

  void setMaxOutboundNumber(uint n) {max_outbound_number = n;}

protected:

  enum RelayTables : uint8_t {
    LARGEPKT,
    SMALLPKT,
    LARGEPKTALTER,
    SMALLPKTALTER
  };

  // unique id for each app
  // will be set by its helper
  uint32_t nodeId;

  // hardcoded port
  uint16_t port = 2333;

  u_int max_outbound_number = 12;

  // listen for incoming message
  Ptr<Socket> mListeningSocket;

  // peer socket, set up in startApplication and cannot be accessed at time 0
  std::map<int,Ptr<Socket>> mPeerSockets;
  
  // peer ip address
  std::map<int,const Address> mPeerAddress;

  // peer id list
  std::vector<int> mPeerList;

  // id of directly connected peers
  std::vector<int> directPeerList;

  std::vector<int> linkEstPeerList;

  std::map<int, double> peerMetric;

  std::vector<int> corePeerList;

  typedef std::pair<int, double> peerMetricEntry;
  typedef std::priority_queue<peerMetricEntry, std::vector<peerMetricEntry>, 
    std::function<bool(const peerMetricEntry&, const peerMetricEntry&)> > peerMetricPrioQueue;
  
  // first is the id of a core peer
  // second is the distance metric to that peer
  peerMetricPrioQueue corePeerMetric;

  double timeout;

  EventId timeoutEvent;
  EventId delaySendEvent;
  EventId delayGroupSendEvent;

  EventId delayedBroadcast;
  EventId delayedFlood;

  double mDelay;

  double outboundBandwidth; // bytes per second

  MessageRecvPool<MessageType> messageRecvPool;
 
  // TODO: do some tracing
  TracedCallback<Ptr<const Packet>> mRxTrace;

  int relayType;

  // define "large" packets and "small" packets
  uint32_t packetSizeThredhold = 500;  // bytes

  /*
  * the application-layer relay table
  * get next hop route from relay entry
  * next hop may be mutiple nodes
  */
  RelayMap relayTableLargePacket;
  RelayMap relayTableSmallPacket;

  std::vector<int> shortestPathRoute;

  int defaultTTL;
  int defaultFloodN;

  bool floodRandomization;

  uint32_t seq = 0;

  uint8_t replicaStat;

  int transferModel = PARALLEL;

  SendQuestBuffer sendquestbuffer;

  bool isSending = false;

  double lastStartTime = 0.0; // second
  double lastStopTime = 0.0; // second

  double lastSendingStateTransferTime = 0.0; // second

  double activePeriod = 0.0; // second
  double inactivePeriod = 0.0; // second

  uint32_t nodeStress = 0; // bytes passed through

  // <<from, to>, bytes passed through>
  std::map<std::pair<u_int32_t, u_int32_t>, double> linkStress;

  bool pooledMsg = true;

  virtual void DoDispose (void);
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  bool validateMessage(MessageType& msg);

  void RecvCallback (Ptr<Socket> socket);
  void AcceptCallback (Ptr<Socket> socket, const Address& from);
  void NormalCloseCallback (Ptr<Socket> socket);
  void ErrorCloseCallback (Ptr<Socket> socket);

  bool checkEventStatus(EventId eid);
  void clearTimeoutEvent();

  // if transport in flood or mixed mode and no randomization, rank peers and choose better ones
  void sortPeer();

  void sortCoreList();

  bool isCoreNode();

  void applicationLayerRelay(MessageType msg, int duplicates = 1);

  virtual void parseMessage(MessageType msg) = 0;

private:

  void sendTo(Ptr<Packet> pkt, int i);
  void sendTo(Ptr<Packet> pkt, int i, double delay);

  void sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers);
  void sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers, double delay);

  void sendNext();
  
  bool compareWRTMetric(const int &a, const int &b);

  bool compareWRTAccumlatedMetric(const int &a, const int &b);

};

// implementation of class BlockChainApplicationBase
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::DoDispose (void) {
  Application::DoDispose ();
}


template <typename MessageType>
BlockChainApplicationBase<MessageType>::BlockChainApplicationBase(void) {

  timeout = 1;
  corePeerMetric = peerMetricPrioQueue([](const peerMetricEntry &a, const peerMetricEntry &b)->bool {
      return a.second > b.second; 
    });

  mPeerSockets.clear();
  mPeerAddress.clear();
  peerMetric.clear();
  relayTableLargePacket.clear();
  relayTableSmallPacket.clear();
  outboundBandwidth = 0;

}


template <typename MessageType>
BlockChainApplicationBase<MessageType>::~BlockChainApplicationBase(void) {}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::StartApplication() {

  if (!mListeningSocket) {
    mListeningSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    mListeningSocket->Bind(local);
  }

  if (relayType == ConsensusMessageBase::FLOOD || relayType == ConsensusMessageBase::MIXED || 
      relayType == ConsensusMessageBase::INFECT_UPON_CONTAGION) {

    if (!floodRandomization) {
      sortPeer();
    }

    std::vector<int> sortBox;

    for (int i = 0; i < (int) directPeerList.size(); ++i) {
      sortBox.push_back(i);
    }

    if (floodRandomization) {
      // shuffle the box
      unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
      shuffle(sortBox.begin(), sortBox.end(), std::default_random_engine(seed));
    }

    for (int i = 0; i < (int) max_outbound_number && i < (int) directPeerList.size(); ++i) {
      linkEstPeerList.push_back(directPeerList[sortBox[i]]);
    }

  }

  /**
   * connect to peers
   * TODO: Now we simply connect to every peer and use part of these sockets \
   * w.r.t. transport strategies in use. We do not need to know all addresses \
   * and keep all sockets.
   */
  for (auto address : mPeerAddress) {
    Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress peer = InetSocketAddress(Ipv4Address::ConvertFrom(address.second), port);
    sock->Bind();
    sock->Connect(peer);

    switch (transferModel) {
    
    case PARALLEL:
      sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
      break;
    
    case SEQUENCIAL:
      sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
      break;
    
    default:
      break;
    }
    mPeerSockets.insert(std::pair<int,Ptr<Socket>>(address.first, sock));
  }

  // ready
  replicaStat = RUNNING;

  lastStartTime = Simulator::Now().GetSeconds();

  lastSendingStateTransferTime = lastStartTime;
  
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::StopApplication() {
  if (mListeningSocket) {
    mListeningSocket->Close();
  }

  for (auto peerSocket : mPeerSockets) {
    (peerSocket.second)->Close();
  }

  clearDelaySendEvent();

  lastStopTime = Simulator::Now().GetSeconds();

  if (isSending) {
    activePeriod += (lastStopTime - lastSendingStateTransferTime);
  }
  else {
    inactivePeriod += (lastStopTime - lastSendingStateTransferTime);
  }

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AcceptCallback(Ptr<Socket> socket, const Address& from) {
  socket->SetRecvCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::RecvCallback, this));
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::NormalCloseCallback (Ptr<Socket> socket) {
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::ErrorCloseCallback (Ptr<Socket> socket) {
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::RecvCallback(Ptr<Socket> socket) {
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AddPeer(int id, Address add) {

  mPeerAddress.insert(std::pair<int,const Address>(id, add));

  mPeerList.push_back(id);

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AddDirectPeer(int id) {
  directPeerList.push_back(id);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AddPeerMetric(int id, double metric) {
  peerMetric.insert(std::pair<int, double>(id, metric));
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::acceptPeer(int id, int src, const Address add) {
  
  AddPeer(id, add);
  AddDirectPeer(id);
  AddPeerMetric(id, 0); // Todo: update metric

  // connect socket
  Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress peer = InetSocketAddress(Ipv4Address::ConvertFrom(add), port);
  sock->Bind();
  sock->Connect(peer);
  switch (transferModel) {
  
  case PARALLEL:
    sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
    break;
  
  case SEQUENCIAL:
    sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
    break;
  
  default:
    break;
  }
  mPeerSockets.insert(std::pair<int,Ptr<Socket>>(id, sock));

  // update relay table
  if (relayType == ConsensusMessageBase::FLOOD || relayType == ConsensusMessageBase::MIXED || 
      relayType == ConsensusMessageBase::INFECT_UPON_CONTAGION) {
    
    if (linkEstPeerList.size() < max_outbound_number) {
      linkEstPeerList.push_back(id);
    }
  }
  
  // update relay table
  for (auto it  = relayTableLargePacket.begin(); it != relayTableLargePacket.end(); ++it) {
    if (it->first.src == src) {
      it->second.push_back(id);
    }
  }

  for (auto it  = relayTableSmallPacket.begin(); it != relayTableSmallPacket.end(); ++it) {
    if (it->first.src == src) {
      it->second.push_back(id);
    }
  }

  replicaStat = RUNNING;

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::joinPeer(int id, const Address add) {
  AddPeer(id, add);
  AddDirectPeer(id);
  AddPeerMetric(id, 0); // Todo: update metric

  // connect socket
  Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  InetSocketAddress peer = InetSocketAddress(Ipv4Address::ConvertFrom(add), port);
  sock->Bind();
  sock->Connect(peer);
  switch (transferModel) {
  
  case PARALLEL:
    sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
    break;
  
  case SEQUENCIAL:
    sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase<MessageType>::dateSentCallback, this));
    break;
  
  default:
    break;
  }
  mPeerSockets.insert(std::pair<int,Ptr<Socket>>(id, sock));

  replicaStat = RUNNING;
}


template <typename MessageType>
std::pair<RelayMap, RelayMap> BlockChainApplicationBase<MessageType>::disablePeer() {
  replicaStat = CRASH;
  StopApplication();
  return std::make_pair(relayTableLargePacket, relayTableSmallPacket);
}

template <typename MessageType>
void BlockChainApplicationBase<MessageType>::handoverPeer(std::pair<RelayMap, RelayMap> handover_map, int id, int source) {
  for (auto it = relayTableLargePacket.begin(); it != relayTableLargePacket.end(); ++it) {
    if (it->first.src == source) {
      bool relay = false;
      for (auto dst = it->second.begin(); dst != it->second.end(); ++dst) {
        if (*dst == id) {
          relay = true;
          it->second.erase(dst);
          break;
        }
      }
      if (relay) {
        for (auto handover = handover_map.first.begin(); handover != handover_map.first.end(); ++handover) {
          for (auto target: handover->second) {
            (*it).second.push_back(target);
          }
        }
      }
    }
  }

  for (auto it = relayTableSmallPacket.begin(); it != relayTableSmallPacket.end(); ++it) {
    if (it->first.src == source) {
      bool relay = false;
      for (auto dst = it->second.begin(); dst != it->second.end(); ++dst) {
        if (*dst == id) {
          relay = true;
          it->second.erase(dst);
          break;
        }
      }
      if (relay) {
        for (auto handover = handover_map.first.begin(); handover != handover_map.first.end(); ++handover) {
          for (auto target: handover->second) {
            (*it).second.push_back(target);
          }
        }
      }
    }
  } 
}



template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AddCorePeer(int id) {
  corePeerList.push_back(id);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::AddCorePeerMetric(int id, double distance) {
  corePeerMetric.push(std::pair<int, double>(id, distance));
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::BroadcastToPeers(Ptr<Packet> pkt, double delay) {
  void (BlockChainApplicationBase<MessageType>::*fp)(Ptr<Packet>) = &BlockChainApplicationBase<MessageType>::BroadcastToPeers;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::installShortestPathRoute(std::vector<int> route) {
  shortestPathRoute = std::move(route);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::BroadcastToPeers(Ptr<Packet> pkt, double delay, RelayEntry k) {
  void (BlockChainApplicationBase<MessageType>::*fp)(Ptr<Packet>, RelayEntry) = &BlockChainApplicationBase<MessageType>::BroadcastToPeers;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, k);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::BroadcastToPeers(Ptr<Packet> pkt, RelayEntry k) {
  
  // std::cout << "Overlay Broadcast" << std::endl;
  // std::cout << "at:"<<nodeId<<" source:"<<k.src<<" from:"<<k.from << std::endl;
  // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl;

  std::vector<int> relayList;

  // std::cout << "finding: " << k.src << ',' << k.from << std::endl;

  if (pkt->GetSerializedSize() > packetSizeThredhold) {

    // std::cout << "among: " << std::endl;
    // for (auto t: relayTableLargePacket) {
    //   std::cout << t.first.src << ',' << t.first.from << std::endl;
    // }

    auto relayListIter = relayTableLargePacket.find(k);
    if (relayListIter != relayTableLargePacket.end()) {
      relayList = relayListIter->second;
    }
  }

  else {

    // std::cout << "among: " << std::endl;
    // for (auto t: relayTableSmallPacket) {
    //   std::cout << t.first.src << ',' << t.first.from << std::endl;
    // }

    auto relayListIter = relayTableSmallPacket.find(k);
    if (relayListIter != relayTableSmallPacket.end()) {
      relayList = relayListIter->second;
    }
  }

  // std::cout << nodeId << " sending to: ";
  // for (auto dst: relayList) {
  //   std::cout << dst << " ";
  // }
  // std::cout << std::endl;

  switch (transferModel) {

  case PARALLEL:
    // order of relayList matters, do not change
    for (auto i : relayList) {

      // std::cout << "to:"<<i << std::endl;

      sendTo(pkt, i);
    }
    break;
  
  case SEQUENCIAL:

    sendInSequence(pkt, relayList);

    break;
  
  default:
    break;
  }

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::BroadcastToPeers(Ptr<Packet> pkt) {

  switch (transferModel) {

    case PARALLEL:

      for (auto sock : mPeerSockets) {

        // std::cout << "Send from "<<nodeId<<" to "<<sock.first << std::endl;
        // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl << std::endl;

        sendTo(pkt, sock.first);
      }
      break;
  
    case SEQUENCIAL:

      // std::cout << "Group Send from "<<nodeId << std::endl;
      // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl << std::endl;

      sendInSequence(pkt, mPeerList);
      break;
    
    default:
      break;

  }

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::relay(MessageType msg) {

  // std::cout << "relayMessage" << std::endl;
  // std::cout << "at: "<<nodeId<<" souce: "<<msg.getSrcAddr()<<" from: "<<msg.getFromAddr() << std::endl;
  // std::cout << "time: "<<Simulator::Now().GetSeconds()<< std::endl << std::endl;

  RelayEntry k(msg.getSrcAddr(), msg.getFromAddr());

  msg.setFromAddr(nodeId);

  // use node identifier instead of address just for coding simplicity
  // actually knowing ip-address is enough 

  Ptr<Packet> packet = msg.toPacket();
  BroadcastToPeers(packet, k);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::flood(MessageType msg) {
  uint8_t ttl = msg.getTTL();

  NS_ASSERT(ttl > 0);

  if (ttl != 0) ttl = ttl - 1;
  msg.setTTL(ttl);
  
  floodAnyway(std::move(msg));
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::flood(MessageType msg, double delay) {
   void (BlockChainApplicationBase<MessageType>::*fp)(MessageType) = &BlockChainApplicationBase<MessageType>::floodAnyway;
   delayedFlood = Simulator::Schedule(Seconds(delay), fp, this, std::move(msg));
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::floodAnyway(MessageType msg) {
    
  // std::cout << "floodMessage" << std::endl;
  // std::cout << "at:"<<nodeId<<" souce:"<<msg.getSrcAddr()<<" from:"<<msg.getFromAddr() << std::endl; 
  // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl; 

  msg.setFromAddr(nodeId);
  uint8_t ttl = msg.getTTL();
  if (ttl != 0) ttl = ttl - 1;
  msg.setTTL(ttl);
  
  Ptr<Packet> packet = msg.toPacket();

  int peerSize = linkEstPeerList.size();

  if (msg.getForwardN() >= peerSize) {
    sendToPeer(packet, linkEstPeerList);
  }
  else {

    std::vector<int> sortBox;

    for (int i = 0; i < peerSize; ++i) {
      sortBox.push_back(i);
    }

    if (floodRandomization) {
      // shuffle the box
      unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
      shuffle(sortBox.begin(), sortBox.end(), std::default_random_engine(seed));
    }

    std::vector<int> randomizedRecvList;
    for (int i = 0; i < msg.getForwardN(); ++i) {
      // directPeerList is pre-sorted
      // if the sortBox is not shuffle, nodes are chosen according to directPeerList
      // otherwise randomly
      randomizedRecvList.push_back(linkEstPeerList[sortBox[i]]);
      // std::cout << "to:"<<directPeerList[sortBox[i]] << std::endl;
    }
    // std::cout << std::endl;
    sendToPeer(packet, randomizedRecvList);
  }
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendToPeer(Ptr<Packet> pkt, std::vector<int> recv) {
  
  switch (transferModel) {

  case PARALLEL:
    for (auto r : recv) {
      sendTo(pkt, r);
    }
    break;
  
  case SEQUENCIAL:
    sendInSequence(pkt, recv);
    break;
  
  default:
    break;

  }
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendToPeer(Ptr<Packet> pkt, int recv) {
    
  switch (transferModel) {

  case PARALLEL:
    sendTo(pkt, recv);
    break;
  
  case SEQUENCIAL:
    sendInSequence(pkt, std::vector<int>(1, recv));
    break;
  
  default:
    break;

  }
}


/**
 * all send method should eventually chain call this method
 */ 
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendTo(Ptr<Packet> pkt, int i) {

  // if (i >= 50) {
  //   std::cout << "send to enabled node: " << i << std::endl;
  // }

  switch (replicaStat) {
  case RUNNING:
    
    // std::cout << "Send from " << nodeId << " to " << i << std::endl;
    // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl;

    while (mPeerSockets.find(i) == mPeerSockets.end()) {

      // no route
      NS_ASSERT(i != shortestPathRoute[i]);

      i = shortestPathRoute[i];
    }

    mPeerSockets.at(i)->Send(pkt);
    
    break;
  // just stop all outbound message to simulate a crashed node
  case CRASH:

    std::cout << "Fail to send from " << nodeId << " to " << i << std::endl;
    std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl;

  case FAULT:
  default:
    break;
  }
}


// add some artifacial delay to sendTo to simulate things like processing time alike
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendTo(Ptr<Packet> pkt, int i, double delay) {
  void (BlockChainApplicationBase<MessageType>::*fp)(Ptr<Packet>, int) = &BlockChainApplicationBase<MessageType>::sendTo;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, i);
}


// true if it is waiting for a timeout
template <typename MessageType>
bool BlockChainApplicationBase<MessageType>::checkEventStatus(EventId eid) {
  try {
    return !Simulator::IsExpired(eid);
  }
  catch(const std::exception& e) {
    return false;
  }
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::clearTimeoutEvent() {
  if (checkEventStatus(timeoutEvent)) {
    Simulator::Cancel(timeoutEvent);
  }
}


/*
It seems that ns3 tcp is sending packets immediately
Thus to simulate a topology that several outbound sockets are sharing a same outbound bandwidth limit,
we wait for (packet size / outbound bandwidth) after invoking Socket::Send on that packet
see BlockChainApplicationBase::sendNext() also
*/
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers) {

  NS_ASSERT(outboundBandwidth > 0); 
  
  // fixme: who is invoking this function with empty vector??
  if (!receivers.empty()) {

    SendQuest questList(pkt, receivers);

    sendquestbuffer.insert(questList);

    if (!isSending) {

      inactivePeriod += Simulator::Now().GetSeconds() - lastSendingStateTransferTime;
      lastSendingStateTransferTime = Simulator::Now().GetSeconds();

      isSending = true;

      sendNext();

    }

  }

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers, double delay) {
  void (BlockChainApplicationBase<MessageType>::*fp)(Ptr<Packet>, std::vector<int>) = &BlockChainApplicationBase<MessageType>::sendInSequence;
  delayGroupSendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, receivers);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sendNext() {
  if (!sendquestbuffer.empty()) {
    auto task = sendquestbuffer.getNext();

    auto pkt = task.first;
    auto receiver = task.second;

    double finishTime = (double) pkt->GetSize() / outboundBandwidth;

    sendTo(pkt, receiver);

    void (BlockChainApplicationBase<MessageType>::*fp)() = &BlockChainApplicationBase<MessageType>::sendNext;

    Simulator::Schedule(Seconds(finishTime), fp, this);
  }
  else {
    // finished all sending quests
    activePeriod += Simulator::Now().GetSeconds() - lastSendingStateTransferTime;
    lastSendingStateTransferTime = Simulator::Now().GetSeconds();
    
    isSending = false;
  }
}


// callback notified when data sent by udp/tcp
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::dateSentCallback(Ptr<Socket> sock, uint32_t sent) {

  nodeStress += sent;

  ns3::Address address;
  sock->GetPeerName(address);

  /**
   * GetPeerName set a strange Address of type 4, length 7
   * consist of 4 byte ipv4 address and 3 byte unknown padding
   * so we work around to compare equality
   */

  uint8_t buffer[7];
  uint8_t convertedAddress[4];
  address.CopyTo(buffer);
  std::memcpy(convertedAddress, buffer, 4);

  int peerId;

  for (auto c : mPeerAddress) {

    uint8_t convertedC[4];
    c.second.CopyTo(convertedC);

    // if equal
    if (memcmp(convertedC, convertedAddress, 4) == 0) {
      peerId = c.first;
    }
  }

  auto link = std::make_pair(nodeId, peerId);
  linkStress[link] += sent;

}


/**
 * Process messages received and parsed correctly as a consensus message
 * Check their header fields and pass to conrespond processing functions
 */
template <typename MessageType>
void BlockChainApplicationBase<MessageType>::onMessageCallback(MessageType msg) {

  // std::cout << "Receive" << std::endl;
  // std::cout << "at: "<<nodeId<<" from: "<<msg.getFromAddr() << std::endl;


  // First check if is supposed to do some appliaction-layer forwarding stuff   
  // pass a copy
   
  // parse message type
  // use message pool to store servely out-of-order messages to
  // avoid a lot of re-transmition; filter out dulicates and detect 
  // inconsistances 

   uint8_t count;
   bool conflict;
   bool hasfull;

  if (pooledMsg) {
    std::tie(count, conflict, hasfull) = messageRecvPool.insertWithDetect(msg);
  }

  switch (msg.getBlockType()) {
  
  case ConsensusMessageBase::NORMAL_BLOCK:

    if (pooledMsg) {
      applicationLayerRelay(msg, count);
      if (count <= 1) { // only process once
        parseMessage(std::move(msg));
      }
    }
    else {
      applicationLayerRelay(msg);
      parseMessage(std::move(msg)); 
    }
  break;

  // todo not tested yet

  case ConsensusMessageBase::COMPACT_HEAD:

    if (pooledMsg) { 
      if (!hasfull) {
        msg.setBlockType(ConsensusMessageBase::REQUIRE);
        Ptr<Packet> packet = msg.toPacket();
        std::set<uint32_t> source = messageRecvPool.getSource(msg);
        if (!source.empty()) {
          
          // TODO: choose source
          
          sendToPeer(packet, int(*source.begin()));
        }
      }
      else {
        applicationLayerRelay(msg, count);
        messageRecvPool.getFullMessage(msg);
        parseMessage(std::move(msg));
      }
    }
    break;
  
  case ConsensusMessageBase::REQUIRE:
    if (pooledMsg) {
      if (hasfull) {
        messageRecvPool.getFullMessage(msg);
        Ptr<Packet> packet = msg.toPacket();
        sendToPeer(packet, int(msg.getSrcAddr()));
      }
    }
    break;
  
  default:
    applicationLayerRelay(msg);
    std::cerr << "Bad block type" << std::endl;
    break;
  }
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::clearDelaySendEvent() {

  if (checkEventStatus(delaySendEvent)) {
    Simulator::Cancel(delaySendEvent);
  }

  if (checkEventStatus(delayGroupSendEvent)) {
    Simulator::Cancel(delayGroupSendEvent);
  }

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::insertRelayLargePacket(int src, int from, int to) {
  
  RelayEntry k(src, from);

  if (relayTableLargePacket.find(k) == relayTableLargePacket.end()) {
    relayTableLargePacket.insert(std::make_pair(k, std::vector<int>())); 
  }

  // value of tableInUse is vector<int> type
  relayTableLargePacket[k].push_back(to);

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::insertRelaySmallPacket(int src, int from, int to) {
  
  RelayEntry k(src, from);

  if (relayTableSmallPacket.find(k) == relayTableSmallPacket.end()) {
    relayTableSmallPacket.insert(std::make_pair(k, std::vector<int>())); 
  }

  // value of tableInUse is vector<int> type
  relayTableSmallPacket[k].push_back(to);

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::changeStatus(uint8_t type) {
  replicaStat = type;
}


template <typename MessageType>
bool BlockChainApplicationBase<MessageType>::compareWRTMetric(const int &a, const int &b) {
  return peerMetric.at(a) < peerMetric.at(b);
}


template <typename MessageType>
bool BlockChainApplicationBase<MessageType>::compareWRTAccumlatedMetric(const int &a, const int &b) {
  return corePeerList.at(a) < corePeerList.at(b);
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sortPeer() {

  std::sort(directPeerList.begin(), directPeerList.end(), 
    [this](int a, int b){return compareWRTMetric(a, b);});

}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::sortCoreList() {
  std::sort(corePeerList.begin(), corePeerList.end(), 
    [this](int a, int b){return compareWRTAccumlatedMetric(a, b);});
}


template <typename MessageType>
bool BlockChainApplicationBase<MessageType>::isCoreNode() {
  for (auto node : corePeerList) {
    if ((int) nodeId == node) {
      return true;
    }
  }
  return false;
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::applicationLayerRelay(MessageType msg, int duplicates) {
  switch(msg.getTransportType()) {

    case ConsensusMessageBase::DIRECT:
      if (msg.getDstAddr() != nodeId) {
        sendToPeer(msg.toPacket(), msg.getDstAddr());
      }
      break;
    
    case ConsensusMessageBase::CORE_RELAY:
      if (msg.getTTL() > 0) {
        // random flood phase 
        flood(msg);
      }
      else {
        // pass to core node to broadcast
        if (nodeId == msg.getDstAddr()) {      
          msg.setSrcAddr(nodeId);
          msg.setFromAddr(nodeId);
          msg.setDstAddr(std::numeric_limits<uint32_t>::infinity());
          msg.setTransportType(ConsensusMessageBase::RELAY);

          relay(msg);
        }
        else {
          sendToPeer(msg.toPacket(), msg.getDstAddr());
        }
      }
      break;

    case ConsensusMessageBase::MIXED:
      if (msg.getTTL() > 0) {
        flood(msg);
      }
      else {

        // start broadcast here
        msg.setSrcAddr(nodeId);
        msg.setFromAddr(nodeId);
        msg.setTransportType(ConsensusMessageBase::RELAY);

        relay(msg);
      }
      break;

    case ConsensusMessageBase::RELAY:
      relay(msg);
      break;
    
    case ConsensusMessageBase::INFECT_UPON_CONTAGION:

      if (pooledMsg) {
        if (messageRecvPool.hasMessage(msg) == 0) {
          // first receive
          uint8_t ttl = msg.getTTL();
          if (ttl > 1) {
            msg.setTTL(ttl - 1);
          }
        }
        if (msg.getTTL() > 0) {
          floodAnyway(msg);
        }
        break;
      }
    // INTENDED FALL THROUGH

    case ConsensusMessageBase::FLOOD:
      if (duplicates <= 1) floodAnyway(msg);
      break;
    
    default:
      std::cerr << "Bad message transfer type" << std::endl;
      break;

  }
}


// get the ratio of time this application is sending something from start to stop, till now if it is still running
template <typename MessageType>
double BlockChainApplicationBase<MessageType>::getActiveRate() {
  double unSettled = Simulator::Now().GetSeconds() - lastSendingStateTransferTime;
  if (isSending) activePeriod += unSettled;
  else inactivePeriod += unSettled;
  lastSendingStateTransferTime = Simulator::Now().GetSeconds();
  return activePeriod / (activePeriod + inactivePeriod + 2e-45);
}


// get bytes passed by this application per second from start till now
template <typename MessageType>
double BlockChainApplicationBase<MessageType>::getNodeStress() {
  double period = (lastStopTime > lastStartTime ? lastStopTime : Simulator::Now().GetSeconds()) - lastStartTime;
  return (double) nodeStress / (period + 2e-45);
}


template <typename MessageType>
std::map<std::pair<u_int32_t, u_int32_t>, double> 
BlockChainApplicationBase<MessageType>::getLinkStress() {
  double period = (lastStopTime > lastStartTime ? lastStopTime : Simulator::Now().GetSeconds()) - lastStartTime;
  for (auto &c : linkStress) {
    c.second /= period;
  }
  return linkStress;
}


template <typename MessageType>
void BlockChainApplicationBase<MessageType>::printState() {
  for (auto relay: relayTableLargePacket) {
    std::cout << "relay large at " << nodeId << " :" << relay.first.from << " " << relay.first.src
      << " to: ";
    for (auto dst: relay.second) std::cout << " " << dst;
    std::cout << std::endl;
  }
  for (auto relay: relayTableSmallPacket) {
    std::cout << "relay small at " << nodeId << " :" << relay.first.from << " " << relay.first.src
      << " to: ";
    for (auto dst: relay.second) std::cout << " " << dst;
    std::cout << std::endl;
  }
}


}

#endif