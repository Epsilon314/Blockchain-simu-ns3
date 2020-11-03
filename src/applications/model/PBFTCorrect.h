#ifndef PBFTCORRECT_H
#define PBFTCORRECT_H

#include "BlockChainApplicationBase.h"
#include "PBFTMessage.h"
#include "MessageRecvPool.h"
#include <algorithm>
#include <array>
#include <random>
#include <chrono>

namespace ns3 {

class Address;
class Socket;
class Packet;
class PBFTMessage;
class MessageRecvPool;

/**
 * PBFT plus
 * basicly Practical Byzantine Fault Tolerance algorithm
 * with optional modifications and overlay networks
 */

class PBFTCorrect : public BlockChainApplicationBase {

protected:

  uint32_t round;
  uint32_t stage;
  int totalNodes;
  int quorum;

  int blockSize;

  // hash, sign etc, rough estimation
  int messageConstantLen = 80;

  uint32_t primaryId;
  
  std::set<int> prepareCount;
  std::set<int> commitCount;
  std::set<int> blameCount;
  // std::set<int> replyCount;
  std::set<int> newEpochCount;

  std::vector<std::set<int> > replyCount;

  EventId nextRequestEvent;
  EventId delayedBroadcast;
  EventId delayedFlood;

  bool firstPrepare = true;
  bool firstPreprepare = true;

  double prepareTime;
  double preprepareTime;


  bool continous;

  bool pooledMsg = true;
  bool checkConflict;
  bool warnConflict;

  PBFTMessage message();
  PBFTMessage message(int l);

  MessageRecvPool messageRecvPool;

  template<class MessageType>
  bool validateMessage(MessageType& msg);

  virtual void StartApplication(void);  
  virtual void StopApplication(void);   
  virtual void DoDispose(void);



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
  void clearTimeoutEvent();

  void clearScheduledEvent();

  void onMessageCallback(void);
  void onMessageCallback(PBFTMessage msg);

  void applicationLayerRelay(PBFTMessage msg);
  void parseMessage(PBFTMessage msg);

  void onRequest(PBFTMessage msg);
  void onPreprepare(PBFTMessage msg);

  void onPrepare(PBFTMessage msg);
  void prepared(bool shortcut = false);

  void onCommit(PBFTMessage msg);
  void committed();

  void onBlame(PBFTMessage msg);
  void onReply(PBFTMessage msg);
  void onNewEpoch(PBFTMessage msg);
  void onConfirmEpoch(PBFTMessage msg);

  void relay(PBFTMessage msg);
  void flood(PBFTMessage msg);
  void flood(PBFTMessage msg, double delay);
  void floodAnyway(PBFTMessage msg);
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

  void SendToPrimary(PBFTMessage msg);

  void SendToRoot(PBFTMessage msg);

  void BroadcastPBFT(PBFTMessage msg); 
  void BroadcastPBFT(PBFTMessage msg, double delay);

  void newRound();
  void discardRound();

  int incCount(std::set<int> &counter, int n);

  int incPrepareCount(int n);
  int incCommitCount(int n);
  int incBlameCount(int n);
  int incReplyCount(int round, int n);
  int incNewEpochCount(int n);

  void setRound(int r);
  void setStage(int s);
  void setTotalNode(int n);
  void setQuorum();
  void updatePrimary();

  void setBlockSize(int sz);
  void setContinous(bool c);

  inline int getRound() {return round;}
  inline int getStage() {return stage;}
  inline int getPrepareCount() {return prepareCount.size();}
  inline int getCommitCount() {return commitCount.size();}
  inline int getPrimary() {return primaryId;}

};

}


#endif