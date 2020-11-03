#ifndef PBFTCORRECTHELPER_H
#define PBFTCORRECTHELPER_H

#include "ns3/object-factory.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/uinteger.h"
#include "ns3/PBFTCorrect.h"

namespace ns3 {


/*
 * a helper class to create pbftcorrect applications with setted parameters
 * same usage as other ns3 applicaiton helpers
 */

class PBFTCorrectHelper {

public:

  PBFTCorrectHelper(uint32_t n, double t);
  ~PBFTCorrectHelper();

  void SetTotalNodes(uint32_t n);
  void SetTimeout(double t);
  void SetBlockSz(int sz);
  void SetDelay(double d);
  void SetTransType(int t);
  void SetTTL(int t);
  void SetFloodN(int n);
  void SetFloodRandomization(bool b);
  void SetContinous(bool c);
  void SetTransferModel(int t);

  void SetAttribute (std::string name, const AttributeValue &value);

  ApplicationContainer Install (NodeContainer c);
  ApplicationContainer Install (Ptr<Node> node);
  ApplicationContainer Install (std::string nodeName);

protected:

  virtual Ptr<Application> InstallPriv (Ptr<Node> node);
  
  ObjectFactory mFactory;

  uint32_t mTotalNodes;
  double timeout;
  int idCounter;
  int blockSz;
  double delay;
  int relayType;
  int ttl;
  int floodN;
  bool floodR;
  bool continous;
  int transferModel;
    
};

}
#endif