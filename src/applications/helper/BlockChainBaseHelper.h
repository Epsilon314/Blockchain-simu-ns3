// Yiqing Zhu
// yiqing.zhu.314@gmail.com

//decrepited

#ifndef BLOCKCHAINBASEHELPER_H
#define BLOCKCHAINBASEHELPER_H

#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/uinteger.h"

namespace ns3 {

class BlockChainBaseHelper {

public:

  BlockChainBaseHelper(void);
  ~BlockChainBaseHelper();

  void commonConstructor(double t);

  void SetAttribute (std::string name, const AttributeValue &value);

  ApplicationContainer Install (NodeContainer c);
  ApplicationContainer Install (Ptr<Node> node);
  ApplicationContainer Install (std::string nodeName);

protected:

  virtual Ptr<Application> InstallPriv (Ptr<Node> node);
  ObjectFactory mFactory;

  int nodeId;
  double timeout;
  int idCounter;

};


}

#endif
