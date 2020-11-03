#include "PBFTCorrect.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include <iostream>
#include <sstream>
#include <string>


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(PBFTCorrect);

NS_LOG_COMPONENT_DEFINE("PBFTCorrect");


TypeId PBFTCorrect::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::PBFTCorrect")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<PBFTCorrect> ()
    .AddTraceSource("Rx",
                    "A packet has been received", 
                    MakeTraceSourceAccessor(&PBFTCorrect::mRxTrace),
                    "ns3::Packet::TracedCallback");
  return tid;
}


void PBFTCorrect::DoDispose (void) {
  BlockChainApplicationBase::DoDispose ();
}


PBFTCorrect::PBFTCorrect(void) {
  stage = IDLE;
  round = 0;
}


PBFTCorrect::~PBFTCorrect(void) {}


void PBFTCorrect::StartApplication() {

  // chain up with superclass setups
  BlockChainApplicationBase::StartApplication();

  // setup listening socket
  mListeningSocket->Listen();
  mListeningSocket->ShutdownSend();
  
  mListeningSocket->SetRecvCallback(MakeCallback(&PBFTCorrect::RecvCallback, this));
  mListeningSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                                      MakeCallback(&PBFTCorrect::AcceptCallback, this));
  mListeningSocket->SetCloseCallbacks(MakeCallback(&PBFTCorrect::NormalCloseCallback, this),
                                      MakeCallback(&PBFTCorrect::ErrorCloseCallback, this));

}


void PBFTCorrect::StopApplication() {

  BlockChainApplicationBase::StopApplication();

  /**
   * cancel all scheduled events for this app
   * ns3 simulation will not end if there is a scheduled event
   * even after hitting the end time
   */
  clearDelaySendEvent();
  clearScheduledEvent();
  clearTimeoutEvent();

}


/**
 * Process messages received and parsed correctly as a consensus message
 * Check their header fields and pass to conrespond processing functions
 * 
 * TODO: decouple this part as a standalone module so that it can be reused
 * to evalue network design with other consensus protocols  
 *
 */
void PBFTCorrect::onMessageCallback(PBFTMessage msg) {

  NS_LOG_INFO("Receive");
  NS_LOG_INFO("at: "<<nodeId<<" from: "<<msg.getFromAddr());

  /**
   * First check if is supposed to do some appliaction-layer forwarding stuff
   * 
   * pass a copy
   */
  applicationLayerRelay(msg);


  /**
   * parse message type
   * use message pool to store servely out-of-order messages to
   * avoid a lot of re-transmition; filter out dulicates and detect 
   * inconsistances 
   */

   uint8_t count;
   bool conflict;
   bool hasfull;

  if (pooledMsg) {
    std::tie(count, conflict, hasfull) = messageRecvPool.insertWithDetect(msg);
  }

  switch (msg.getBlockType()) {
  
  case ConsensusMessageBase::NORMAL_BLOCK:

    parseMessage(std::move(msg));
    break;
  
  case ConsensusMessageBase::COMPACT_HEAD:

    if (pooledMsg) { 
      if (!hasfull) {
        msg.setBlockType(ConsensusMessageBase::REQUIRE);
        Ptr<Packet> packet = msg.toPacket();
        std::set<uint32_t> source = messageRecvPool.getSource(msg);
        if (!source.empty()) {
          /**
          * TODO: choose source
          */
          sendToPeer(packet, int(*source.begin()));
        }
      }
      else {
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
    std::cerr << "Bad block type" << std::endl;
    break;
  }
}


void PBFTCorrect::applicationLayerRelay(PBFTMessage msg) {

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
        if (isCoreNode()) {      
          msg.setSrcAddr(nodeId);
          msg.setFromAddr(nodeId);
          msg.setTransportType(ConsensusMessageBase::RELAY);

          relay(msg);
        }
        else {
          SendToRoot(msg);
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
      flood(msg);
      break;
    
    default:
      std::cerr << "Bad message transfer type" << std::endl;
      break;

  }
}


void PBFTCorrect::parseMessage(PBFTMessage msg) {

  // if this message is sent to me or to all
  if (msg.getDstAddr() == nodeId 
    || msg.getDstAddr() == std::numeric_limits<uint32_t>::infinity()) {

    NS_LOG_INFO("Parse Message");

    switch (msg.getType()) {
      case REQUEST:
        onRequest(std::move(msg));
        break;
      case PRE_PREPARE:
        onPreprepare(std::move(msg));
        break;
      case PREPARE:
        onPrepare(std::move(msg));
        break;
      case COMMIT:
        onCommit(std::move(msg));
        break;
      case BLAME:
        onBlame(std::move(msg));
        break;
      case REPLY:
        onReply(std::move(msg));
        break;
      case NEWEPOCH:
        onNewEpoch(std::move(msg));
        break;
      case CONFIRM_NEWEPOCH:
        onConfirmEpoch(std::move(msg));
        break;
      default:
        std::cerr << "Bad type" << std::endl;
        break;
    }

  }


}


// Todo: fault detect and recovery from timeout
void PBFTCorrect::onTimeoutCallback() {

  if (!replicaStat == RUNNING) return;

  NS_LOG_INFO("timeout");
  NS_LOG_INFO("at:"<<nodeId<<" primary:"<<primaryId);
  NS_LOG_INFO("round:"<<round);
  NS_LOG_INFO("stage:"<<stage);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  switch (stage)
  {
  case REQUEST:
    onRequestTimeout();
    break;
  case PRE_PREPARE:
    onPreprepareTimeout();
    break;
  case PREPARE:
    onPrepareTimeout();
    break;
  case IDLE:
  case COMMIT:
  case BLAME:
  case REPLY:
  case NEWEPOCH:
  case CONFIRM_NEWEPOCH:
  // INTENDED
  default:
    discardRound();
    break;
  }
}


void PBFTCorrect::onRequestTimeout() {
  //request new round
  sendBlame();
}


void PBFTCorrect::onPreprepareTimeout() {
  sendBlame();
}


void PBFTCorrect::onPrepareTimeout() {
  sendBlame();
}


void PBFTCorrect::RecvCallback (Ptr<Socket> sock) {

  if (!replicaStat == RUNNING) return;

  Ptr<Packet> packet = sock->Recv();

  mRxTrace(packet);

  int payloadSize = packet->GetSize();
  uint8_t *buffer = new uint8_t[payloadSize];
  packet->CopyData(buffer, payloadSize);
  try {
    PBFTMessage msg = message(blockSize);
    msg.deserialization(payloadSize, buffer);

    if (validateMessage(msg)) {
      onMessageCallback(std::move(msg));
    }
  }
  catch(const std::exception& e) {
    std::cerr << "parser message failed" << std::endl;
    return;
  }
}


void PBFTCorrect::onRequest(PBFTMessage msg) {

  NS_LOG_INFO("onRequest");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  // for simplicity, only response in IDLE now
  // just discard requests in other states
  // Todo : queue and pace requests
  if (nodeId == primaryId && stage == IDLE) {

    // reuse the object, save payload before rest
    // note that there exist a copy in recv pool
    msg.reset(messageConstantLen);

    msg.setType(PRE_PREPARE);
    msg.setSignerId(nodeId);
    msg.setRound(round);

    BroadcastPBFT(std::move(msg));

    stage = PRE_PREPARE;
    setTimeoutEvent();
  }
}


void PBFTCorrect::onPreprepare(PBFTMessage msg) {

  NS_LOG_INFO("onPreprepare");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  if (msg.getSignerId() == primaryId) {
    if (msg.getRound() == round) {
      if (stage == IDLE || stage == REQUEST) {
        // everything is right

        if (firstPreprepare) {
          firstPreprepare = false;
          preprepareTime = Simulator::Now().GetSeconds();
        }

        // reuse the object, save payload before rest
        // note that there exist a copy in recv pool
        msg.reset(blockSize);

        msg.setType(PREPARE);
        msg.setSignerId(nodeId);
        msg.setRound(round);

        double timeE = Simulator::Now().GetSeconds() - preprepareTime;

        if (timeE >= mDelay) {
          BroadcastPBFT(std::move(msg));
        }
        else {
          BroadcastPBFT(std::move(msg), mDelay - timeE);
        }

        stage = PRE_PREPARE;
        setTimeoutEvent();
        
        // in case message received is quite out-of-order
        prepared();
      }
      else {
        // received a duplicate preprepare
        // ignore it
      }
    }
    else if (msg.getRound() > round) {
      // perhaps lag back
      // believe the primary

      // give up states of last round
      discardRound();

      // if control reaches this branch, it shall always be the first time to hear a prepare
      firstPreprepare = false;
      preprepareTime = Simulator::Now().GetSeconds();
    
      round = msg.getRound();
      //Todo: retrieve missing rounds

      msg.reset(blockSize);
      msg.setType(PREPARE);
      msg.setSignerId(nodeId);
      msg.setRound(round);

      double timeE = Simulator::Now().GetSeconds() - preprepareTime;

      if (timeE >= mDelay) {
        BroadcastPBFT(std::move(msg));
      }
      else {
        BroadcastPBFT(std::move(msg), mDelay - timeE);
      }

      stage = PRE_PREPARE;
      setTimeoutEvent();

      // in case message received is quite out-of-order
      prepared();

    }
    else {
      // msg round < node round
      // conflict
      // blame leader with this preprepare and previous quorum cert
      if (isBackupPrimary()) {
        sendNewEpoch();
      }
      else {
        sendBlame();
      }
    }

  }
}


void PBFTCorrect::onPrepare(PBFTMessage msg) {

  NS_LOG_INFO("onPrepare");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");


  if (msg.getRound() == round) {

    if (firstPrepare) {
      firstPrepare = false;
      prepareTime = Simulator::Now().GetSeconds();
    }

    if (incPrepareCount(msg.getSignerId()) >= quorum && stage == PRE_PREPARE) {
      prepared(true);
    }
    // this is buggy, because a node must check the invariant that
    // a leader will never proposed a lower round  
    // so if a node missed the proposal, it should try retrieve it from somewhere or
    // just abandon that round and wait for a higher round
    //
    // else if (incPrepareCount(msg.getSignerId()) >= quorum + 1 && stage == IDLE) {
    //  prepared(true);
    //}  
  }
}


void PBFTCorrect::prepared(bool shortcut) {
  if (shortcut || ((int) prepareCount.size() >= quorum && stage == PRE_PREPARE) ) {

    PBFTMessage rmsg = message(messageConstantLen);
    rmsg.setType(COMMIT);
    rmsg.setSignerId(nodeId);
    rmsg.setRound(round);

    double timeE = Simulator::Now().GetSeconds() - prepareTime;

    if (timeE >= mDelay) {
      BroadcastPBFT(std::move(rmsg));
    }
    else {
      BroadcastPBFT(std::move(rmsg), mDelay - timeE);
    }

    stage = PREPARE;
    setTimeoutEvent();

    // in case message received is quite out-of-order
    committed();
  }
}


void PBFTCorrect::onCommit(PBFTMessage msg) {

  NS_LOG_INFO("onCommit");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");


  if (msg.getRound() == round) {
    if (incCommitCount(msg.getSignerId()) >= quorum && stage == PREPARE) {
      committed();
    }
  }
}


void PBFTCorrect::committed() {
  if ( (int) commitCount.size() >= quorum && stage == PREPARE) {
    stage = COMMIT;

    clearTimeoutEvent();
  
    NS_LOG_INFO("Committed");
    NS_LOG_INFO("at:"<<nodeId);
    NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
    NS_LOG_INFO("priamry:"<<primaryId);
    NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
    NS_LOG_INFO("");

    if (nodeId != primaryId) {
      sendReply();
      newRound();
    }
  }
}


void PBFTCorrect::onBlame(PBFTMessage msg) {

  // note that change-view is not fully implemented and can be stuck
  // in some cases 
  // Todo
  NS_LOG_INFO("onBlame");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  if (msg.getRound() == round) {
    if (incBlameCount(msg.getSignerId()) >= quorum) {
      if (isBackupPrimary()) {
        // claim to be the new primary
        stage = NEWEPOCH;
        sendNewEpoch();
      }
      else {
        // aware of a epoch-changing event
        // Todo: timeout waiting for the backup primary node
        stage = NEWEPOCH;
      }
    }
  }
}


void PBFTCorrect::onReply(PBFTMessage msg) {

  NS_LOG_INFO("OnReplay");
  NS_LOG_INFO("at:"<<nodeId<<" from:"<<msg.getSignerId());
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("r-round:"<<msg.getRound());
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  if (incReplyCount(msg.getRound(), msg.getSignerId()) == totalNodes - 1) {

      NS_LOG_INFO("AllCommitted");
      NS_LOG_INFO("round:"<<msg.getRound());
      NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
      NS_LOG_INFO(""); 

      std::cout<<"<all conf: "<<msg.getRound()<<" "<<Simulator::Now().GetSeconds()<<" >"<<std::endl<<std::endl;
  }

  if (nodeId == primaryId && msg.getRound() == round)  {
    if ( (int) replyCount[round].size() >= quorum) {

      NS_LOG_INFO("MajorityCommitted");
      NS_LOG_INFO("round:"<<round);
      NS_LOG_INFO("priamry:"<<primaryId);
      NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
      NS_LOG_INFO("");

      std::cout<<"<majority conf: "<<round<<" "<<Simulator::Now().GetSeconds()<<" >"<<std::endl<<std::endl;

      // start a new round after honest majority made progress
      // Todo: piggy-bag this on next pre-prepare message
      newRound();

      if (continous) {
        sendRequest();
      }

    }
  }
}


void PBFTCorrect::onNewEpoch(PBFTMessage msg) {
  
  NS_LOG_INFO("on new epoch");
  NS_LOG_INFO("");

  if (msg.getRound() == round && msg.getSignerId() == getBackupPrimary()) {
    // honest majority stuck at same round and agree to change epoch
    msg.reset(messageConstantLen);
    
    msg.setType(CONFIRM_NEWEPOCH);
    msg.setSignerId(nodeId);
    msg.setSrcAddr(nodeId);
    msg.setRound(round);

    discardRound();
    updatePrimary();

    SendToPrimary(std::move(msg));
  }
  else if (msg.getRound() > round && msg.getSignerId() == msg.getRound() % totalNodes) {
    // Todo: retrieve missing rounds

    // if previous round is correct
    round = msg.getRound();

    msg.reset(messageConstantLen);
    msg.setType(CONFIRM_NEWEPOCH);
    msg.setSignerId(nodeId);
    msg.setSrcAddr(nodeId);
    msg.setRound(round);

    discardRound();
    updatePrimary();

    SendToPrimary(std::move(msg));
  }
}


void PBFTCorrect::onConfirmEpoch(PBFTMessage msg) {

  NS_LOG_INFO("on confirm new epoch");
  NS_LOG_INFO("");

  if (incNewEpochCount(msg.getSignerId()) >= quorum) {
    discardRound();
    updatePrimary();

    // Todo: resume the request
    sendRequest();
  }
}


void PBFTCorrect::sendRequest() {

  NS_LOG_INFO("sendRequest");
  NS_LOG_INFO("at:"<<nodeId);
  NS_LOG_INFO("round:"<<round<<"stage:"<<stage);
  NS_LOG_INFO("priamry:"<<primaryId);
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");

  PBFTMessage msg = message();
  msg.setType(REQUEST);
  msg.setSignerId(nodeId);
  msg.setSrcAddr(nodeId);

  std::cout<<"<req: "<<round<<" "<<Simulator::Now().GetSeconds()<<" >"<<std::endl<<std::endl;

  SendToPrimary(std::move(msg));

  stage = REQUEST;

  setTimeoutEvent();

}


void PBFTCorrect::sendReply() {

  if (stage == COMMIT) {
    PBFTMessage msg = message(messageConstantLen);
    msg.setType(REPLY);
    msg.setSignerId(nodeId);
    msg.setSrcAddr(nodeId);
    msg.setRound(round);

    SendToPrimary(std::move(msg));
  }
}


void PBFTCorrect::sendRequestCircle(double inv) {
  
  sendRequest();
  void (PBFTCorrect::*fp)(double) = &PBFTCorrect::sendRequestCircle;
  nextRequestEvent = Simulator::Schedule(Seconds(inv), fp, this, inv);
}


void PBFTCorrect::sendBlame() {

  // notify an outage possibly due to the leader 

  PBFTMessage msg = message(messageConstantLen);
  msg.setType(BLAME);
  msg.setSignerId(nodeId);
  msg.setSrcAddr(nodeId);
  msg.setRound(round);
  
  BroadcastPBFT(std::move(msg));
}


void PBFTCorrect::sendNewEpoch() {

  // get a proof that the leader is faulty, or gather enough blames
  // the proof part is omitted for now

  if (nodeId == getBackupPrimary()) {
    // assert that this method will only be called by backup primary of the claimed round

    PBFTMessage msg = message(messageConstantLen);
    msg.setType(NEWEPOCH);
    msg.setSignerId(nodeId);
    msg.setSrcAddr(nodeId);
    msg.setRound(round);

    BroadcastPBFT(std::move(msg));
  }
}

 
void PBFTCorrect::updatePrimary() {
  primaryId = round % totalNodes;
}


bool PBFTCorrect::isPrimary() {
  return primaryId == nodeId;
}


bool PBFTCorrect::isBackupPrimary() {
  return nodeId == round % totalNodes;
}


uint32_t PBFTCorrect::getBackupPrimary() {
  return round % totalNodes;
}


int PBFTCorrect::incCount(std::set<int> &counter, int n) {
  counter.insert(n);
  return counter.size();
}


int PBFTCorrect::incPrepareCount(int n) {
  return incCount(prepareCount, n);
}


int PBFTCorrect::incCommitCount(int n) {
  return incCount(commitCount, n);
}


int PBFTCorrect::incBlameCount(int n) {
  return incCount(blameCount, n);
}


int PBFTCorrect::incReplyCount(int round, int n) {

  if ( (int) replyCount.size() <= round) {
    replyCount.resize(round + 10);
  }

  return incCount(replyCount[round], n);
}


int PBFTCorrect::incNewEpochCount(int n) {
  return incCount(newEpochCount, n);
}


void PBFTCorrect::newRound() {
  round++;
  stage = IDLE;
  firstPrepare = true;
  firstPreprepare = true;
  prepareCount.clear();
  commitCount.clear();
  blameCount.clear();
  newEpochCount.clear();
  messageRecvPool.clear();
}


void PBFTCorrect::discardRound() {
  stage = IDLE;
  firstPrepare = true;
  firstPreprepare = true;
  prepareCount.clear();
  commitCount.clear();
  if (round < replyCount.size()) {
    replyCount[round].clear();
  }
  newEpochCount.clear();
  messageRecvPool.clear();
}


void PBFTCorrect::setRound(int r) {
  round = r;
}


void PBFTCorrect::setStage(int s) {
  stage = s;
}


void PBFTCorrect::setTotalNode(int n) {
  totalNodes = n;
}


void PBFTCorrect::setQuorum() {
  
  //self count as one vote
  //so donot need to add 1

  quorum = (int) (totalNodes * 2.0 / 3.0);
}


void PBFTCorrect::SendToRoot(PBFTMessage msg) {

  // do not send to self
  NS_ASSERT(!isCoreNode());

  int root = -1;

  // choose the nearest root
  root = corePeerMetric.top().first;

  // sanity check
  NS_ASSERT(root != -1);

  msg.setFromAddr(nodeId);
  Ptr<Packet> packet = msg.toPacket();

  sendToPeer(packet, root);
}


void PBFTCorrect::BroadcastPBFT(PBFTMessage msg) {

	if (relayType == BlockChainApplicationBase::RELAY) {

		msg.setTransportType(ConsensusMessageBase::RELAY);
    msg.setSrcAddr(nodeId);
    msg.setFromAddr(nodeId);

    relay(std::move(msg));
	}

  if (relayType == BlockChainApplicationBase::MIXED) {
    msg.setTransportType(ConsensusMessageBase::MIXED);

    msg.setForwardN(defaultFloodN);
    msg.setTTL(defaultTTL);
		flood(std::move(msg));
  }

  if (relayType == BlockChainApplicationBase::CORE_RELAY) {
        
    msg.setTransportType(ConsensusMessageBase::CORE_RELAY);
    msg.setForwardN(defaultFloodN);
    msg.setTTL(defaultTTL);

    if (isCoreNode()) {
      msg.setSrcAddr(nodeId);
      msg.setFromAddr(nodeId);
		  relay(std::move(msg));
    }
    else {

      if (msg.getTTL() > 0) {
        flood(std::move(msg));
      }
      else {
        SendToRoot(std::move(msg));
      }
    }
  }

  // \cite Fair and Efficient Gossip in Hyperledger Fabric 
  if (relayType == BlockChainApplicationBase::INFECT_UPON_CONTAGION) {
    msg.setTransportType(ConsensusMessageBase::INFECT_UPON_CONTAGION);
    msg.setForwardN(defaultFloodN);
    msg.setTTL(defaultTTL);
    floodAnyway(std::move(msg));
  }
}


void PBFTCorrect::BroadcastPBFT(PBFTMessage msg, double delay) {

  void (PBFTCorrect::*fp)(PBFTMessage) = &PBFTCorrect::BroadcastPBFT;
  delayedBroadcast = Simulator::Schedule(Seconds(delay), fp, this, std::move(msg));

}


/**
 * send the packet to the primary node
 */
void PBFTCorrect::SendToPrimary(PBFTMessage msg) {

  msg.setTransportType(ConsensusMessageBase::DIRECT);

  if (primaryId != nodeId) {
    Ptr<Packet> packet = msg.toPacket();
    sendToPeer(packet, primaryId);
  }
  else {
    if (replicaStat == RUNNING && validateMessage(msg)) {
      onMessageCallback(std::move(msg));
    }
  }
}


/**
 * relay message according to (pre-defined) address book
 */
void PBFTCorrect::relay(PBFTMessage msg) {

  NS_LOG_INFO("relayMessage");
  NS_LOG_INFO("at: "<<nodeId<<" souce: "<<msg.getSrcAddr()<<" from: "<<msg.getFromAddr());
  NS_LOG_INFO("time: "<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");


  RelayEntry k(msg.getSrcAddr(), msg.getFromAddr());

  msg.setFromAddr(nodeId);

  // use node identifier instead of address just for coding simplicity
  // actually knowing ip-address is enough 

  Ptr<Packet> packet = msg.toPacket();
  BroadcastToPeers(packet, k);
}


void PBFTCorrect::flood(PBFTMessage msg) {
  
  uint8_t ttl = msg.getTTL();

  NS_ASSERT(ttl > 0);

  if (ttl != 0) ttl = ttl - 1;
  msg.setTTL(ttl);
  
  floodAnyway(std::move(msg));

}


void PBFTCorrect::flood(PBFTMessage msg, double delay) {

   void (PBFTCorrect::*fp)(PBFTMessage) = &PBFTCorrect::floodAnyway;
   delayedFlood = Simulator::Schedule(Seconds(delay), fp, this, std::move(msg));
   
}


void PBFTCorrect::floodAnyway(PBFTMessage msg) {
    
  NS_LOG_INFO("floodMessage");
  NS_LOG_INFO("at:"<<nodeId<<" souce:"<<msg.getSrcAddr()<<" from:"<<msg.getFromAddr());
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());

  msg.setFromAddr(nodeId);
  uint8_t ttl = msg.getTTL();
  if (ttl != 0) ttl = ttl - 1;
  msg.setTTL(ttl);
  
  Ptr<Packet> packet = msg.toPacket();

  int peerSize = directPeerList.size();

  if (msg.getForwardN() >= peerSize) {
    sendToPeer(packet, directPeerList);
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
      randomizedRecvList.push_back(directPeerList[sortBox[i]]);
      NS_LOG_INFO("to:"<<directPeerList[sortBox[i]]);
    }
    sendToPeer(packet, randomizedRecvList);
  }
  NS_LOG_INFO("");
}

void PBFTCorrect::setTimeoutEvent() {

  clearTimeoutEvent();

  void (PBFTCorrect::*fp)(void) = &PBFTCorrect::onTimeoutCallback;
  timeoutEvent = Simulator::Schedule(Seconds(timeout), fp, this);
}


void PBFTCorrect::clearTimeoutEvent() {
  if (checkEventStatus(timeoutEvent)) {
    Simulator::Cancel(timeoutEvent);
  }
}


void PBFTCorrect::clearScheduledEvent() {

  if (checkEventStatus(nextRequestEvent)) {
    Simulator::Cancel(nextRequestEvent);
  }

  if (checkEventStatus(delayedBroadcast)) {
    Simulator::Cancel(delayedBroadcast);
  }

  if (checkEventStatus(delayedFlood)) {
    Simulator::Cancel(delayedFlood);
  }

}


void PBFTCorrect::setBlockSize(int sz) {
  blockSize = sz;
}


void PBFTCorrect::setContinous(bool c) {
  continous = c;
}


PBFTMessage PBFTCorrect::message() {
  PBFTMessage msg;
  msg.setSeq(seq++);
  return msg;
}


PBFTMessage PBFTCorrect::message(int l) {
  PBFTMessage msg(l);
  msg.setSeq(seq++);
  return msg;
}


template<class MessageType>
bool PBFTCorrect::validateMessage(MessageType& msg) {
  // Unimplemented
  return true;
}


} // namespace ns3