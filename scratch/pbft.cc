/*
Yiqing Zhu
yiqing.zhu.314@gmail.com
**/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/brite-module.h"
#include "ns3/random-variable-stream.h"

#include <sys/stat.h>
#include <iostream>
#include <set>
#include <random>
#include <chrono>

using namespace ns3;

std::vector<std::vector<int> > captureHistory;

// cite from Decentralization in Bitcoin and Ethereum Networks
std::array<std::array<double, 2>, 4> bandwidthDistribution_BitcoinV6 = {{   
                                                                        // {11, 0.1},      // too low, not suitable. Will freeze kademlia
                                                                        // {45.2, 0.33},   
                                                                        {78.2, 0.5},
                                                                        {94.3, 0.67},
                                                                        {207.9, 0.9},
                                                                        {300, 1}
                                                                    }}; 


void globalView(ApplicationContainer apps, int n) {
    std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
    for (int i = 0; i < n; ++i) {
        Ptr<PBFTCorrect> app = apps.Get(i)->GetObject<PBFTCorrect>();
        int nodeid = app->getNodeId(), round = app->getRound(), stage = app->getStage()
            ,prepareVote = app->getPrepareCount(), commitVote = app->getCommitCount(), 
            trustedPrimary = app->getPrimary(), blameVote = app->getBlameCount(), 
            replayVote = app->getReplyCount(), newEpochVote = app->getNewEpochCount();
        std::vector<int> status = {nodeid, round, stage};
        captureHistory.push_back(status);
        std::cout << "Node: " << nodeid << " round: " << round << " stage: " << stage
            << " prepareVote: " << prepareVote << " commitVote: " << commitVote << " replayVote: "
            << replayVote << " blameVote: " << blameVote << " newEpochVote: " << newEpochVote <<
            " trusted primary: " << trustedPrimary << std::endl;
    }
    std::cout << std::endl;
}


void printAverageLatency(Ptr<PBFTCorrect> app, bool full = true) {
    if (full) {
        std::cout << "Node: " << app->getNodeId() << " average latency: " << app->getAverageLatency() << std::endl;
    }
    else {
        std::cout << app->getAverageLatency() << ", ";
    }
}


void printActiveRate(Ptr<PBFTCorrect> app) {
  std::cout << "node " << app->getNodeId() << " active rate: " << app->getActiveRate() << std::endl; 
}


void printStress(Ptr<PBFTCorrect> app, bool full = true) {
  if (full) {
    // remember that block size and bandwidth has a *1000 ratio 
    std::cout << "node " << app->getNodeId() << " stress: " << app->getNodeStress() * 1000 << std::endl;
  }
  else {
    std::cout << app->getNodeStress() * 1000 << ", ";
  }
}


void printLinkStress(Ptr<PBFTCorrect> app) {
    auto link_stress = app->getLinkStress();
    for (auto s : link_stress) {
        // remember that block size and bandwidth has a 1000 ratio 
        std::cout << "link: " << s.first.first << "," << s.first.second << " stress: " << s.second * 1000 << std::endl;
    }
    std::cout << std::endl;
}


int main(int argc, char *argv[])    {

    // LogComponentEnable("PBFTCorrect", LOG_LEVEL_INFO);
    // LogComponentEnable("BlockChainApplicationBase", LOG_LEVEL_INFO);

    // using large payload size can significantly slow down the simulation and may cause out of memory
    // shrink payload size and b.w togethor 1000 times to avoid it
    int payloadLen = 500;        // k byte

	CommandLine cmd;
	cmd.AddValue(
		"l",
		"payload length",
		payloadLen
	);
	cmd.Parse(argc,argv);

    enum NETMODEL {
        LOCAL,
        FULLC,
        BRITE,
        GEO_SIMU
    };

    int net = GEO_SIMU;

    NodeContainer nodes;

    uint32_t stableCount = 100;
    uint32_t churnCount = 0;
    uint32_t nodesCount = stableCount + churnCount;

    

    double timeout = 100.0;

    int totalDataRate = 300000; // bps *1000 ratio to speed up simulation

    // double delay = 0.0001;

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0","255.255.255.0");

    BlockChainTopologyHelper topologyHelper(stableCount, churnCount);

    switch (net) {
        case BRITE:
        {
            std::string confFile = "/home/y1qin9zhu/Documents/ns3/ns-allinone-3.30.1/ns-3.30.1/myResults/blockchain-sim/RTWaxman.conf";
            std::string storedFile = "/home/y1qin9zhu/Documents/ns3/ns-allinone-3.30.1/ns-3.30.1/myResults/blockchain-sim/storedTopology";

            struct stat buffer;
            bool storedFileExist = stat(storedFile.c_str(), &buffer) == 0;

            MyBriteTopologyHelper bth(confFile, storedFile);

            if (storedFileExist) {
                bth.SetRestore(true);
            }
            else {
                bth.SetRestore(false);
            }
            bth.AssignStreams(3);
            bth.GenerateBriteTopology();
            bth.ExportEdgeInfo();

            MyBriteTopologyHelper::BriteEdgeInfoList edgeInfoList = bth.getEdgeInfo();

            for (auto link : edgeInfoList) {
                //std::cout<<link.srcId<<" "<<link.destId<<std::endl;
                topologyHelper.insertLinkInfo(link.srcId, link.destId, link.delay, link.bandwidth * 1000000);
            }
        }
        break;
        
        case GEO_SIMU:
        {            
            std::string confFile = "/home/y1qin9zhu/Documents/ns3/ns-allinone-3.30.1/ns-3.30.1/src/applications/ping-data.json";

            GeoSimulationTopologyHelper geo(confFile);

            // std::set<uint32_t> cities;
            // see ping-data.json for city number, for now

            // for (uint32_t i: selected) cities.insert(i);
            // geo.setSelectedCity(cities);

            Ptr<EmpiricalRandomVariable> bandwidthEmpirical = CreateObject<EmpiricalRandomVariable> ();

            for (auto i: bandwidthDistribution_BitcoinV6) {
                bandwidthEmpirical->CDF(i[0], i[1]);
            }

            std::vector<GeoSimulationTopologyHelper::DelayInfo> links = geo.getClique(nodesCount);

            NS_ASSERT(nodesCount == geo.getCliqueCityList().size());

            for (auto link : links) {
                auto bandwidth = (int) (bandwidthEmpirical->GetValue() * 1000000);

                // std::cout<< link.noFrom << " " << link.noTo << " " << link.avgDelay << ";" ;
                topologyHelper.insertLinkInfo(link.noFrom, link.noTo, link.avgDelay, bandwidth);
            }
            std::cout << std::endl;
        }
        break;
    
    default:
        break;
    }

    PBFTCorrectHelper pbfthelper = PBFTCorrectHelper(nodesCount, timeout);
    pbfthelper.SetVoteNodes(80);
    pbfthelper.SetBlockSz(payloadLen);
    // header length 42, fixme
    // double estimatedDelay = (payloadLen + 42) * 8.0 / (double)totalDataRate + delay;
    
    // fixme, this parameter tell the app to slow down a bit
    pbfthelper.SetDelay(0);
    
    // FLOOD CORE_RELAY MIXED  INFECT_UPON_CONTAGION
    pbfthelper.SetTransType(ConsensusMessageBase::CORE_RELAY);

    pbfthelper.SetFloodN(1);
    pbfthelper.SetTTL(0);

    pbfthelper.SetTransferModel(BlockChainApplicationBase<PBFTMessage>::SEQUENCIAL);

    pbfthelper.SetFloodRandomization(true);
    
    
    pbfthelper.SetContinous(true);

    pbfthelper.SetOutboundBandwidth((double)totalDataRate); 
    pbfthelper.setBroadcastDuplicateCount(1);

    topologyHelper.setupPBFTApp(pbfthelper);
    topologyHelper.setAddressHelper(address);
    topologyHelper.setNodeBw(totalDataRate);
    topologyHelper.setMessageSize(payloadLen * 1000);
    
    // SHORTEST_PATH_TREE, SEQUENCIAL_AWARE, DISTRIBUTED, KADCAST
    topologyHelper.setTopologyGenerationMethod1(BlockChainTopologyHelper::DISTRIBUTED); 
    topologyHelper.setTopologyGenerationMethod2(BlockChainTopologyHelper::DISTRIBUTED);
    
    topologyHelper.setBroadwidthModel(BlockChainTopologyHelper::CAPPED_BY_NODE);

    topologyHelper.installLink();

    topologyHelper.setLinkMetricDefination(BlockChainTopologyHelper::DELAY_BW_BALANCE);
    topologyHelper.setChooseCoreMethod(BlockChainTopologyHelper::SPECTRAL_CLUSTERING);   // ALL  SPECTRAL_CLUSTERING
    topologyHelper.setClusterN(6);

    topologyHelper.setOverlayRoute();
    topologyHelper.installShorestPath();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::cout << "Topology build done." << std::endl;

    ApplicationContainer pbftNodes = topologyHelper.getApp();

    auto coreNodes = topologyHelper.getCoreNode();

    std::vector<int> joinTarget;
    for (auto i = 0; i < (int) stableCount; ++i) joinTarget.push_back(i);
    unsigned join_seed = std::chrono::system_clock::now().time_since_epoch().count();
    shuffle(joinTarget.begin(), joinTarget.end(), std::default_random_engine(join_seed));

    std::vector<double> churnTime;
    std::uniform_real_distribution<double> unif(50.0, 95.0);
    auto re_engine = std::default_random_engine(join_seed);

    for (auto i = 0; i < (int) churnCount; ++i) {
        churnTime.push_back(unif(re_engine));
    }
    
    for (auto i = 0; i < (int) churnCount; ++i) {
        std::cout << churnTime[i] << ' ';
    }

    for (auto core_idx: coreNodes) {
        // auto core_node = pbftNodes.Get(core_idx)->GetObject<PBFTCorrect>();
        for (int i = (int) stableCount; i < (int) nodesCount; ++i) {
            // Ptr<PBFTCorrect> churn = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
            
            int join = joinTarget[(core_idx + i) % stableCount];
            Ptr<PBFTCorrect> join_n = pbftNodes.Get(join)->GetObject<PBFTCorrect>();

            join_n->changeStatus(PBFTCorrect::CRASH);

            std::cout << "schedule accept peer: " << join << ' ' << i << ' ' << core_idx << ' '
                << topologyHelper.findAddress(core_idx, i) << std::endl;


            Simulator::Schedule(Seconds(5.0), &PBFTCorrect::acceptPeer, join_n, 
                i,
                core_idx,
                topologyHelper.findAddress(core_idx, i)
            );
        }
    }

    for (int i = stableCount; i < (int) nodesCount; ++i) {
        auto churn_node = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
        for (int j = 0; j < (int) nodesCount; ++j) {
            if (j == i) continue;
            Simulator::Schedule(Seconds(4.9), &PBFTCorrect::joinPeer, churn_node,
                j,
                topologyHelper.findAddress(i, j)
            );
        }
    }

    for (int i = stableCount; i < (int) nodesCount; ++i) {
        auto churn_node = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
        Simulator::Schedule(Seconds(churnTime[i - stableCount]), &PBFTCorrect::disablePeer, churn_node);
    }
    
    std::cout << "Churn node schedule done" << std::endl;

    int crash_count = 15;
    std::vector<int> crash_nodes;
    
    for (int i = 0; i < (int) stableCount; ++i) {
        crash_nodes.push_back(i);
    }

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

    std::shuffle(crash_nodes.begin(), crash_nodes.end(), std::default_random_engine(seed));

    std::cout << "Sampling crash nodes: ";


    for (int i = 0, j = 0; j < crash_count; ++i) {

        int id = crash_nodes[i];

        if (id == 10 || std::find(coreNodes.begin(), coreNodes.end(), id) != coreNodes.end()) continue;
        
        Ptr<PBFTCorrect> crashed = pbftNodes.Get(id)->GetObject<PBFTCorrect>();

        // Simulator::Schedule(Seconds(10), &PBFTCorrect::disablePeer, crashed);

        Simulator::Schedule(Seconds(50), &PBFTCorrect::changeStatus, crashed,
            BlockChainApplicationBase<PBFTMessage>::CRASH);

        ++j;
        
        std::cout << id << ' ';
    }
    std::cout << " Done" << std::endl;

    Ptr<PBFTCorrect> client = pbftNodes.Get(10)->GetObject<PBFTCorrect>();
    
    
    // Simulator::Schedule(Seconds(0.1), &PBFTCorrect::BroadcastTest, client);
    Simulator::Schedule(Seconds(0.1), &PBFTCorrect::sendRequestCircle, client, 100.0);

    for (double time = 1; time < 100; ++time) {
        Simulator::Schedule(Seconds(time), &globalView, pbftNodes, nodesCount);
    }

    // pointToPoint.EnablePcapAll("pbft", false);

    for (u_int32_t i = 0; i < nodesCount; ++i) {
        Ptr<PBFTCorrect> node = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
        Simulator::Schedule(Seconds(98), printStress, node, true);
        Simulator::Schedule(Seconds(98), printAverageLatency, node, true);
        Simulator::Schedule(Seconds(98), printActiveRate, node);
        Simulator::Schedule(Seconds(98), printLinkStress, node);
    }

    Simulator::Schedule(Seconds(98), [](){std::cout << "<AvgLatency: ";});
    for (uint32_t i = 0; i < nodesCount; ++i) {
        Ptr<PBFTCorrect> node = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
        Simulator::Schedule(Seconds(98), printAverageLatency, node, false);
    }
    Simulator::Schedule(Seconds(98), [](){std::cout << ">" << std::endl;});


    Simulator::Schedule(Seconds(98), [](){std::cout << "<NodeStress: ";});
    for (uint32_t i = 0; i < nodesCount; ++i) {
        Ptr<PBFTCorrect> node = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
        Simulator::Schedule(Seconds(98), printStress, node, false);
    }
    Simulator::Schedule(Seconds(98), [](){std::cout << ">" << std::endl;});
    
    
    //AsciiTraceHelper ascii;
    //pointToPoint.EnableAsciiAll(ascii.CreateFileStream("pbft.tr"));
    pbftNodes.Start(Seconds(0));
    pbftNodes.Stop(Seconds(100));

    Simulator::Run();
  	Simulator::Destroy();

    return 0;
}
