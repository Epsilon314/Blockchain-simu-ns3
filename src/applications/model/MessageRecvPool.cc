#include "MessageRecvPool.h"

namespace ns3 {

MessageRecvPool::MessageRecvPool() {

}

MessageRecvPool::~MessageRecvPool() {

}


MessageRecvPool::search_result MessageRecvPool::_searchPool(PBFTMessage& msg, bool detect, bool insert) {

  switch (msg.getBlockType()) {
  
  case ConsensusMessageBase::NORMAL_BLOCK:

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

  case ConsensusMessageBase::COMPACT_HEAD:

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


PBFTMessage MessageRecvPool::getMessage(uint64_t id) {
  for (auto m : mMesgRecvPool) {
    if (m.uniqueMessageId == id && m.hasFull) {
      return m.msg;
    }
  }
  return PBFTMessage();
}

PBFTMessage MessageRecvPool::getMessage(size_t hz, unsigned char* head) {
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
  return PBFTMessage();
}

std::set<uint32_t> MessageRecvPool::getSource(PBFTMessage& msg) {
  switch (msg.getBlockType()) {
  
  case ConsensusMessageBase::NORMAL_BLOCK:
    return getSource(msg.uniqueMessageSeq());
  case ConsensusMessageBase::COMPACT_HEAD:
    return getSource(msg.getCompactSize(), msg.getCompactHead());
  default:
    return std::set<uint32_t>();
  
  }
}

std::set<uint32_t> MessageRecvPool::getSource(uint64_t id) {
  for (auto m : mMesgRecvPool) {
    if (m.uniqueMessageId == id && m.hasFull) {
      return m.sourceNodeList;
    }
  }
  return std::set<uint32_t>();
}

std::set<uint32_t> MessageRecvPool::getSource(size_t hz, unsigned char* head) {
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

bool MessageRecvPool::getFullMessage(PBFTMessage& msg) {
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

uint8_t MessageRecvPool::insert(PBFTMessage& msg) {
  return std::get<0>(_searchPool(msg, false, true));
}

MessageRecvPool::search_result MessageRecvPool::insertWithDetect(PBFTMessage& msg) {
  return _searchPool(msg, true, true);
}

uint8_t MessageRecvPool::hasMessage(PBFTMessage& msg) {
  return std::get<0>(_searchPool(msg, false, false));
}

MessageRecvPool::search_result MessageRecvPool::hasMessageWithDetect(PBFTMessage& msg) {
  return _searchPool(msg, true, false);
}

bool MessageRecvPool::hasConflict(PBFTMessage& msg) {
  return std::get<1>(_searchPool(msg, true, false));
}

void MessageRecvPool::clear() {
  mMesgRecvPool.clear();
}

} //namespace ns3

