#include "BlockChainApplicationBase.h"
#include <iostream>
#include <algorithm>

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (BlockChainApplicationBase);

NS_LOG_COMPONENT_DEFINE("BlockChainApplicationBase");


// implementation of class RelayEntry

RelayEntry::RelayEntry(int s, int f) : src(s), from(f) {}


RelayEntry::RelayEntry() : src(0), from(0) {}


bool RelayEntry::operator<(const RelayEntry &that) const{
  if (src != that.src) return src < that.src;
  else return from < that.from;
}


// implementation of class SendQuestBuffer

SendQuestBuffer::SendQuestBuffer() : currentIndex(0) {}


bool SendQuestBuffer::empty() {

  return sendBuffer.empty() && currentIndex == (int) currentQuest.mReceivers.size();

}


std::pair<Ptr<Packet>, int> SendQuestBuffer::getNext() {

  //Todo: will tree-wise scheluding help? 

  if (currentIndex < (int) currentQuest.mReceivers.size()) {
    return std::pair<Ptr<Packet>, int>(
      currentQuest.mPkt,
      currentQuest.mReceivers[currentIndex++]
    );
  }
  else {
    currentIndex = 0;

    NS_ASSERT(!sendBuffer.empty());

    currentQuest = sendBuffer.front();
    sendBuffer.pop_front();

    return std::pair<Ptr<Packet>, int>(
      currentQuest.mPkt,
      currentQuest.mReceivers[currentIndex++]
    );
  }
}


void SendQuestBuffer::insert(SendQuest quest) {
  sendBuffer.push_back(quest);
}


// implementation of class BlockChainApplicationBase

void BlockChainApplicationBase::DoDispose (void) {
  Application::DoDispose ();
}


TypeId BlockChainApplicationBase::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::BlockChainApplicationBase")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<BlockChainApplicationBase> ()
    .AddTraceSource ("Rx",
                     "A packet has been received", 
                     MakeTraceSourceAccessor (&BlockChainApplicationBase::mRxTrace),
                     "ns3::Packet::AddressTracedCallback");
  return tid;
}


BlockChainApplicationBase::BlockChainApplicationBase(void) {

  timeout = 1;
  corePeerMetric = peerMetricPrioQueue([](const peerMetricEntry &a, const peerMetricEntry &b)->bool {
      return a.second > b.second; 
    });

  mPeerSockets.clear();
  mPeerAddress.clear();
  directPeerMetric.clear();
  relayTableLargePacket.clear();
  relayTableSmallPacket.clear();

}


BlockChainApplicationBase::~BlockChainApplicationBase(void) {}


void BlockChainApplicationBase::StartApplication() {

  if (!mListeningSocket) {
    mListeningSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    mListeningSocket->Bind(local);
  }

  if (relayType == BlockChainApplicationBase::FLOOD || relayType == BlockChainApplicationBase::MIXED) {
    if (!floodRandomization) {
      sortPeer();
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
      break;
    
    case SEQUENCIAL:
      sock->SetDataSentCallback(MakeCallback(&BlockChainApplicationBase::dateSentCallback, this));
      break;
    
    default:
      break;
    }
    mPeerSockets.insert(std::pair<int,Ptr<Socket>>(address.first, sock));
  }

  // ready
  replicaStat = RUNNING;

}


void BlockChainApplicationBase::StopApplication() {
  if (mListeningSocket) {
    mListeningSocket->Close();
  }

  for (auto peerSocket : mPeerSockets) {
    (peerSocket.second)->Close();
  }

  clearDelaySendEvent();
}


void BlockChainApplicationBase::AcceptCallback(Ptr<Socket> socket, const Address& from) {
  socket->SetRecvCallback(MakeCallback(&BlockChainApplicationBase::RecvCallback, this));
}


void BlockChainApplicationBase::NormalCloseCallback (Ptr<Socket> socket) {
}


void BlockChainApplicationBase::ErrorCloseCallback (Ptr<Socket> socket) {
}


void BlockChainApplicationBase::RecvCallback(Ptr<Socket> socket) {
}


void BlockChainApplicationBase::AddPeer(int id, Address add) {

  mPeerAddress.insert(std::pair<int,const Address>(id, add));

  mPeerList.push_back(id);

}


void BlockChainApplicationBase::AddDirectPeer(int id) {
  directPeerList.push_back(id);
}


void BlockChainApplicationBase::AddPeerMetric(int id, double metric) {
  directPeerMetric.insert(std::pair<int, double>(id, metric));
}


void BlockChainApplicationBase::AddCorePeer(int id) {
  corePeerList.push_back(id);
}


void BlockChainApplicationBase::AddCorePeerMetric(int id, double distance) {
  corePeerMetric.push(std::pair<int, double>(id, distance));
}


void BlockChainApplicationBase::BroadcastToPeers(Ptr<Packet> pkt, double delay) {
  void (BlockChainApplicationBase::*fp)(Ptr<Packet>) = &BlockChainApplicationBase::BroadcastToPeers;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt);
}


void BlockChainApplicationBase::installShortestPathRoute(std::vector<int> route) {
  shortestPathRoute = std::move(route);
}


void BlockChainApplicationBase::BroadcastToPeers(Ptr<Packet> pkt, double delay, RelayEntry k) {
  void (BlockChainApplicationBase::*fp)(Ptr<Packet>, RelayEntry) = &BlockChainApplicationBase::BroadcastToPeers;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, k);
}


void BlockChainApplicationBase::BroadcastToPeers(Ptr<Packet> pkt, RelayEntry k) {
  
  NS_LOG_INFO("Overlay Broadcast");
  NS_LOG_INFO("at:"<<nodeId<<" source:"<<k.src<<" from:"<<k.from);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());

  // std::cout << "Overlay Broadcast" << std::endl;
  // std::cout << "at:"<<nodeId<<" source:"<<k.src<<" from:"<<k.from << std::endl;
  // std::cout << "time:"<<Simulator::Now().GetSeconds() << std::endl;

  std::vector<int> relayList;

  if (pkt->GetSerializedSize() > packetSizeThredhold) {

    auto relayListIter = relayTableLargePacket.find(k);
    if (relayListIter != relayTableLargePacket.end()) {
      relayList = relayListIter->second;
    }

  }

  else {

    auto relayListIter = relayTableSmallPacket.find(k);
    if (relayListIter != relayTableSmallPacket.end()) {
      relayList = relayListIter->second;
    }

  }

  
  switch (transferModel) {

  case PARALLEL:
    // order of relayList matters, do not change
    for (auto i : relayList) {

      NS_LOG_INFO("to:"<<i);

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

  NS_LOG_INFO("");
}


void BlockChainApplicationBase::BroadcastToPeers(Ptr<Packet> pkt) {

  switch (transferModel) {

    case PARALLEL:

      for (auto sock : mPeerSockets) {

        NS_LOG_INFO("Send from "<<nodeId<<" to "<<sock.first);
        NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
        NS_LOG_INFO("");

        sendTo(pkt, sock.first);
      }
      break;
  
    case SEQUENCIAL:

      NS_LOG_INFO("Group Send from "<<nodeId);
      NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
      NS_LOG_INFO("");

      sendInSequence(pkt, mPeerList);
      break;
    
    default:
      break;

  }

}


void BlockChainApplicationBase::sendToPeer(Ptr<Packet> pkt, std::vector<int> recv) {
  
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


void BlockChainApplicationBase::sendToPeer(Ptr<Packet> pkt, int recv) {
    
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
void BlockChainApplicationBase::sendTo(Ptr<Packet> pkt, int i) {
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
  case FAULT:
  default:
    break;
  }
}


void BlockChainApplicationBase::sendTo(Ptr<Packet> pkt, int i, double delay) {
  void (BlockChainApplicationBase::*fp)(Ptr<Packet>, int) = &BlockChainApplicationBase::sendTo;
  delaySendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, i);
}


// true if it is waiting for a timeout
bool BlockChainApplicationBase::checkEventStatus(EventId eid) {
  try {
    return !Simulator::IsExpired(eid);
  }
  catch(const std::exception& e) {
    return false;
  }
}


void BlockChainApplicationBase::sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers) {
  
  if (!receivers.empty()) {
    SendQuest questList(pkt, receivers);

    sendquestbuffer.insert(questList);

    if (!isSending) {
      isSending = true;
      std::pair<Ptr<Packet>, int > quest = sendquestbuffer.getNext();
      sendTo(quest.first, quest.second);
    }

  }

}


void BlockChainApplicationBase::sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers, double delay) {
//todo
  void (BlockChainApplicationBase::*fp)(Ptr<Packet>, std::vector<int>) = &BlockChainApplicationBase::sendInSequence;
  delayGroupSendEvent = Simulator::Schedule(Seconds(delay), fp, this, pkt, receivers);
}


void BlockChainApplicationBase::dateSentCallback(Ptr<Socket> sock, uint32_t sent) {
  //todo
  if (!sendquestbuffer.empty()) {
    std::pair<Ptr<Packet>, int > quest = sendquestbuffer.getNext();
    sendTo(quest.first, quest.second);
  }
  else {
    isSending = false;
  }
}


void BlockChainApplicationBase::clearDelaySendEvent() {

  if (checkEventStatus(delaySendEvent)) {
    Simulator::Cancel(delaySendEvent);
  }

  if (checkEventStatus(delayGroupSendEvent)) {
    Simulator::Cancel(delayGroupSendEvent);
  }

}


int BlockChainApplicationBase::getNodeId() {
  return nodeId;
}


void BlockChainApplicationBase::setNodeId(int id) {
  nodeId = id;
}


void BlockChainApplicationBase::setDelay(double d) {
  mDelay = d;
}


void BlockChainApplicationBase::setTimeout(double t) {
  timeout = t;
}


void BlockChainApplicationBase::setRelayType(int t) {
  relayType = t;
}


void BlockChainApplicationBase::setTransferModel(int t) {
  transferModel = t;
}


void BlockChainApplicationBase::insertRelayLargePacket(int src, int from, int to) {
  
  RelayEntry k(src, from);

  if (relayTableLargePacket.find(k) == relayTableLargePacket.end()) {
    relayTableLargePacket.insert(std::make_pair(k, std::vector<int>())); 
  }

  // value of tableInUse is vector<int> type
  relayTableLargePacket[k].push_back(to);

}


void BlockChainApplicationBase::insertRelaySmallPacket(int src, int from, int to) {
  
  RelayEntry k(src, from);

  if (relayTableSmallPacket.find(k) == relayTableSmallPacket.end()) {
    relayTableSmallPacket.insert(std::make_pair(k, std::vector<int>())); 
  }

  // value of tableInUse is vector<int> type
  relayTableSmallPacket[k].push_back(to);

}



void BlockChainApplicationBase::changeStatus(uint8_t type) {
  replicaStat = type;
}


bool BlockChainApplicationBase::compareWRTMetric(const int &a, const int &b) {
  return directPeerMetric.at(a) < directPeerMetric.at(b);
}


bool BlockChainApplicationBase::compareWRTAccumlatedMetric(const int &a, const int &b) {
  return corePeerList.at(a) < corePeerList.at(b);
}


void BlockChainApplicationBase::sortPeer() {

  std::sort(directPeerList.begin(), directPeerList.end(), 
    [this](int a, int b){return compareWRTMetric(a, b);});

}


void BlockChainApplicationBase::sortCoreList() {

  std::sort(corePeerList.begin(), corePeerList.end(), 
    [this](int a, int b){return compareWRTAccumlatedMetric(a, b);});

}

bool BlockChainApplicationBase::isCoreNode() {
  for (auto node : corePeerList) {
    if ((int) nodeId == node) {
      return true;
    }
  }
  return false;
}

void BlockChainApplicationBase::setDefaultTTL(int ttl) {
  defaultTTL = ttl;
}


void BlockChainApplicationBase::setDefaultFloodN(int n) {
  defaultFloodN = n;
}


void BlockChainApplicationBase::setFloodRandomization(bool b) {
  floodRandomization = b;
}

} // namespace ns3