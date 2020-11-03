#ifndef MYBRITETOPOLOGYHELPER_H
#define MYBRITETOPOLOGYHELPER_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "ns3/random-variable-stream.h"

#include "Brite.h"


namespace ns3 {

// a reduced and modified version of brite-topology-helper
// since the original api does not suit my need 

class MyBriteTopologyHelper {

public:

  struct BriteNodeInfo
  {
    int nodeId;
    double xCoordinate;
    double yCoordinate;
    int inDegree;
    int outDegree;
  };

  struct BriteEdgeInfo
  {
    int edgeId;
    int srcId;
    int destId;
    double length;
    double delay;
    double bandwidth;
  };

  typedef std::vector<BriteNodeInfo> BriteNodeInfoList;
  typedef std::vector<BriteEdgeInfo> BriteEdgeInfoList;

  MyBriteTopologyHelper(std::string confFile, std::string storedFile);

  ~MyBriteTopologyHelper();

  void AssignStreams(int64_t streamNumber);

  void GenerateBriteTopology(void);

  BriteEdgeInfoList getEdgeInfo();

  void ExportEdgeInfo();

  void SetRestore(bool restore);

private:

  void BuildBriteNodeInfoList(void);
  void BuildBriteEdgeInfoList(void);

  static const int mbpsToBps = 1000000;

  /// brite configuration file to use
  std::string m_confFile;

  /// brite seed file to use
  std::string m_seedFile;

  /// brite seed file to generate for next run
  std::string m_newSeedFile;

  std::string m_storedTopologyFile;

  /// the Brite topology
  brite::Topology* m_topology;

  /// stores the number of nodes created in the BRITE topology
  uint32_t m_numNodes;

  /// stores the number of edges created in the BRITE topology
  uint32_t m_numEdges;

  BriteNodeInfoList m_briteNodeInfoList;
  BriteEdgeInfoList m_briteEdgeInfoList;

  /// random variable stream for brite seed file
  Ptr<UniformRandomVariable> m_uv;

  bool restored = false;

};

} //namespace ns3
#endif