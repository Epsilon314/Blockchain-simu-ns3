// Yiqing Zhu
// yiqing.zhu.314@gmail.com

//unfinished

#ifndef TENDERMINTCORRECT_H
#define TENDERMINTCORRECT_H

#include "BlockChainApplicationBase.h"
#include "TendermintMessage.h"
#include <set>

namespace ns3 {

class Address;
class Socket;
class Packet;

typedef std::map<std::pair<uint32_t, uint32_t>, std::set<uint32_t> > MessageLog;


class TendermintCorrect : public BlockChainApplicationBase {

private:

	uint32_t round = 0;
	uint32_t height = 0;

	uint32_t step;

	uint32_t lockedValue = TENDERMINT_NIL;
	uint32_t lockedRound = TENDERMINT_MINUSONE;
	
	uint32_t validValue = TENDERMINT_NIL;
	uint32_t validRound = TENDERMINT_MINUSONE;

	std::vector<uint32_t> decision;

	int totalNodes;
	uint32_t quorum;
	uint32_t quorumRound;
	int blockSize = 4;
	int messageConstantLen = 20;

	// std::map<std::pair<uint32_t, uint32_t>, std::set<uint32_t> >
	// map (value, nodes vote for that value in some round)
	MessageLog preVoteCount;
	MessageLog preCommitCount;

	std::map<uint32_t, std::set<uint32_t> > asyMessageCount;

	uint32_t NoneSpecialValueCount = LAST_MARKER;

  EventId nextRequestEvent;

	EventId timeoutProposeEvent;
	EventId timeoutPrevoteEvent;
	EventId timeoutPrecommitEvent;

	uint32_t proposeTimeoutHeight;
	uint32_t proposeTimeoutRound;
	uint32_t prevoteTimeoutHeight;
	uint32_t prevoteTimeoutRound;
	uint32_t precommitTimeoutHeight;
	uint32_t precommitTimeoutRound;


	double timeoutPropose(uint32_t round);
	double timeoutPrevote(uint32_t round);
	double timeoutPrecommit(uint32_t round);

	bool firstQuorumProposalIndicator = true;
	bool firstQuorumPrevoteIndicator = true;
	bool firstQuorumPrecommitIndicator = true;

	void onTimeoutProposeCallback(void);
	void onTimeoutPrevoteCallback(void);
	void onTimeoutPrecommitCallback(void);

	void onTimeoutCallback(void);
  void setTimeoutEvent(void);
	void setTimeoutEvent(uint32_t eventType);
  void clearTimeoutEvent();
  void onMessageCallback(TendermintMessage& msg);

	void RecvCallback (Ptr<Socket> socket);

	void SendToLeader(Ptr<Packet> pkt);
  void BroadcastToPeers(Ptr<Packet> pkt);
  void BroadcastToPeers(Ptr<Packet> pkt, RelayEntry k);
  void BroadcastToPeersWithDelay(Ptr<Packet> pkt, double delay);
  void BroadcastToPeersWithDelay(Ptr<Packet> pkt, double delay, RelayEntry k);
	void BroadcastTendermint(TendermintMessage& msg);

	void onProposal(TendermintMessage& msg);
	void onPreVote(TendermintMessage& msg);
	void onPreCommit(TendermintMessage& msg);

	void onTimeoutProposal();
	void onTimeoutPreVote();
	void onTimeoutPreCommit();

	int getProposer(int round, int height);

	void incHeight();

	uint32_t getNoneSpecialValue() {
		
		// external 
		// return some payload

		return NoneSpecialValueCount++;
	}

	bool isValidValue(TendermintMessage& msg) {

		// external 
		// check if the message is valid

		return true;

	}

	uint32_t incMessageCount(uint32_t id, uint32_t value, uint32_t round, MessageLog counter);
	uint32_t getMessageCount(uint32_t value, uint32_t round, MessageLog counter);
	uint32_t getMessageCount(uint32_t round, MessageLog counter);

	void relay(TendermintMessage& msg);

protected:

  virtual void StartApplication(void);  
  virtual void StopApplication(void);   
  virtual void DoDispose(void);

public:

	/*
	* -1 and nil discribed in the orignal paper
	*/
	enum TENDERMINT_SPECIAL : uint32_t {
		TENDERMINT_MINUSONE,
		TENDERMINT_NIL,


		LAST_MARKER // do not remove 
	};

  static TypeId GetTypeId (void);

  TendermintCorrect();
  
  virtual ~TendermintCorrect(void);

	void startRound(int r);

	void setRound(int r);
  void setStep(int s);
  void setTotalNode(int n);
  void setQuorum();

};


} //namespace ns3
#endif 