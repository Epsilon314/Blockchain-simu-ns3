#include "PBFTMessage.h"
#include <cryptopp/sm3.h>

namespace ns3 {

PBFTMessage::PBFTMessage(void) : ConsensusMessageBase() {
  mMessageType = 0;
  mLenPayload = 20;
  mRound = 0;
  mMessageNo = 0;
  mSignerId = 0;
  mProof = 0;
  mPayload = new unsigned char[mLenPayload]();
}


PBFTMessage::PBFTMessage(int payloadLen) : ConsensusMessageBase() {
  mMessageType = 0;
  mLenPayload = payloadLen;
  mRound = 0;
  mMessageNo = 0;
  mSignerId = 0;
  mProof = 0;
  mPayload = new unsigned char[mLenPayload]();
}


PBFTMessage::PBFTMessage(const PBFTMessage& msg) : ConsensusMessageBase(msg) {
  mMessageType = msg.mMessageType;
  mLenPayload = msg.mLenPayload;
  mRound = msg.mRound;
  mMessageNo = msg.mMessageNo;
  mSignerId = msg.mSignerId;
  mProof = msg.mProof;
  mPayload = new unsigned char[mLenPayload];
  memcpy(mPayload, msg.mPayload, mLenPayload);
}


PBFTMessage& PBFTMessage::operator=(PBFTMessage other) noexcept {
  swap(*this, other);
  return *this;
}


PBFTMessage::PBFTMessage(PBFTMessage&& msg) noexcept : ConsensusMessageBase(msg) {
  swap(*this, msg);
}


bool PBFTMessage::operator==(const PBFTMessage& other) {
  if (!ConsensusMessageBase::operator==(other)) {
    return false;
  }

  if (mMessageType != other.mMessageType || 
      mLenPayload != other.mLenPayload ||
      mRound != other.mRound ||
      mMessageNo != other.mMessageNo ||
      mSignerId != other.mSignerId ||
      mProof != other.mProof)
  {
    return false;
  }
  else {
    for (size_t i = 0; i < mLenPayload; ++i) {
      if (mPayload[i] != other.mPayload[i]) return false;
    }
    return true;
  }
}


PBFTMessage::~PBFTMessage(void) {
  if (mPayload) delete[] mPayload;
}


// friend
void swap(PBFTMessage& a, PBFTMessage& b) noexcept {
  swap(static_cast<ConsensusMessageBase&>(a), static_cast<ConsensusMessageBase&>(b));
  std::swap(a.mMessageType, b.mMessageType);
  std::swap(a.mLenPayload, b.mLenPayload);
  std::swap(a.mRound, b.mRound);
  std::swap(a.mMessageNo, b.mMessageNo);
  std::swap(a.mSignerId, b.mSignerId);
  std::swap(a.mProof, b.mProof);
  std::swap(a.mPayload, b.mPayload);
}


void PBFTMessage::reset() {

  ConsensusMessageBase::reset();

  mMessageType = 0;
  mLenPayload = 20;
  mRound = 0;
  mMessageNo = 0;
  mSignerId = 0;
  mProof = 0;
  mPayload = new unsigned char[mLenPayload]();

}


void PBFTMessage::reset(int payloadLen) {

  ConsensusMessageBase::reset();

  mMessageType = 0;
  mLenPayload = payloadLen;
  mRound = 0;
  mMessageNo = 0;
  mSignerId = 0;
  mProof = 0;
  mPayload = new unsigned char[mLenPayload]();

}


std::ostringstream PBFTMessage::serialization() {
    std::ostringstream out = ConsensusMessageBase::serialization();
    out.write((const char*) &mMagic, 1);
    out.write((const char*) &mMessageType, 4);
    out.write((const char*) &mLenPayload, 4);
    out.write((const char*) &mRound, 4);
    out.write((const char*) &mMessageNo, 4);
    out.write((const char*) &mSignerId, 4);
    out.write((const char*) &mProof, 4);
    out.write((const char*) mPayload, mLenPayload);
    return out;
}


int PBFTMessage::deserialization(int size, unsigned char const serialInput[]) {

  // first parse super class
  int superDeserialization = ConsensusMessageBase::deserialization(mHeadSize, serialInput);
  if (superDeserialization != 0) return 1;
  
  // if super class is parsed successfully, move the pointers and continue parsing
  size -= mHeadSize;
  serialInput += mHeadSize;

  switch (mBlockType) {
  
  case ConsensusMessageBase::NORMAL_BLOCK:

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
    memcpy(&mMessageNo, serialInput, 4);
    serialInput += 4;

    size -= 4;
    if (size < 0) return 1;
    memcpy(&mSignerId, serialInput, 4);
    serialInput += 4;

    size -= 4;
    if (size < 0) return 1;
    memcpy(&mProof, serialInput, 4);
    serialInput += 4;

    delete[] mPayload;
    mPayload = new unsigned char[mLenPayload]();

    size -= mLenPayload;
    if (size < 0) return 1;
    memcpy(mPayload, serialInput, mLenPayload);

    return 0;

    break;
  
  case ConsensusMessageBase::COMPACT_HEAD:
  case ConsensusMessageBase::REQUIRE:
    
    size -= 4;
    if (size < 0) return 1;
    memcpy(&compactHeadSize, serialInput, 4);
    serialInput += 4;

    size -= compactHeadSize;
    if (size < 0) return 1;
    /**
     * TODO: unsafe
     */  
    if (compactHead) {
      delete[] compactHead;
    }
    compactHead = new unsigned char[compactHeadSize];
    memcpy(compactHead, serialInput, compactHeadSize);

    break;
  }
  return 0;

}


Ptr<Packet> PBFTMessage::toPacket() {

  /**
   * we omit the network-byteorder to host-byteorder matter
   * since it is a simulation and will not face different byteorders
   * which should be handled in real implementations
   */

  Ptr<Packet> pkt;

  switch (mBlockType) {
  case ConsensusMessageBase::NORMAL_BLOCK:
    {
      std::ostringstream msgStream(std::stringstream::binary);
      msgStream = serialization();
      pkt = Create<Packet> ((uint8_t*) msgStream.str().c_str(), msgStream.str().length());
      return pkt;
    }
  case ConsensusMessageBase::COMPACT_HEAD:
    {
      std::ostringstream msgStream(std::stringstream::binary);
      msgStream = ConsensusMessageBase::serialization();
      packHead();
      msgStream.write((const char*) compactHeadSize, 4);
      msgStream.write((const char*) compactHead, compactHeadSize);
      pkt = Create<Packet> ((uint8_t*) msgStream.str().c_str(), msgStream.str().length());
      return pkt;
    }
  case ConsensusMessageBase::REQUIRE:
    {
      std::ostringstream msgStream(std::stringstream::binary);
      msgStream = ConsensusMessageBase::serialization();
      msgStream.write((const char*) compactHeadSize, 4);
      msgStream.write((const char*) compactHead, compactHeadSize);
      pkt = Create<Packet> ((uint8_t*) msgStream.str().c_str(), msgStream.str().length());
      return pkt;
    }
    default:
      return pkt;
  }
}


void PBFTMessage::packHead() {

  /**
   * set compactHead with SM3 hash of the packet 
   */

  CryptoPP::SM3 hash;

  std::ostringstream msgStream(std::stringstream::binary);
  msgStream.write((const char*) &mMagic, 1);
  msgStream.write((const char*) &mMessageType, 4);
  msgStream.write((const char*) &mLenPayload, 4);
  msgStream.write((const char*) &mRound, 4);
  msgStream.write((const char*) &mMessageNo, 4);
  msgStream.write((const char*) &mSignerId, 4);
  msgStream.write((const char*) &mProof, 4);
  msgStream.write((const char*) mPayload, mLenPayload);
  msgStream.write((const char*) &mSeq, 4);

  std::string digest;

  hash.Update((const CryptoPP::byte*) msgStream.str().data(), msgStream.str().size());
  digest.resize(hash.DigestSize());
  hash.Final((CryptoPP::byte*)&digest[0]);

  compactHeadSize = digest.length();
  compactHead = new unsigned char[compactHeadSize];

  memcpy(compactHead, digest.data(), compactHeadSize);

}


uint64_t PBFTMessage::uniqueMessageSeq() {
  uint64_t ret = (uint64_t) getSignerId();
  ret = ret << 32;
  ret = ret ^ (uint64_t) getSeq();
  return ret;
}

}