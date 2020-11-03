#ifndef PBFTMESSAGE_H
#define PBFTMESSAGE_H

#include "ConsensusMessage.h"
#include "BlockChainApplicationBase.h"

namespace ns3 {

class PBFTMessage : public ConsensusMessageBase {

private:

  uint8_t mMagic = 0xc4;
  uint32_t mMessageType;
  uint32_t mLenPayload = 0;
  uint32_t mRound;
  uint32_t mMessageNo;
  uint32_t mSignerId;
  uint32_t mProof;
  unsigned char* mPayload = NULL;

public:

  PBFTMessage();
  PBFTMessage(int payloadLen);
  PBFTMessage(const PBFTMessage& msg);
  PBFTMessage& operator=(PBFTMessage other) noexcept;
  PBFTMessage(PBFTMessage&& msg) noexcept;

  bool operator==(const PBFTMessage& other);

  virtual ~PBFTMessage();

  friend void swap(PBFTMessage& a, PBFTMessage& b) noexcept;

  void reset();
  void reset(int payloadLen);

  std::ostringstream serialization();

  int deserialization(int size, unsigned char const serialInput[]);

  Ptr<Packet> toPacket();

  void packHead();

  
  inline void setType(uint32_t type) {mMessageType = type;}


  inline void setSignerId(uint32_t id) {mSignerId = id;}


  inline void setRound(uint32_t round) {mRound = round;}


  inline void setNo(uint32_t no) {mMessageNo = no;}


  inline void setProof(uint32_t proof) {mProof = proof;}


  inline uint32_t getType() {return mMessageType;}


  inline uint32_t getSignerId() {return mSignerId;}


  inline uint32_t getRound() {return mRound;}


  inline uint32_t getNo() {return mMessageNo;}


  inline uint32_t getProof() {return mProof;}


  uint64_t uniqueMessageSeq();

};

}
#endif