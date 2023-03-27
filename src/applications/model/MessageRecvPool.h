// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#ifndef MESSAGERECVPOOL_H
#define MESSAGERECVPOOL_H

//#include "ConsensusMessage.h"
#include <tuple>
#include <set>
#include <vector>

namespace ns3 {

template <typename MessageType>
class MessageRecvPool {

  // todo: now its totally stored in memory, thus not suitable for large scale expr

public:

  typedef std::tuple<uint8_t, bool, bool> search_result;

  MessageRecvPool();
  ~MessageRecvPool();

  MessageType getMessage(uint64_t id);
  MessageType getMessage(size_t hz, unsigned char* head);
  
  std::set<uint32_t> getSource(MessageType& msg);

  std::set<uint32_t> getSource(uint64_t id);
  std::set<uint32_t> getSource(size_t hz, unsigned char* head);

  /**
   * given a compacted message
   * get the full message if received before
   */
  bool getFullMessage(MessageType& msg);

  uint8_t insert(MessageType& msg);
  search_result insertWithDetect(MessageType& msg);
  
  uint8_t hasMessage(MessageType& msg);
  search_result hasMessageWithDetect(MessageType& msg);

  bool hasConflict(MessageType& msg);

  void clear();


private:

  struct messageEntry {
    uint64_t uniqueMessageId = 0xFFFFFFFF;

    size_t compactHeadSize = 0;
    unsigned char* compactHead = NULL;

    uint8_t receiveFreq = 0;

    std::set<uint32_t> sourceNodeList;

    bool hasFull = false;
    MessageType msg;
  };

  std::vector<messageEntry> mMesgRecvPool;

  search_result _searchPool(MessageType& msg, bool detect, bool insert);
  
};



// definations begin here

template <typename MessageType>
MessageRecvPool<MessageType>::MessageRecvPool() {

}


template <typename MessageType>
MessageRecvPool<MessageType>::~MessageRecvPool() {

}


template <typename MessageType>
typename MessageRecvPool<MessageType>::search_result MessageRecvPool<MessageType>::_searchPool(MessageType& msg, bool detect, bool insert) {

  switch (msg.getBlockType()) {
  
  case MessageType::NORMAL_BLOCK:

  {
    msg.packHead();
    size_t headsize = msg.getCompactSize();
    
    /**
     * if there exists a message in the pool that has the same head with the received message
     * then check if the stored message is conflict with new message
     * increase frequency counter and source node list unless conflict
     */
    for (auto m : mMesgRecvPool) {
      if (headsize == m.compactHeadSize) {
        bool flag = true;
        for (size_t i = 0; i < headsize; ++i) {
          if (msg.getCompactHead()[i] != m.compactHead[i]) {
            flag = false;
            break;
          }
        }
        if (flag) {
          bool conflict = false;
          if (detect && m.hasFull) {
            conflict = !(msg == m.msg);
          }
          if (!conflict && insert) {
            m.sourceNodeList.insert(msg.getSrcAddr());
            m.receiveFreq++;
          }
          return search_result(m.receiveFreq, conflict, m.hasFull);
        }
      }
    }
    
    /**
     * Receive for the first time 
     */

    if (insert) {
      messageEntry newEntry;
      newEntry.msg = msg;
      newEntry.msg.packHead();
      newEntry.uniqueMessageId = msg.uniqueMessageSeq();
      newEntry.receiveFreq = 1;
      newEntry.hasFull = true;
      newEntry.sourceNodeList.insert(msg.getSrcAddr());

      mMesgRecvPool.push_back(newEntry);

      return search_result(1, false, true);
    }
    else {
      return search_result(0, false, false);
    }

    break;
  }

  case MessageType::COMPACT_HEAD:

  {
    size_t headsize = msg.getCompactSize();

    /**
     * if receive a known messages head
     * increase frequency counter and add source node list
     */
    for (auto &m : mMesgRecvPool) {
      if (headsize == m.compactHeadSize) {
        bool flag = true;
        for (size_t i = 0; i < headsize; ++i) {
          if (msg.getCompactHead()[i] != m.compactHead[i]) {
            flag = false;
            break;
          }
        }
        if (flag) {
          if (insert) {
            m.sourceNodeList.insert(msg.getSrcAddr());
            m.receiveFreq++;
          }
          return search_result(m.receiveFreq, false, m.hasFull);
        }
      }
    }

    if (insert) {
      messageEntry newEntry;
      newEntry.msg = msg;
      newEntry.compactHeadSize = newEntry.msg.getCompactSize();
      newEntry.compactHead = newEntry.msg.getCompactHead();
      newEntry.receiveFreq = 1;
      newEntry.sourceNodeList.insert(msg.getSrcAddr());
      newEntry.hasFull = false;
      mMesgRecvPool.push_back(newEntry);
      return search_result(1, false, false);
    }
    else {
      return search_result(0, false, false);
    }

    break;
  }

  default:
    return search_result(0, false, false);
  }
}


template <typename MessageType>
MessageType MessageRecvPool<MessageType>::getMessage(uint64_t id) {
  for (auto m : mMesgRecvPool) {
    if (m.uniqueMessageId == id && m.hasFull) {
      return m.msg;
    }
  }
  return MessageType();
}


template <typename MessageType>
MessageType MessageRecvPool<MessageType>::getMessage(size_t hz, unsigned char* head) {
  for (auto m : mMesgRecvPool) {
    if (hz == m.compactHeadSize) {
      bool flag = true;
      for (size_t i = 0; i < hz; ++i) {
        if (head[i] != m.compactHead[i]) {
          flag = false;
          break;
        }
      }
      if (flag && m.hasFull) {
        return m.msg;
      }
    }
  }
  return MessageType();
}


template <typename MessageType>
std::set<uint32_t> MessageRecvPool<MessageType>::getSource(MessageType& msg) {
  switch (msg.getBlockType()) {
  
  case MessageType::NORMAL_BLOCK:
    return getSource(msg.uniqueMessageSeq());
  case MessageType::COMPACT_HEAD:
    return getSource(msg.getCompactSize(), msg.getCompactHead());
  default:
    return std::set<uint32_t>();
  
  }
}


template <typename MessageType>
std::set<uint32_t> MessageRecvPool<MessageType>::getSource(uint64_t id) {
  for (auto m : mMesgRecvPool) {
    if (m.uniqueMessageId == id && m.hasFull) {
      return m.sourceNodeList;
    }
  }
  return std::set<uint32_t>();
}


template <typename MessageType>
std::set<uint32_t> MessageRecvPool<MessageType>::getSource(size_t hz, unsigned char* head) {
  for (auto m : mMesgRecvPool) {
    if (hz == m.compactHeadSize) {
      bool flag = true;
      for (size_t i = 0; i < hz; ++i) {
        if (head[i] != m.compactHead[i]) {
          flag = false;
          break;
        }
      }
      if (flag && m.hasFull) {
        return m.sourceNodeList;
      }
    }
  }
  return std::set<uint32_t>();
}


template <typename MessageType>
bool MessageRecvPool<MessageType>::getFullMessage(MessageType& msg) {
  size_t headsize = msg.getCompactSize();

  for (auto m : mMesgRecvPool) {
    if (headsize == m.compactHeadSize && m.hasFull) {
      bool flag = true;
      for (size_t i = 0; i < headsize; ++i) {
        if (msg.getCompactHead()[i] != m.compactHead[i]) {
          flag = false;
          break;
        }
      }
      if (flag) {
        msg = m.msg;
        return true;
      }
    }
  }
  return false;
}


template <typename MessageType>
uint8_t MessageRecvPool<MessageType>::insert(MessageType& msg) {
  return std::get<0>(_searchPool(msg, false, true));
}


template <typename MessageType>
typename MessageRecvPool<MessageType>::search_result MessageRecvPool<MessageType>::insertWithDetect(MessageType& msg) {
  return _searchPool(msg, true, true);
}


template <typename MessageType>
uint8_t MessageRecvPool<MessageType>::hasMessage(MessageType& msg) {
  return std::get<0>(_searchPool(msg, false, false));
}


template <typename MessageType>
typename MessageRecvPool<MessageType>::search_result MessageRecvPool<MessageType>::hasMessageWithDetect(MessageType& msg) {
  return _searchPool(msg, true, false);
}


template <typename MessageType>
bool MessageRecvPool<MessageType>::hasConflict(MessageType& msg) {
  return std::get<1>(_searchPool(msg, true, false));
}


template <typename MessageType>
void MessageRecvPool<MessageType>::clear() {
  mMesgRecvPool.clear();
}


} // namespace ns3 
#endif