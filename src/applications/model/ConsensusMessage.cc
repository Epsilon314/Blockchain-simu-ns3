// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#include "ConsensusMessage.h"
#include <limits>

namespace ns3 {

ConsensusMessageBase::ConsensusMessageBase() :
  mTransportType(DIRECT),
  mBlockType(NORMAL_BLOCK),
  mSrcAddr(0),
  mFromAddr(0),
  mDestinationAddr(std::numeric_limits<uint32_t>::infinity()),
  mForwardN(0),
  mTTL(0),
  mSeq(0),
  mTs(0.0),
  compactHeadSize(32),
  compactHead(NULL) {}


ConsensusMessageBase::ConsensusMessageBase(const ConsensusMessageBase& msg) {
  mTransportType = msg.mTransportType;
  mBlockType = msg.mBlockType;
  mSrcAddr = msg.mSrcAddr;
  mFromAddr = msg.mFromAddr;
  mDestinationAddr = msg.mDestinationAddr;
  mForwardN = msg.mForwardN;
  mTTL = msg.mTTL;
  mSeq = msg.mSeq;
  mTs = msg.mTs;
  compactHeadSize = msg.compactHeadSize;
  if (compactHeadSize > 0) {
      compactHead = new unsigned char[compactHeadSize];
  }
  if (msg.compactHead != NULL) {
      memcpy(compactHead, msg.compactHead, compactHeadSize);
  }
}


ConsensusMessageBase::~ConsensusMessageBase() {
  if (compactHead) delete[] compactHead;
}


// friend
void swap(ConsensusMessageBase& a, ConsensusMessageBase& b) noexcept{
  std::swap(a.mTransportType, b.mTransportType);
  std::swap(a.mBlockType, b.mBlockType);
  std::swap(a.mSrcAddr, b.mSrcAddr);
  std::swap(a.mFromAddr, b.mFromAddr);
  std::swap(a.mDestinationAddr, b.mDestinationAddr);
  std::swap(a.mForwardN, b.mForwardN);
  std::swap(a.mTTL, b.mTTL);
  std::swap(a.mSeq, b.mSeq);
  std::swap(a.mTs, b.mTs);
  std::swap(a.compactHeadSize, b.compactHeadSize);
  std::swap(a.compactHead, b.compactHead);
}


void ConsensusMessageBase::reset() {
  mTransportType = DIRECT;
  mBlockType = NORMAL_BLOCK;
  mSrcAddr = 0;
  mFromAddr = 0;
  mDestinationAddr = std::numeric_limits<uint32_t>::infinity();
  mForwardN = 0;
  mTTL = 0;
  mSeq = 0;
  mTs = 0.0;
  compactHeadSize = 32;
  compactHead = NULL;
}


/**
 * when comparing messages 
 * we do not check variable fields (used as overlay network labels)
 */ 
bool ConsensusMessageBase::operator==(const ConsensusMessageBase& other) {
  if (mBlockType != other.mBlockType ||
      mSeq != other.mSeq ||
      compactHeadSize != other.compactHeadSize)
  {
    return false;
  }
  else {
    for (size_t i = 0; i < compactHeadSize; ++i) {
      if (compactHead[i] != other.compactHead[i]) return false;
    }
    return true;
  }
}


std::ostringstream ConsensusMessageBase::serialization() {
  std::ostringstream out(std::stringstream::binary);
  out.write((const char*) &mTransportType, 1);
  out.write((const char*) &mBlockType, 1);
  out.write((const char*) &mSrcAddr, 4);
  out.write((const char*) &mFromAddr, 4);
  out.write((const char*) &mDestinationAddr, 4);
  out.write((const char*) &mForwardN, 1);
  out.write((const char*) &mTTL, 1);
  out.write((const char*) &mSeq, 4);
  out.write((const char*) &mTs, 8);
  return out;
}


int ConsensusMessageBase::deserialization(int size, unsigned char const serialInput[]) {

  // parse the buffer as consensus message base

  size -= 1;
  if (size < 0) return 1;
  memcpy(&mTransportType, serialInput, 1);
  serialInput += 1;

  size -= 1;
  if (size < 0) return 1;
  memcpy(&mBlockType, serialInput, 1);
  serialInput += 1;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mSrcAddr, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mFromAddr, serialInput, 4);
  serialInput += 4;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mDestinationAddr, serialInput, 4);
  serialInput += 4;

  size -= 1;
  if (size < 0) return 1;
  memcpy(&mForwardN, serialInput, 1);
  serialInput += 1;

  size -= 1;
  if (size < 0) return 1;
  memcpy(&mTTL, serialInput, 1);
  serialInput += 1;

  size -= 4;
  if (size < 0) return 1;
  memcpy(&mSeq, serialInput, 4);
  serialInput += 4;

  size -= 8;
  if (size < 0) return 1;
  memcpy(&mTs, serialInput, 8);
  serialInput += 8;

  if (size == 0) {
    // all fields parsed
    return 0;
  }
  else {
    // more fields to be parsed
    return 2;
  }

}

}