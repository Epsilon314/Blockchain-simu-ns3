#include "myBriteTopologyHelper.h"

namespace ns3 {

MyBriteTopologyHelper::MyBriteTopologyHelper(std::string confFile, std::string storedFile) {
  
  m_confFile = confFile;
  m_storedTopologyFile = storedFile;
  m_topology = NULL;
  m_numNodes = 0;
  m_numEdges = 0;

  m_uv = CreateObject<UniformRandomVariable> ();

}

void MyBriteTopologyHelper::SetRestore(bool restore) {
  restored = restore;
}

MyBriteTopologyHelper::~MyBriteTopologyHelper() {
  delete m_topology;
}

void MyBriteTopologyHelper::AssignStreams(int64_t streamNumber) {
  m_uv->SetStream (streamNumber);
}

void MyBriteTopologyHelper::ExportEdgeInfo() {
  std::ofstream edgeInfoFile;

  edgeInfoFile.open (m_storedTopologyFile, std::ios_base::out | std::ios_base::trunc);

  NS_ASSERT (!edgeInfoFile.fail());

  for (auto edge : m_briteEdgeInfoList) {
    edgeInfoFile << edge.edgeId << " " << edge.srcId << " "
                 << edge.destId << " " << edge.length << " "
                 << edge.delay << " " << edge.bandwidth << std::endl;
  }
  edgeInfoFile.close();
}

MyBriteTopologyHelper::BriteEdgeInfoList MyBriteTopologyHelper::getEdgeInfo() {
  return m_briteEdgeInfoList;
}

void MyBriteTopologyHelper::BuildBriteNodeInfoList(void) {

  brite::Graph *g = m_topology->GetGraph ();

  for (int i = 0; i < g->GetNumNodes(); ++i) {
    
    BriteNodeInfo nodeInfo;
    nodeInfo.nodeId = g->GetNodePtr (i)->GetId ();
    nodeInfo.xCoordinate = g->GetNodePtr(i)->GetNodeInfo ()->GetCoordX ();
    nodeInfo.yCoordinate = g->GetNodePtr(i)->GetNodeInfo ()->GetCoordY ();
    nodeInfo.inDegree = g->GetNodePtr(i)->GetInDegree ();
    nodeInfo.outDegree = g->GetNodePtr(i)->GetOutDegree ();

    m_briteNodeInfoList.push_back(nodeInfo);
  }

}

void MyBriteTopologyHelper::BuildBriteEdgeInfoList(void) {

  brite::Graph *g = m_topology->GetGraph ();
  std::list<brite::Edge*>::iterator el;
  std::list<brite::Edge*> edgeList = g->GetEdges ();

  for (el = edgeList.begin (); el != edgeList.end (); el++) {
    
    BriteEdgeInfo edgeInfo;
    edgeInfo.edgeId = (*el)->GetId ();
    edgeInfo.srcId = (*el)->GetSrc ()->GetId ();
    edgeInfo.destId = (*el)->GetDst ()->GetId ();
    edgeInfo.length = (*el)->Length ();

    switch ((*el)->GetConf ()->GetEdgeType ()) {
      case brite::EdgeConf::RT_EDGE:
        edgeInfo.delay = ((brite::RouterEdgeConf*)((*el)->GetConf ()))->GetDelay ();
        edgeInfo.bandwidth = (*el)->GetConf ()->GetBW ();
        break;

      case brite::EdgeConf::AS_EDGE:
        edgeInfo.delay =  0;     /* No delay for AS Edges */
        edgeInfo.bandwidth = (*el)->GetConf ()->GetBW ();
        break;
    }
    m_briteEdgeInfoList.push_back (edgeInfo);
  }
}


void MyBriteTopologyHelper::GenerateBriteTopology(void) {

  if (restored) {
    std::ifstream storedEdgeInfoFile(m_storedTopologyFile);
    std::string egdeInfo;
    
    NS_ASSERT (!storedEdgeInfoFile.fail());

    char delimiter = ' ';
    
    if (storedEdgeInfoFile.is_open()) {
      while(std::getline(storedEdgeInfoFile, egdeInfo)) {
        
        BriteEdgeInfo edge;

        std::string token;
        std::vector<std::string> tokens;

        std::stringstream edgeInfoStream(egdeInfo);

        while (std::getline(edgeInfoStream, token, delimiter)) {
          tokens.push_back(token);
        }

        edge.edgeId = std::stoi(tokens[0]);
        edge.srcId = std::stoi(tokens[1]);
        edge.destId = std::stoi(tokens[2]);
        edge.length = std::stod(tokens[3]);
        edge.delay = std::stod(tokens[4]);
        edge.bandwidth = std::stod(tokens[5]);

        m_briteEdgeInfoList.push_back(edge);

        }
      storedEdgeInfoFile.close();
    }
  }

  else {

    //check to see if need to generate seed file
    bool generateSeedFile = m_seedFile.empty ();

    if (generateSeedFile) {
      std::ofstream seedFile;

      //overwrite file if already there
      seedFile.open ("briteSeedFile.txt", std::ios_base::out | std::ios_base::trunc);

      //verify open
      NS_ASSERT (!seedFile.fail ());

      //Generate seed file expected by BRITE
      //need unsigned shorts 0-65535
      seedFile << "PLACES " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile << "CONNECT " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile << "EDGE_CONN " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile << "GROUPING " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile << "ASSIGNMENT " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile << "BANDWIDTH " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << " " << m_uv->GetInteger (0, 65535) << std::endl;
      seedFile.close ();

      //if we're using NS3 generated seed files don't want brite to create a new seed file.
      m_seedFile = m_newSeedFile = "briteSeedFile.txt";
    }

    brite::Brite br (m_confFile, m_seedFile, m_newSeedFile);
    m_topology = br.GetTopology ();
    BuildBriteNodeInfoList ();
    BuildBriteEdgeInfoList ();

    //brite automatically spits out the seed values used to a separate file so no need to keep this anymore
    if (generateSeedFile) {
      remove ("briteSeedFile.txt");
    }


  }

}

} //namespace ns3