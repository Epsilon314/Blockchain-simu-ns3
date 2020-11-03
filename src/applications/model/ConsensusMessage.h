#ifndef CONSENSUSMESSAGE_H
#define CONSENSUSMESSAGE_H
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

namespace ns3 {

class ConsensusMessageBase {

protected:

  size_t mHeadSize = 20;  // update this after adding a new field

  // fields 
  // after adding a new field, shall update mHeadSize,
  // serialization() and deserialization() corespendingly

  uint8_t mTransportType; 
  uint8_t mBlockType;
  uint32_t mSrcAddr;
  uint32_t mFromAddr;
  uint32_t mDestinationAddr;
  uint8_t mForwardN;
  uint8_t mTTL;
  uint32_t mSeq;

  // optional fields
  size_t compactHeadSize = 0;
  unsigned char* compactHead = NULL;
  
  //end of fields

  friend void swap(ConsensusMessageBase& a, ConsensusMessageBase& b) noexcept;

  void reset();
  
    
public:


  // application relay protocol to use
  enum TRANSPORTTYPE : uint8_t {
    DIRECT,
    RELAY,
    FLOOD,
    MIXED,
    CORE_RELAY,
    INFECT_UPON_CONTAGION
  };


  enum BLOCKTYPE : uint8_t {
    NORMAL_BLOCK,
    COMPACT_HEAD,
    REQUIRE
  };


  ConsensusMessageBase();
  ConsensusMessageBase(const ConsensusMessageBase& msg);

  ~ConsensusMessageBase();

  bool operator==(const ConsensusMessageBase& other);

  // convert between object and packet buffer
  std::ostringstream serialization();
  int deserialization(int size, unsigned char const serialInput[]);

  // access functions 

  inline void setTransportType(uint8_t t) {mTransportType = t;}

  inline void setBlockType(uint8_t t) {mBlockType = t;}

  inline void setSrcAddr(uint32_t s) {mSrcAddr = s;}

  inline void setFromAddr(uint32_t f) {mFromAddr = f;}

  inline void setDstAddr(uint32_t d) {mDestinationAddr = d;}

  inline void setForwardN(uint8_t n) {mForwardN = n;}

  inline void setTTL(uint8_t ttl) {mTTL = ttl;}

  inline void setSeq(uint32_t s) {mSeq = s;}

  inline uint8_t getTransportType() {return mTransportType;}

  inline uint8_t getBlockType() {return mBlockType;}

  inline uint32_t getSrcAddr() {return mSrcAddr;}

  inline uint32_t getFromAddr() {return mFromAddr;}

  inline uint32_t getDstAddr() {return mDestinationAddr;}

  inline uint8_t getForwardN() {return mForwardN;}

  inline uint8_t getTTL() {return mTTL;}

  inline uint32_t getSeq() {return mSeq;}

  inline size_t getCompactSize() {return compactHeadSize;}

  inline unsigned char* getCompactHead() {return compactHead;}

};

}
#endif
