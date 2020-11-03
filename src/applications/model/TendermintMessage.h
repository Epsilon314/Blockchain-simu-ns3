#ifndef TENDERMINTMESSAGE_H
#define TENDERMINTMESSAGE_H

#include "ConsensusMessage.h"
#include "BlockChainApplicationBase.h"


namespace ns3 {

class TendermintMessage : public ConsensusMessageBase {

private:

	uint8_t mMagic = 0x7a;
	uint32_t mMessageType;
	uint32_t mLenPayload;
	uint32_t mRound;
	uint32_t mHeight;
	uint32_t mSignerId;
	uint32_t mValueId;
	uint32_t mValidRound;
	unsigned char* mPayload;

public:

  enum tendermintState : uint32_t {
    PROPOSAL,
    PRE_VOTE,
    PRE_COMMIT
  };

	TendermintMessage();
  TendermintMessage(int lenPayload);
	~TendermintMessage();
  std::ostringstream serialization();
  int deserialization(int size, unsigned char const serialInput[]);

	Ptr<Packet> toPacket();

  void setType(uint32_t type) {mMessageType = type;}
  void setSignerId(uint32_t id) {mSignerId = id;}
  void setRound(uint32_t round) {mRound = round;}
	void setHeight(uint32_t h) {mHeight = h;}
	void setValueId(uint32_t id) {mValueId = id;}
	void setValidRound(uint32_t r) {mValidRound = r;}

  uint32_t getType() {return mMessageType;}
  uint32_t getSignerId() {return mSignerId;}
  uint32_t getRound() {return mRound;}
	uint32_t getHeight() {return mHeight;}
	uint32_t getValueId() {return mValueId;}
	uint32_t getValidRound() {return mValidRound;}

};


} // namespace ns3
#endif