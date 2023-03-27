// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#ifndef PBFTCORRECT_H
#define PBFTCORRECT_H

#include "BlockChainApplicationBase.h"
#include "PBFTMessage.h"

#include <algorithm>


namespace ns3 {

/**
 * PBFT plus
 * basicly Practical Byzantine Fault Tolerance algorithm
 * with optional modifications and overlay networks
 */

class PBFTCorrect : public BlockChainApplicationBase<PBFTMessage> {

protected:

  uint32_t round;
  uint32_t stage;

  int totalNodes;
  int voteNodes;

  int quorum;

  int blockSize;

  // hash, sign etc, rough estimation
  int messageConstantLen = 80;

  int broadcast_duplicates = 1;

  uint32_t primaryId;
  
  std::set<int> prepareCount;
  std::set<int> commitCount;
  std::set<int> blameCount;

  std::set<int> newEpochCount;

  std::vector<std::set<int> > replyCount;

  EventId nextRequestEvent;

  bool firstPrepare = true;
  bool firstPreprepare = true;

  double prepareTime;
  double preprepareTime;


  bool continous;

  bool checkConflict;
  bool warnConflict;

  bool msgLatencyLogOn = true;

  // <source id, latency, unique msg id>
  // for latency statistic
  std::vector <std::tuple<u_int32_t, double, u_int64_t>> recvMsgLog;

  PBFTMessage message();
  PBFTMessage message(int l);

  std::queue<PBFTMessage> pendingRequest;

  template<class MessageType>
  bool validateMessage(MessageType& msg);

  virtual void StartApplication(void);  
  virtual void StopApplication(void);   
  virtual void DoDispose(void);

  void invokePending();

public:

  enum PBFT_PRIMITIVE : uint32_t {
    IDLE,
    REQUEST,
    PRE_PREPARE,
    PREPARE,
    COMMIT,
    
    BLAME,
    
    REPLY,

    NEWEPOCH,
    CONFIRM_NEWEPOCH
  };
  
  static TypeId GetTypeId (void);

  PBFTCorrect();
  
  virtual ~PBFTCorrect(void);

  void onTimeoutCallback(void);
  void setTimeoutEvent(void);

  void clearScheduledEvent();

  void parseMessage(PBFTMessage msg);

  void onRequest(PBFTMessage msg);
  void onPreprepare(PBFTMessage msg);

  void onPrepare(PBFTMessage msg);
  void prepared(bool shortcut = false);

  void onCommit(PBFTMessage msg);
  void committed();

  void onBlame(PBFTMessage msg);
  void onReply(PBFTMessage msg);

  void nextRound();

  void onNewEpoch(PBFTMessage msg);
  void onConfirmEpoch(PBFTMessage msg);

  void onRequestTimeout();
  void onPreprepareTimeout();
  void onPrepareTimeout();

  void sendRequest();
  void sendRequestCircle(double inv);
  void sendReply();

  void sendBlame();
  void sendNewEpoch();

  void RecvCallback (Ptr<Socket> socket);

  bool isPrimary();
  bool isBackupPrimary();
  uint32_t getBackupPrimary();

  void sendToPrimary(PBFTMessage msg);

  void sendToRoot(PBFTMessage msg, int duplicates);

  void BroadcastPBFT(PBFTMessage msg); 
  void BroadcastPBFT(PBFTMessage msg, double delay);

  void BroadcastTest();

  void newRound();
  void discardRound();

  int incCount(std::set<int> &counter, int n);

  int incPrepareCount(int n);
  int incCommitCount(int n);
  int incBlameCount(int n);
  int incReplyCount(int r, int n);
  int incNewEpochCount(int n);

  void setRound(int r);
  void setStage(int s);
  void setTotalNode(int n);
  void setQuorum();
  void setVote(int n);
  void updatePrimary();

  void setBlockSize(int sz);
  void setContinous(bool c);

  void setLatencyLog(bool l) {msgLatencyLogOn = l;}

  void setBroadcastDuplicateCount(int c) {broadcast_duplicates = c;}

  inline int getRound() {return round;}
  inline int getStage() {return stage;}
  
  int getPrepareCount() {return prepareCount.size();}
  int getCommitCount() {return commitCount.size();}
  int getBlameCount() {return blameCount.size();}
  int getReplyCount() {return replyCount.size() > round ? replyCount[round].size() : 0;}
  int getNewEpochCount() {return newEpochCount.size();}
  int getPrimary() {return primaryId;}

  double getAverageLatency();

};

}

#endif