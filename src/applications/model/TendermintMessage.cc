#include "TendermintMessage.h"

namespace ns3 {


TendermintMessage::TendermintMessage() {

  ConsensusMessageBase();
  mMessageType = 0;
  mLenPayload = 4;
  mRound = 0;
  mSignerId = 0;
  mPayload = new unsigned char[mLenPayload]();

}

TendermintMessage::TendermintMessage(int lenPayload) {

  ConsensusMessageBase();
  mMessageType = 0;
  mLenPayload = lenPayload;
  mRound = 0;
  mSignerId = 0;
  mPayload = new unsigned char[mLenPayload]();

}

TendermintMessage::~TendermintMessage() {
	delete[] mPayload;
}


std::ostringstream TendermintMessage::serialization() {

	std::ostringstream out = ConsensusMessageBase::serialization();
	out.write((const char*) &mMagic, 1);
	out.write((const char*) &mMessageType, 4);
	out.write((const char*) &mLenPayload, 4);
	out.write((const char*) &mRound, 4);
	out.write((const char*) &mHeight, 4);
	out.write((const char*) &mSignerId, 4);
	out.write((const char*) &mValueId, 4);
  out.write((const char*) &mValidRound, 4);
	out.write((const char*) mPayload, mLenPayload);
	return out;

}



int TendermintMessage::deserialization(int size, unsigned char const serialInput[]) {

  // first parse super class
  int superDeserialization = ConsensusMessageBase::deserialization(mHeadSize, serialInput);
  if (superDeserialization != 0) return 1;
  
  // if super class is parsed successfully, move the pointers and continue parsing
  size -= mHeadSize;
  serialInput += mHeadSize;


  size -= 1;
  if (size < 0) return 1;
  uint8_t rMagic;
  memcpy(&rMagic, serialInput, 1);
  if (rMagic != mMagic) return 1;
  serialInput += 1;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mMessageType, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mLenPayload, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mRound, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mHeight, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mSignerId, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mValueId, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mValidRound, serialInput, 4);
  serialInput += 4;

  delete[] mPayload;
  mPayload = new unsigned char[mLenPayload]();

  size -= 4;
  if (size < 0) return 1;
  memcpy(mPayload, serialInput, mLenPayload);

  return 0;

}

Ptr<Packet> TendermintMessage::toPacket() {
  std::ostringstream msgStream(std::stringstream::binary);

  /*
  we omit the network-byteorder to host-byteorder matter
  since it is a simulation and will not face different byteorders
  which should be handled in real implementations
  */

  msgStream = serialization();

  Ptr<Packet> pkt = Create<Packet> ((uint8_t*) msgStream.str().c_str(), msgStream.str().length());

  return pkt;
}


} //namespace ns3