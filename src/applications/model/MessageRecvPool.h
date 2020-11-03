#ifndef MESSAGERECVPOOL_H
#define MESSAGERECVPOOL_H

#include "PBFTMessage.h"
#include <tuple>

namespace ns3 {

class MessageRecvPool {

public:

  typedef std::tuple<uint8_t, bool, bool> search_result;

  MessageRecvPool();
  ~MessageRecvPool();

  PBFTMessage getMessage(uint64_t id);
  PBFTMessage getMessage(size_t hz, unsigned char* head);
  
  std::set<uint32_t> getSource(PBFTMessage& msg);
  std::set<uint32_t> getSource(uint64_t id);
  std::set<uint32_t> getSource(size_t hz, unsigned char* head);

  /**
   * given a compacted message
   * get the full message if received before
   */
  bool getFullMessage(PBFTMessage& msg);

  uint8_t insert(PBFTMessage& msg);
  search_result insertWithDetect(PBFTMessage& msg);
  
  uint8_t hasMessage(PBFTMessage& msg);
  search_result hasMessageWithDetect(PBFTMessage& msg);

  bool hasConflict(PBFTMessage& msg);

  void clear();


private:

  struct messageEntry {
    uint64_t uniqueMessageId = 0xFFFFFFFF;

    size_t compactHeadSize = 0;
    unsigned char* compactHead = NULL;

    uint8_t receiveFreq = 0;

    std::set<uint32_t> sourceNodeList;

    bool hasFull = false;
    PBFTMessage msg;
  };

  std::vector<messageEntry> mMesgRecvPool;

  search_result _searchPool(PBFTMessage& msg, bool detect, bool insert);
  
};


} // namespace ns3 
#endif