#ifndef BLOCKCHAINAPPLICATIONBASE_H
#define BLOCKCHAINAPPLICATIONBASE_H

#include <vector>
#include <set>
#include <map>
#include <list>
#include <queue>
#include <functional>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/application.h"



namespace ns3 {

class Socket;
class Address;

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
class BlockChainApplicationBase : public Application {

public:

  enum NET_TRANS_TYPE : uint32_t {
    DIRECT,
    RELAY,
    FLOOD,
    MIXED,
    CORE_RELAY,
    INFECT_UPON_CONTAGION
  };

  enum REPLICA_STAT : uint8_t {
    RUNNING,
    CRASH,
    FAULT
  };

  enum RELAY_TYPE : uint8_t {
    SEQUENCIAL,
    PARALLEL
  };

 
  static TypeId GetTypeId (void);

  BlockChainApplicationBase(void);
  virtual ~BlockChainApplicationBase (void);

  void AddPeer(int id, const Address add);

  void AddDirectPeer(int id);
  void AddPeerMetric(int id, double metric);

  void insertRelayLargePacket(int src, int from, int to);
  void insertRelaySmallPacket(int src, int from, int to);

  void AddCorePeer(int id);
  void AddCorePeerMetric(int id, double distance);

  void changeStatus(uint8_t type);

  void dateSentCallback(Ptr<Socket> sock, uint32_t sent);

  void sendToPeer(Ptr<Packet> pkt, int recv);
  void sendToPeer(Ptr<Packet> pkt, std::vector<int> recv);

  void BroadcastToPeers(Ptr<Packet> pkt);
  void BroadcastToPeers(Ptr<Packet> pkt, RelayEntry k);
  void BroadcastToPeers(Ptr<Packet> pkt, double delay);
  void BroadcastToPeers(Ptr<Packet> pkt, double delay, RelayEntry k);

  int getNodeId();

  void setNodeId(int id);
  void setDelay(double d);
  void setTimeout(double t);
  void setRelayType(int t);
  void setTransferModel(int t);

  void setDefaultTTL(int ttl);
  void setDefaultFloodN(int n);

  void setFloodRandomization(bool b);

  void installShortestPathRoute(std::vector<int> route);

  void clearDelaySendEvent();

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

  std::map<int, double> directPeerMetric;

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

  double mDelay;
 
  // TODO: do some tracing
  TracedCallback<Ptr<const Packet>> mRxTrace;

  int relayType;

  uint32_t packetSizeThredhold = 500;  // bytes

  /*
  * the application-layer relay table
  * get next hop route from relay entry
  * next hop may be mutiple nodes
  */
  std::map<RelayEntry, std::vector<int> > relayTableLargePacket;
  std::map<RelayEntry, std::vector<int> > relayTableSmallPacket;

  std::vector<int> shortestPathRoute;

  int defaultTTL;
  int defaultFloodN;

  bool floodRandomization;

  uint32_t seq = 0;

  uint8_t replicaStat;

  int transferModel = PARALLEL;

  SendQuestBuffer sendquestbuffer;

  bool isSending = false;

  virtual void DoDispose (void);
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  template<class MessageType>
  bool validateMessage(MessageType& msg);

  void RecvCallback (Ptr<Socket> socket);
  void AcceptCallback (Ptr<Socket> socket, const Address& from);
  void NormalCloseCallback (Ptr<Socket> socket);
  void ErrorCloseCallback (Ptr<Socket> socket);

  bool checkEventStatus(EventId eid);

  // if transport in flood or mixed mode and no randomization, rank peers and choose better ones
  void sortPeer();

  void sortCoreList();

  bool isCoreNode();


private:

  void sendTo(Ptr<Packet> pkt, int i);
  void sendTo(Ptr<Packet> pkt, int i, double delay);

  void sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers);
  void sendInSequence(Ptr<Packet> pkt, std::vector<int> receivers, double delay);
  
  bool compareWRTMetric(const int &a, const int &b);

  bool compareWRTAccumlatedMetric(const int &a, const int &b);

};

}

#endif