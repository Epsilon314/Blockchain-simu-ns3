#include "TendermintCorrect.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"



namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED(TendermintCorrect);

NS_LOG_COMPONENT_DEFINE("TendermintCorrect");

TypeId TendermintCorrect::GetTypeId (void) {
  static TypeId tid = TypeId ("ns3::TendermintCorrect")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<TendermintCorrect> ()
    .AddTraceSource("Rx",
                    "A packet has been received", 
                    MakeTraceSourceAccessor(&TendermintCorrect::mRxTrace),
                    "ns3::Packet::TracedCallback");
  return tid;
}

TendermintCorrect::TendermintCorrect() {
  step = TendermintMessage::PROPOSAL;
}
  
TendermintCorrect::~TendermintCorrect() {

}

void TendermintCorrect::StartApplication(void) {

  BlockChainApplicationBase::StartApplication();
    
  mListeningSocket->Listen();
  mListeningSocket->ShutdownSend();
  
  mListeningSocket->SetRecvCallback(MakeCallback(&TendermintCorrect::RecvCallback, this));
  mListeningSocket->SetAcceptCallback(MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
                                      MakeCallback(&TendermintCorrect::AcceptCallback, this));
  mListeningSocket->SetCloseCallbacks(MakeCallback(&TendermintCorrect::NormalCloseCallback, this),
                                      MakeCallback(&TendermintCorrect::ErrorCloseCallback, this));

  for (auto address : mPeerAddress) {
    Ptr<Socket> sock = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    InetSocketAddress peer = InetSocketAddress(Ipv4Address::ConvertFrom(address.second), port);
    sock->Connect(peer);
    mPeerSockets.insert(std::pair<int,Ptr<Socket>>(address.first, sock));
  }

	decision.push_back(TENDERMINT_NIL);

	startRound(0);

}


void TendermintCorrect::StopApplication(void) {
	BlockChainApplicationBase::StopApplication();
  clearTimeoutEvent();
} 


void TendermintCorrect::DoDispose(void) {
  Application::DoDispose ();
}


void TendermintCorrect::RecvCallback (Ptr<Socket> socket) {

  Ptr<Packet> packet = sock->Recv();

  mRxTrace(packet);

  int payloadSize = packet->GetSize();
  uint8_t *buffer = new uint8_t[payloadSize];
  packet->CopyData(buffer, payloadSize);
  try {
    TendermintMessage msg(blockSize);
    msg.deserialization(payloadSize, buffer);
    onMessageCallback(msg);
  }
  catch(const std::exception& e) {
    return;
  }

}

void TendermintCorrect::onTimeoutCallback(void) {
}


void TendermintCorrect::onTimeoutProposeCallback(void) {
	if (height == proposeTimeoutHeight && round == proposeTimeoutRound && step == PROPOSAL) {
		TendermintMessage msg;
		msg.setType(TendermintMessage::PRE_VOTE);
		msg.setSignerId(nodeId);
		msg.setRound(round);
		msg.setHeight(height);
		msg.setValueId(TENDERMINT_NIL);
		BroadcastTendermint(msg);
		step = TendermintMessage::PRE_VOTE;
	}
}


void TendermintCorrect::onTimeoutPrevoteCallback(void) {
	if (height == prevoteTimeoutHeight && round == prevoteTimeoutRound && step == PRE_VOTE) {
		TendermintMessage msg;
		msg.setType(PRE_COMMIT);
		msg.setSignerId(nodeId);
		msg.setRound(round);
		msg.setHeight(height);
		msg.setValueId(TENDERMINT_NIL);
		BroadcastTendermint(msg);
		step = PRE_COMMIT;
	}
}


void TendermintCorrect::onTimeoutPrecommitCallback(void) {
	if (height == precommitTimeoutHeight && round == precommitTimeoutRound) {
		startRound(round + 1);
	}
}


void TendermintCorrect::setTimeoutEvent(void) {
  
	clearTimeoutEvent();

  void (TendermintCorrect::*fp)(void) = &TendermintCorrect::onTimeoutCallback;
  timeoutEvent = Simulator::Schedule(Seconds(timeout), fp, this);

}


void TendermintCorrect::clearTimeoutEvent() {
  if (checkEventStatus(timeoutEvent)) {
    Simulator::Cancel(timeoutEvent);
  }
}


void TendermintCorrect::onMessageCallback(TendermintMessage& msg) {
  
  switch(msg.getTransportType()) {
  case ConsensusMessageBase::RELAY:
    relay(msg);
    break;
  case ConsensusMessageBase::FLOOD:
    flood(msg);
    break;
  case ConsensusMessageBase::MIXED:
    if (msg.getTTL() != 0) {
      flood(msg);
    }
    else {
      msg.setSrcAddr(nodeId);
      msg.setFromAddr(nodeId);
      msg.setTransportType(ConsensusMessageBase::RELAY);
      relay(msg);
    }

  }

  NS_LOG_INFO("Parse Message");

	if (msg.getHeight() == height && msg.getRound() > round) {

		std::map<uint32_t, std::set<uint32_t> >::iterator it;
		it = asyMessageCount.find(msg.getRound());

		if (it == asyMessageCount.end()) {
			std::set<uint32_t> newSet;
			newSet.insert(msg.getSignerId());
			asyMessageCount.insert(std::pair<uint32_t, std::set<uint32_t> >(msg.getRound(), newSet));
		}

		else {
			it->second.insert(msg.getSignerId());
			if (it->second.size() == quorumRound) {
				startRound(msg.getRound());
			}
		}
	}

  switch (msg.getType())
  {
  case TendermintMessage::PROPOSAL:
    onProposal(msg);
    break;
  case TendermintMessage::PRE_VOTE:
    onPreVote(msg);
    break;
  case TendermintMessage::PRE_COMMIT:
    onPreCommit(msg);
    break;
  default:
    break;
  }
}


void TendermintCorrect::startRound(int r); {
	
	round = r;
	step = TendermintMessage::PROPOSAL;

	if (getProposer(round, height) == nodeId) {

		TendermintMessage msg;
		msg.setType(TendermintMessage::PROPOSAL);
		msg.setSignerId(nodeId);
		msg.setRound(round);
		msg.setHeight(height);
		msg.setValidRound(validRound);

		if (validValue != TENDERMINT_NIL) {
			msg.setValueId(validValue);
		}
		else {
			msg.setValueId(getNoneSpecialValue());
		}
				
		BroadcastTendermint(msg);
	}
	else {

		// onTimeoutPropose hp rp 
		// timeout rp

		setTimeoutEvent();
	}
}


void TendermintCorrect::onProposal(TendermintMessage& msg) {
	if (msg.getHeight() == height && 
			msg.getSignerId() == getProposer(msg.getRound(), height)) {

		if (msg.getRound() == round) {

			// upon proposal hp rp from proposer

			TendermintMessage rmsg;

			if (msg.getValidRound() == TENDERMINT_MINUSONE && step == TendermintMessage::PROPOSAL) {
				rmsg.setType(TendermintMessage::PROVOTE);
				rmsg.setSignerId(nodeId);
				rmsg.setRound(round);
				rmsg.setHeight(height);
				if (isValidValue(msg) && (lockedRound == -1 || lockedValue == msg.getValueId())) {
					rmsg.setValueId(msg.getValueId());
				}
				else {
					rmsg.setValueId(TENDERMINT_NIL);
				}
				BroadcastTendermint(rmsg);
				step = TendermintMessage::PRE_VOTE;
			}

			if (msg.getValidRound() >= 0 && msg.getValidRound() <= round &&
			 							step == TendermintMessage::PROPOSAL &&  getMessageCount(msg.getValueId(), round, preVoteCount) == quorum) {
				rmsg.setType(TendermintMessage::PRE_VOTE);
				rmsg.setSignerId(nodeId);
				rmsg.setRound(round);
				rmsg.setHeight(height);
				if (isValidValue(msg) && (lockedRound <= msg.getValidRound() || lockedValue == msg.getValueId())) {
					rmsg.setValueId(msg.getValueId());
				}
				else {
					rmsg.setValueId(TENDERMINT_NIL);
				}
				BroadcastTendermint(rmsg);
				step = TendermintMessage::PRE_VOTE;
			}

			if (step >= TendermintMessage::PRE_VOTE &&  getMessageCount(msg.getValueId(), round, preVoteCount) == quorum &&
															 isValidValue(msg) && firstQuorumProposalIndicator) {

        firstQuorumProposalIndicator = false;
				if (step == TendermintMessage::PRE_VOTE) {
					lockedValue = msg.getValueId();
					lockedRound = round;
					rmsg.setType(TendermintMessage::PRE_COMMIT);
					rmsg.setSignerId(nodeId);
					rmsg.setRound(round);
					rmsg.setHeight(height);
					rmsg.setValueId(msg.getValueId());
					BroadcastTendermint(rmsg);
					step = TendermintMessage::PRE_COMMIT;
				}
				validValue = msg.getValueId();
				validRound = round;
			}

		}

		else {

			uint32_t r = msg.getRound();

			if (getMessageCount(msg.getValueId(), r, preCommitCount) == quorum &&
							 decision.at(height) == TENDERMINT_NIL) {
			  if (isValidValue(msg)) {
					decision.at(height) = msg.getValueId();
					incHeight();
					startRound(0);
				}
			}
		}
	}
}


void TendermintCorrect::onPreVote(TendermintMessage& msg) {

	if (msg.getHeight() == height && step == PRE_VOTE && firstQuorumPrevoteIndicator && getMessageCount(round, preVoteCount) >= quorum) {
		firstQuorumPrevoteIndicator = false;
		setTimeoutEvent();
		//TODO set timeout event
	}

	if (msg.getHeight() == height && step == PRE_VOTE && getMessageCount(TENDERMINT_NIL, round, preVoteCount) == quorum) {
		TendermintMessage rmsg;
		rmsg.setType(TendermintMessage::PRE_COMMIT);
		rmsg.setRound(round);
		rmsg.setSignerId(nodeId);
		rmsg.setHeight(height);
		rmsg.setValueId(TENDERMINT_NIL);
		BroadcastTendermint(rmsg);
		step = TendermintMessage::PRE_COMMIT;
	}

}


void TendermintCorrect::onPreCommit(TendermintMessage& msg) {
	if (msg.getHeight() == height && firstQuorumPrecommitIndicator && getMessageCount(round, preCommitCount) == quorum) {
		firstQuorumPrecommitIndicator = false;
		setTimeoutEvent();
		//TODO set timeout event
	}
}


/*
 * save distinct signers for a certain type of message voting for certain value
 * return the count of signers
 */
uint32_t TendermintCorrect::incMessageCount(uint32_t id, uint32_t value, uint32_t round, MessageLog counter) {

	MessageLog::iterator it = counter.find(std::pair<uint32_t,uint32_t>(value,round));
	if (it == counter.end()) {
		std::set<uint32_t> newSet;
		newSet.insert(id);
		counter.insert(std::pair<std::pair<uint32_t,uint32_t>, std::set<uint32_t> >(std::pair<uint32_t,uint32_t>(value,round), newSet));
		return 1;
	}
	else {
		(it->second).insert(id);
		return (it->second).size();
	}
}


uint32_t TendermintCorrect::getMessageCount(uint32_t value, uint32_t round, MessageLog &counter) {
	MessageLog::iterator it = counter.find(std::pair<uint32_t,uint32_t>(value,round));
	if (it == counter.end()) {
		return 0;
	}
	else {
		return (it->second).size();
	}
}


uint32_t TendermintCorrect::getMessageCount(uint32_t round, MessageLog &counter) {
	
	uint32_t count = 0;

	for (auto it = counter.begin(); it != counter.end(); ++it) {
		if (it->first.second == round) {
			count += (it->second).size();
		}
	}
	return count;
}


void TendermintCorrect::setTimeoutEvent(uint32_t eventType) {

	
	switch (eventType) {

		case TendermintMessage::PROPOSAL:
		void (TendermintCorrect::*fp)(void) = &TendermintCorrect::onTimeoutProposeCallback;
  	timeoutProposeEvent=Simulator::Schedule(Seconds(timeoutPropose(round)), fp, this);
		break;

		case TendermintMessage::PRE_VOTE:
		void (TendermintCorrect::*fp)(void) = &TendermintCorrect::onTimeoutPrevoteCallback;
	  timeoutPrevoteEvent=Simulator::Schedule(Seconds(timeoutPrevote(round)), fp, this);
		break;

		case TendermintMessage::PRE_COMMIT:
		void (TendermintCorrect::*fp)(void) = &TendermintCorrect::onTimeoutPrecommitCallback;
  	timeoutPrecommitEvent=Simulator::Schedule(Seconds(timeoutPrecommit(round)), fp, this);
		break;
	}

}



void TendermintCorrect::incHeight() {

	height++;

	// reset all counters, indicators and clocks

	lockedValue = TENDERMINT_NIL;
	lockedRound = TENDERMINT_MINUSONE;

	validValue = TENDERMINT_NIL;
	validRound = TENDERMINT_MINUSONE;

	preVoteCount.clear();
	preCommitCount.clear();
	asyMessageCount.clear();
	

	if (checkEventStatus(timeoutProposeEvent)) {
    Simulator::Cancel(timeoutProposeEvent);
  }

	if (checkEventStatus(timeoutPrevoteEvent)) {
    Simulator::Cancel(timeoutPrevoteEvent);
  }

	if (checkEventStatus(timeoutPrecommitEvent)) {
    Simulator::Cancel(timeoutPrecommitEvent);
  }

	firstQuorumProposalIndicator = true;
	firstQuorumPrevoteIndicator = true;
	firstQuorumPrecommitIndicator = true;

}

void TendermintCorrect::setRound(int r) {
	round = r;
}

void TendermintCorrect::setStep(int s) {
	step = s;
}

void TendermintCorrect::setTotalNode(int n) {
	totalNodes = n;
}

void TendermintCorrect::setQuorum() {
	quorum = (int) totalNodes * 2 / 3;
	quorumRound = (int) totalNodes /2;
}

void TendermintCorrect::BroadcastTendermint(TendermintMessage& msg) {
	if (relayType == DIRECT) {
		Ptr<Packet> packet = msg.toPacket();
		BroadcastToPeers(packet);
	}
	if (relayType == RELAY) {
		msg.setRelayType(RELAY);
		msg.setFrom(nodeId);
		Ptr<Packet> packet = msg.toPacket();
		RelayEntry k(msg.getSignerId(), nodeId);
		BroadcastToPeers(packet, k);
	}
}


// TODO should imple in super class since its a common function
void TendermintCorrect::relay(TendermintMessage& msg) {
	
  NS_LOG_INFO("relayMessage");
  NS_LOG_INFO("at:"<<nodeId<<" souce:"<<msg.getSignerId()<<" from:"<<msg.getFrom());
  NS_LOG_INFO("time:"<<Simulator::Now().GetSeconds());
  NS_LOG_INFO("");


  RelayEntry k(msg.getSignerId(), msg.getFrom());
  msg.setFrom(nodeId);

  Ptr<Packet> packet = msg.toPacket();
  BroadcastToPeers(packet, k);

}



}// namespace ns3