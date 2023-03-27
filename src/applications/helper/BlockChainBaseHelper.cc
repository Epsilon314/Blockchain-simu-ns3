// Yiqing Zhu
// yiqing.zhu.314@gmail.com

//decrepited

#include "ns3/string.h"
#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/BlockChainBaseHelper.h"
#include "ns3/BlockChainApplicationBase.h"


namespace ns3 {

BlockChainBaseHelper::BlockChainBaseHelper(void) {
  idCounter=0;
}

BlockChainBaseHelper::~BlockChainBaseHelper() {}

void BlockChainBaseHelper::commonConstructor(double t) {
  timeout = t;
}

void BlockChainBaseHelper::SetAttribute (std::string name, const AttributeValue &value) {
  mFactory.Set(name, value);
}

ApplicationContainer BlockChainBaseHelper::Install(Ptr<Node> node) {
  return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer BlockChainBaseHelper::Install(std::string nodeName) {
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer BlockChainBaseHelper::Install(NodeContainer c) {
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
    apps.Add(InstallPriv(*i));
  }
  return apps;
}

Ptr<Application> BlockChainBaseHelper::InstallPriv(Ptr<Node> node) {
  Ptr<BlockChainApplicationBase> app = mFactory.Create<BlockChainApplicationBase>();
  app->setNodeId(idCounter++);
  node->AddApplication(app);
  return app;
}

}