// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#include "BlockChainApplicationBase.h"
#include <iostream>
#include <algorithm>


namespace ns3 {


// rename me 
// implementation in .h file (to avoid some template problem)  


// implementation of class RelayEntry

RelayEntry::RelayEntry(int s, int f) : src(s), from(f) {}


RelayEntry::RelayEntry() : src(0), from(0) {}


bool RelayEntry::operator<(const RelayEntry &that) const{
  if (src != that.src) return src < that.src;
  else return from < that.from;
}


// implementation of class SendQuestBuffer

SendQuestBuffer::SendQuestBuffer() : currentIndex(0) {}


bool SendQuestBuffer::empty() {

  return sendBuffer.empty() && currentIndex == (int) currentQuest.mReceivers.size();

}


std::pair<Ptr<Packet>, int> SendQuestBuffer::getNext() {

  //Todo: will tree-wise scheluding help? 

  if (currentIndex < (int) currentQuest.mReceivers.size()) {
    return std::pair<Ptr<Packet>, int>(
      currentQuest.mPkt,
      currentQuest.mReceivers[currentIndex++]
    );
  }
  else {
    currentIndex = 0;

    NS_ASSERT(!sendBuffer.empty());

    currentQuest = sendBuffer.front();
    sendBuffer.pop_front();

    return std::pair<Ptr<Packet>, int>(
      currentQuest.mPkt,
      currentQuest.mReceivers[currentIndex++]
    );
  }
}


void SendQuestBuffer::insert(SendQuest quest) {
  sendBuffer.push_back(quest);
}



} // namespace ns3