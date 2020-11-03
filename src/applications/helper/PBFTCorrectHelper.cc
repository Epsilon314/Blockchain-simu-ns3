#include "ns3/PBFTCorrectHelper.h"
#include "ns3/string.h"
#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/BlockChainApplicationBase.h"
#include "ns3/PBFTCorrect.h"

namespace ns3 {


PBFTCorrectHelper::PBFTCorrectHelper(uint32_t n, double t) {

  mFactory.SetTypeId("ns3::PBFTCorrect");

  timeout = t;
  mTotalNodes = n;

  /*
   * default values 
   */

  idCounter = 0;
  blockSz = 4;
  delay = 0.05;
  ttl = 2;
  floodN = 1;
  relayType = BlockChainApplicationBase::DIRECT;
  floodR = true;
  continous = false;
  transferModel = BlockChainApplicationBase::PARALLEL;
  
}


PBFTCorrectHelper::~PBFTCorrectHelper() {}


void PBFTCorrectHelper::SetAttribute (std::string name, const AttributeValue &value) {
  mFactory.Set(name, value);
}


/*
 * Set the number of peers that every instance of this app refers to
 * Make sure that your create exactly that number of instances later
 * since it is not guranteed by this class
 */
void PBFTCorrectHelper::SetTotalNodes(uint32_t n) {
  mTotalNodes = n;
}


/*
 * Set the timeout in seconds 
 */
void PBFTCorrectHelper::SetTimeout(double t) {
  timeout = t;
}


/*
 * Set the payload size in bytes 
 */
void PBFTCorrectHelper::SetBlockSz(int sz) {
  blockSz = sz;
}


/*
 * Set the network delay estimated by every app
 */
void PBFTCorrectHelper::SetDelay(double d) {
  delay = d;
}


/*
 * Set the tranport type
 */
void PBFTCorrectHelper::SetTransType(int t) {
  relayType = t;
}


/*
 * Set Time-To-Live in hops
 */
void PBFTCorrectHelper::SetTTL(int t) {
  ttl = t;
}


/*
 * Set number of outbound forward duplications
 */
void PBFTCorrectHelper::SetFloodN(int n) {
  floodN = n;
}


/*
 * If set true, a new request will be sent immediately after the previous one is committed
 * Since in this protocol each correct round works the same,
 * it does not make sense in low adversaty environment and need not to be set.
 */
void PBFTCorrectHelper::SetContinous(bool c) {
  continous = c;
}


/*
 * If set true, app will forward message to random peers
 */
void PBFTCorrectHelper::SetFloodRandomization(bool b) {
  floodR = b;
}


void PBFTCorrectHelper::SetTransferModel(int t) {
  transferModel = t;
}


/*
 * Install functions
 */

ApplicationContainer PBFTCorrectHelper::Install(Ptr<Node> node) {
  return ApplicationContainer(InstallPriv(node));
}


ApplicationContainer PBFTCorrectHelper::Install(std::string nodeName) {
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}


ApplicationContainer PBFTCorrectHelper::Install(NodeContainer c) {
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
    apps.Add(InstallPriv(*i));
  }
  return apps;
}


Ptr<Application> PBFTCorrectHelper::InstallPriv (Ptr<Node> node) {
  Ptr<PBFTCorrect> app = mFactory.Create<PBFTCorrect>();

  app->setTimeout(timeout);
  app->setNodeId(idCounter++);
  app->setTotalNode(mTotalNodes);
  app->setBlockSize(blockSz);
  app->setDelay(delay);
  app->setRelayType(relayType);

  app->setQuorum();

  app->setDefaultTTL(ttl);
  app->setDefaultFloodN(floodN);
  app->setFloodRandomization(floodR);
  app->setContinous(continous);
  app->setTransferModel(transferModel);

  app->updatePrimary();

  node->AddApplication (app);
  return app;
}

}