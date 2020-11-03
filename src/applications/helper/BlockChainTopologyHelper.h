#ifndef BLOCKCHAINTOPOLOGYHELPER_H
#define BLOCKCHAINTOPOLOGYHELPER_H

#include "ns3/PBFTCorrect.h"
#include "ns3/PBFTCorrectHelper.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include "SpectralClustering.h"

#include <set>
#include <tuple>
#include <iostream>
#include <vector>
#include <map>

namespace ns3 {

class Address;
class PointToPointHelper;


class BlockChainTopologyHelper {
    

public:

  // begin definations of structures and enums 

  struct LinkInfo {
    int nodeA;            // source node id
    int nodeB;            // destination node id
    double delay;         // seconds
    int bw;               // bps 
    int share;            // # of overlay links   
    double linkMetric;    // link metric, see updateNodeMetric() for defination
  
    LinkInfo() : nodeA(0), nodeB(0), delay(0), bw(0), share(0), linkMetric(0) {}

    LinkInfo(int a, int b, double d, int i, int s, double l) :
      nodeA(a), nodeB(b), delay(d), bw(i), share(s), linkMetric(l) {}
  
  };


  struct Route {
    int src;              // source node id          
    int dst;              // next hop node id      
    int from;             // previous hop node id
    int self;             // self node id 

    // do not care about it in fact, just need a comparison operator
    // any PARTIAL ORDER relation is OK
    bool operator<(const Route& that) const {
      if (src != that.src) return src < that.src;
      if (dst != that.dst) return dst < that.dst;
      if (from != that.from) return from < that.from;
      return self < that.self;
    }

    Route(int s, int d, int f, int r) : src(s), dst(d), from(f), self(r) {}

    Route() : src(0), dst(0), from(0), self(0) {}

  };


  struct NodeInfo {
    int id;
    int degree;
    double nodeMetric;

    NodeInfo() : id(0), degree(0), nodeMetric(0) {}

    NodeInfo(int i, int d, double m) : id(i), degree(d), nodeMetric(m) {}

  };


  struct AddressEntry {
    int nodeFrom;
    int nodeTo;
    Address addr;
  };


  enum MetricSetting : int {
    DELAY_ONLY,
    DELAY_BW_BALANCE
  }; 


  enum OverlayGenerateMethod : int {
    SHORTEST_PATH_TREE,
    SEQUENCIAL_AWARE
  };


  enum ChooseCoreAlgorithm : int {
    ALL,
    SPECTRAL_CLUSTERING
  };


  enum BroadwidthModel : uint8_t {
    LINK,
    NODE_P,
    NODE_S
  };


  enum PacketRouteLevel : uint8_t {
    LARGE_PACKET,
    SMALL_PACKET
  };


  enum LatencyOptPartiPoint : uint8_t {
    LOPP_LAST,
    LOPP_90,
    LOPP_70
  };

  // end of definations of structures and enums 



  // begin public methods

  BlockChainTopologyHelper(int n);

  ~BlockChainTopologyHelper();


  // import topology data

  void insertLinkInfo(int from, int to, double delay, int bw);


  // change settings

  void setupPBFTApp(PBFTCorrectHelper& pbft);

  void setAddressHelper(Ipv4AddressHelper& addressHelper);

  void setNodeBw(int bw);

  void setOverlayRoute();

  void setLinkMetricDefination(int m);

  void setTopologyGenerationMethod1(int m);
  void setTopologyGenerationMethod2(int m);

  void setChooseCoreMethod(int m);

  void setBroadwidthModel(int m);

  void setClusterN(int n);

  void setupPeerMetric();

  void setMessageSize(int s);  // bytes


  // fire and get results

  void installLink(); 

  ApplicationContainer getApp() {return installedApps;}


private:

  // number of nodes

  int nodeN;


  // begin configs & their defaults

  // bytes
  int averageMessageSize = 0;

  //bytes
  const int smallMessageSize = 80;

  // bps
  int nodeBw = 1; 

  int linkMetricSetting = DELAY_ONLY;

  int topologyGenerateMethod1 = SHORTEST_PATH_TREE;
  int topologyGenerateMethod2 = SHORTEST_PATH_TREE;

  int chooseCore = SPECTRAL_CLUSTERING;

  int broadwidthModel = NODE_S;

  int clusterN = 3;

  int latencyOptPartiPoint = LOPP_LAST;

  int partiPointLast, partiPoint90, partiPoint70;

  std::vector<std::tuple<int, int , int> > excludedLinks;

  // end configs & their defaults


  // begin containers
  
  std::vector<LinkInfo> allLinkInfo; 
  std::vector<NodeInfo> allNodeInfo; 

  std::vector<NetDeviceContainer> allLink;
  std::vector<Ipv4InterfaceContainer> allInterface;
  Ipv4AddressHelper address;
  NodeContainer nodes;
  ApplicationContainer installedApps;
  std::vector<AddressEntry> addressBook;
  PointToPointHelper p2p;

  std::vector<int> coreNodeList;

  // first index is node id 
  std::vector<std::set<Route> > tempRouteTable;

  // first key is root id, second is node id, order of Route matters
  std::map<int, std::vector<std::vector<Route> > > routeTable;

  // first index is root id, second is node id
  // <0> depth,
  // <1> delay from prev start sending to finish all,
  // <2> delay from self start sending to finish all
  // <3> and delay from root to this node
  // <4> # of nodes in its subtree
  std::map<int, std::vector<std::tuple<int, double, double, double, int> > > sequentialMetricTable;

  // make typename shorter
  typedef std::pair<int, double> metricCase;

  // root id, <worst node id, worst delay> , 90, 70
  std::map<int, std::tuple<metricCase, metricCase, metricCase> > treeMetric;

  // <root id, depth>
  std::map<int, int> treeDepth;

  // end containers


  // begin private methods


  // gather node info from link info
  // call after topology is loaded from external sources 
  void setupNodeInfo();

  void setupAppPeer();

  // choose core nodes wrt topology and assign the result to coreNodeList
  void chooseCoreNodes();

  void installCoreList();

  void installRouteTable(int level);

  // overlay algorithm

  // one layer plain
  void generateOverlayRoute_PLAIN();

  // shortest path
  void generateOverlayRoute_SPT();

  // transfer sequence aware
  void generateOverlayRoute_SA();

  int checkStability(int source, int node);

  void changePrev(int source, int changer, int new_prev);


  // getters

  Address findAddress(int from, int to);

  int getSequencialOrder(int src, int prev_node, int node);

  int getChildNumber(int src, int node);

  int getDegree(int i);

  int getLink(int a, int b, LinkInfo &glink);

  bool hasLink(int a, int b);

  double getNodeTransferDelay(int a, int b);

  double getLinkDelay(int a, int b);

  int getPrev(int root, int node);

  std::pair<int, double> getWorstMetric(int root);


  // invariant maintainers

  // re-caculate link metric according to current load 
  void updateLinkMetric();

  // re-caculate node metric according to current load 
  void updateNodeMetric();

  void IncLinkShare(int a, int b);
  void DecLinkShare(int a, int b);

  void updateSequencialMetric();

  void updateDepth();

  void updateTempRouteVector(std::vector<int> &P, int source);
  
  void updateRouteVector();

  void clearState();


  // utils

  bool compareWRTSubtreeMetric(const Route &a, const Route &b);
  void utilSP(std::vector<double> &D, std::vector<int> &P, int source);
  void breadthFirstTopdownWalk(int root, int start, std::function<bool(int)> f);
  
  void peekTreeDebug(int root, int node, int level = 0); 

  void peekSubtreeWeightDistribution(int root); 

  // end private methods

};

} // namespace ns3
#endif