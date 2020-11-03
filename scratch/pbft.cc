#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/brite-module.h"
#include <sys/stat.h>
#include <iostream>
#include <set>

using namespace ns3;

std::vector<std::vector<int> > captureHistory;

void globalView(ApplicationContainer apps, int n) {
    for (int i = 0; i < n; ++i) {
        Ptr<PBFTCorrect> app = apps.Get(i)->GetObject<PBFTCorrect>();
        int nodeid = app->getNodeId(), round = app->getRound(), stage = app->getStage()
            ,prepareVote = app->getPrepareCount(), commitVote = app->getCommitCount(), 
            trustedPrimary = app->getPrimary();
        std::vector<int> status = {nodeid, round, stage};
        captureHistory.push_back(status);
        std::cout << "Node: " << nodeid << " round: " << round << " stage: " << stage
            << " prepare vote: " << prepareVote << " commitVote: " << commitVote 
            << " trusted primary: " << trustedPrimary << std::endl;
    }
    std::cout << std::endl;
}

int main(int argc, char *argv[])
{

    // LogComponentEnable("PBFTCorrect", LOG_LEVEL_INFO);
    // LogComponentEnable("BlockChainApplicationBase", LOG_LEVEL_INFO);

    // using large payload size can lead to memory outage
    // shrink payload size and b.w togethor to avoid it
    int payloadLen = 500;        // byte

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

    int net = BRITE;

    NodeContainer nodes;


    if (net == LOCAL) {

        // csma bus
        uint32_t nodesCount = 20;
        double timeout = 20.0;
        int totalDataRate = 100000000; // 100 Mbps

            
        nodes.Create(nodesCount);

        CsmaHelper csma;
        csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(totalDataRate)));
        csma.SetChannelAttribute("Delay", StringValue("0.01ms"));
        NetDeviceContainer csmaDevices;
        csmaDevices.Add(csma.Install(nodes));

        InternetStackHelper stack;
        stack.Install(nodes);
        Ipv4AddressHelper address;
        address.SetBase("10.0.0.0","255.255.0.0");
        Ipv4InterfaceContainer interfaces = address.Assign(csmaDevices);
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    

        PBFTCorrectHelper pbfthelper = PBFTCorrectHelper(nodesCount, timeout);
        pbfthelper.SetBlockSz(payloadLen);
        pbfthelper.SetDelay(0.001);

        ApplicationContainer pbftNodes = pbfthelper.Install(nodes);

        for (uint32_t i = 0; i < nodesCount; ++i) {
            Ptr<PBFTCorrect> app = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
            for (uint32_t j = 0; j < nodesCount; ++j) {
                if (j != i) {
                    Ptr<PBFTCorrect> peerApp = pbftNodes.Get(j)->GetObject<PBFTCorrect>();
                    app->AddPeer(peerApp->getNodeId(), interfaces.GetAddress(j)); 
                }
            }
        }

        pbftNodes.Start(Seconds(0));
        pbftNodes.Stop(Seconds(100));

        Ptr<PBFTCorrect> client = pbftNodes.Get(1)->GetObject<PBFTCorrect>();
        
        Simulator::Schedule(Seconds(0.1), &PBFTCorrect::sendRequestCircle, client, 100.0);

        //csma.EnablePcapAll("pbft", false);

        //AsciiTraceHelper ascii;
        //csma.EnableAsciiAll(ascii.CreateFileStream("pbft.tr"));

    }

    if (net == FULLC) {

        // full connect
        
        uint32_t nodesCount = 20;
        double timeout = 20.0;
        int totalDataRate = 100000000; // 100 Mbps
        int dataRate = (int) totalDataRate / (nodesCount - 1);

        
        nodes.Create(nodesCount);
        
        PointToPointHelper pointToPoint;

        InternetStackHelper stack;
        stack.Install(nodes);

        Ipv4AddressHelper address;
        address.SetBase("10.0.0.0","255.255.255.0");

        Ipv4InterfaceContainer interfaces;

        pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue(DataRate(dataRate)));
        pointToPoint.SetChannelAttribute ("Delay", StringValue ("5ms"));

        NetDeviceContainer allDevices;

        for (uint32_t i = 0; i < nodesCount - 1; ++i) {
            for (uint32_t j = i + 1; j < nodesCount; ++j) {

                NetDeviceContainer newPair = pointToPoint.Install(nodes.Get(i), nodes.Get(j));
                allDevices.Add(newPair);
                interfaces.Add(address.Assign(newPair));
                address.NewNetwork();
            }
        }
        
        PBFTCorrectHelper pbfthelper = PBFTCorrectHelper(nodesCount, timeout);
        pbfthelper.SetBlockSz(payloadLen);

        double estimatedDelay = (payloadLen + 82) * 8.0 / dataRate + 0.005;

        pbfthelper.SetDelay(estimatedDelay * 2.0);

        ApplicationContainer pbftNodes = pbfthelper.Install(nodes);


        for (uint32_t i = 0; i < nodesCount; ++i) {
            Ptr<PBFTCorrect> app = pbftNodes.Get(i)->GetObject<PBFTCorrect>();
            for (uint32_t j = 0; j < nodesCount; ++j) {
                if (j != i) {
                    Ptr<PBFTCorrect> peerApp = pbftNodes.Get(j)->GetObject<PBFTCorrect>();
                    
                    uint32_t interfaceIdx;
                    /**
                     * just get the right index
                     */
                    if (i < j) {
                        interfaceIdx = (2*nodesCount-i-1)*i+(j-i)*2-1;
                    }
                    else {
                        interfaceIdx = (2*nodesCount-j-1)*j+(i-j)*2-2;
                    }

                    app->AddPeer(peerApp->getNodeId(), interfaces.GetAddress(interfaceIdx)); 
                }
            }
        }

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        pbftNodes.Start(Seconds(0));
        pbftNodes.Stop(Seconds(100));

        Ptr<PBFTCorrect> client = pbftNodes.Get(1)->GetObject<PBFTCorrect>();
        
        Simulator::Schedule(Seconds(0.1), &PBFTCorrect::sendRequestCircle, client, 100.0);

        //pointToPoint.EnablePcapAll("pbft", false);

        //AsciiTraceHelper ascii;
        //pointToPoint.EnableAsciiAll(ascii.CreateFileStream("pbft.tr"));

    }

    if (net == BRITE) {

        uint32_t nodesCount = 50;
        double timeout = 100.0;
        int totalDataRate = 50000; // bps

        // double delay = 0.0001;

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

        Ipv4AddressHelper address;
        address.SetBase("10.0.0.0","255.255.255.0");

        BlockChainTopologyHelper topologyHelper(nodesCount);

        PBFTCorrectHelper pbfthelper = PBFTCorrectHelper(nodesCount, timeout);
        pbfthelper.SetBlockSz(payloadLen);
        // header length 42, fixme
        // double estimatedDelay = (payloadLen + 42) * 8.0 / (double)totalDataRate + delay;
        
        // fixme, this parameter tell the app to slow down a bit
        pbfthelper.SetDelay(0);
        
        pbfthelper.SetTransType(BlockChainApplicationBase::CORE_RELAY);

        pbfthelper.SetFloodN(1);
        pbfthelper.SetTTL(0);

        pbfthelper.SetTransferModel(BlockChainApplicationBase::SEQUENCIAL);

        pbfthelper.SetFloodRandomization(true);
        
        
        pbfthelper.SetContinous(true);

        MyBriteTopologyHelper::BriteEdgeInfoList edgeInfoList = bth.getEdgeInfo();

        for (auto link : edgeInfoList) {
            //std::cout<<link.srcId<<" "<<link.destId<<std::endl;
            topologyHelper.insertLinkInfo(link.srcId, link.destId, link.delay, link.bandwidth * 1000000);
        }

        topologyHelper.setupPBFTApp(pbfthelper);
        topologyHelper.setAddressHelper(address);
        topologyHelper.setNodeBw(totalDataRate);
        topologyHelper.setMessageSize(payloadLen);

        topologyHelper.setTopologyGenerationMethod1(BlockChainTopologyHelper::SEQUENCIAL_AWARE);
        topologyHelper.setTopologyGenerationMethod2(BlockChainTopologyHelper::SEQUENCIAL_AWARE);
        topologyHelper.setBroadwidthModel(BlockChainTopologyHelper::NODE_S);


        topologyHelper.installLink();

        topologyHelper.setLinkMetricDefination(BlockChainTopologyHelper::DELAY_BW_BALANCE);
        topologyHelper.setChooseCoreMethod(BlockChainTopologyHelper::SPECTRAL_CLUSTERING);
        topologyHelper.setClusterN(5);

        topologyHelper.setOverlayRoute();

        ApplicationContainer pbftNodes = topologyHelper.getApp();

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        pbftNodes.Start(Seconds(0));
        pbftNodes.Stop(Seconds(100));

        Ptr<PBFTCorrect> crashed1 = pbftNodes.Get(8)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed2 = pbftNodes.Get(18)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed3 = pbftNodes.Get(20)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed4 = pbftNodes.Get(22)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed5 = pbftNodes.Get(23)->GetObject<PBFTCorrect>();

        Ptr<PBFTCorrect> crashed6 = pbftNodes.Get(24)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed7 = pbftNodes.Get(25)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed8 = pbftNodes.Get(26)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed9 = pbftNodes.Get(27)->GetObject<PBFTCorrect>();
        Ptr<PBFTCorrect> crashed10 = pbftNodes.Get(28)->GetObject<PBFTCorrect>();
  
        
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed1,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed2,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed3,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed4,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed5,
            BlockChainApplicationBase::CRASH);
        
        Simulator::Schedule(Seconds(10), &PBFTCorrect::changeStatus, crashed6,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(10), &PBFTCorrect::changeStatus, crashed7,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(10), &PBFTCorrect::changeStatus, crashed8,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed9,
            BlockChainApplicationBase::CRASH);
        Simulator::Schedule(Seconds(15), &PBFTCorrect::changeStatus, crashed10,
            BlockChainApplicationBase::CRASH);        

        Ptr<PBFTCorrect> client = pbftNodes.Get(10)->GetObject<PBFTCorrect>();
        Simulator::Schedule(Seconds(0.1), &PBFTCorrect::sendRequestCircle, client, 100.0);


        Simulator::Schedule(Seconds(1), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(10), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(14), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(16), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(20), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(30), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(50), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(70), &globalView, pbftNodes, nodesCount);
        Simulator::Schedule(Seconds(90), &globalView, pbftNodes, nodesCount);
        // pointToPoint.EnablePcapAll("pbft", false);

        //AsciiTraceHelper ascii;
        //pointToPoint.EnableAsciiAll(ascii.CreateFileStream("pbft.tr"));

    }

    if (net == GEO_SIMU) {
        
        uint32_t nodesCount = 32;
        double timeout = 20.0;
        int totalDataRate = 50000; // 100 Mbps

        std::string confFile = "/home/y1qin9zhu/Documents/ns3/ns-allinone-3.30.1/ns-3.30.1/src/applications/ping-data.json";

        GeoSimulationTopologyHelper geo(confFile);

        std::set<uint32_t> cities;
        // see ping-data.json for city number, for now
        for (uint32_t i = 0; i < 50; ++i) cities.insert(i);
        geo.setSelectedCity(cities);

        std::vector<GeoSimulationTopologyHelper::DelayInfo> links = geo.parse();

        nodesCount = geo.getCityList().size();
        std::cout << nodesCount << std::endl;

        Ipv4AddressHelper address;
        address.SetBase("10.0.0.0","255.255.255.0");

        BlockChainTopologyHelper topologyHelper(nodesCount);

        PBFTCorrectHelper pbfthelper = PBFTCorrectHelper(nodesCount, timeout);
        pbfthelper.SetBlockSz(payloadLen);
        // header length 42
        double estimatedDelay = (payloadLen + 42) * 8.0 / totalDataRate + 0.1;
        pbfthelper.SetDelay(estimatedDelay);
        
        pbfthelper.SetTransType(BlockChainApplicationBase::MIXED);

        pbfthelper.SetFloodN(2);
        pbfthelper.SetTTL(3);

        pbfthelper.SetFloodRandomization(true);
        
        
        //pbfthelper.SetContinous(true);

        

        for (auto link : links) {

            //std::cout<< link.noFrom << " " << link.noTo << " " << link.avgDelay << std::endl;

            topologyHelper.insertLinkInfo(link.noFrom, link.noTo, link.avgDelay, totalDataRate);

        }     

        topologyHelper.setupPBFTApp(pbfthelper);
        topologyHelper.setAddressHelper(address);
        topologyHelper.setNodeBw(totalDataRate);
        topologyHelper.installLink();

        topologyHelper.setLinkMetricDefination(BlockChainTopologyHelper::DELAY_ONLY);

        topologyHelper.setOverlayRoute();

        ApplicationContainer pbftNodes = topologyHelper.getApp();

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();

        pbftNodes.Start(Seconds(0));
        pbftNodes.Stop(Seconds(100));

        Ptr<PBFTCorrect> client = pbftNodes.Get(1)->GetObject<PBFTCorrect>();
        
        Simulator::Schedule(Seconds(0.1), &PBFTCorrect::sendRequestCircle, client, 100.0);

        //pointToPoint.EnablePcapAll("pbft", false);

        AsciiTraceHelper ascii;
        //pointToPoint.EnableAsciiAll(ascii.CreateFileStream("pbft.tr"));

    }


    

    Simulator::Run();
  	Simulator::Destroy();

    return 0;
}
