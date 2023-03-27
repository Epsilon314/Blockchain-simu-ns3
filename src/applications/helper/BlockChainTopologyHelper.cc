// Yiqing Zhu
// yiqing.zhu.314@gmail.com

#include "ns3/BlockChainTopologyHelper.h"

#include <random>
#include <chrono>
#include <algorithm>

#include <functional>

// Todo
// clean up this file

namespace ns3 {

int xor_dis(u_int8_t a, u_int8_t b) {
	return a ^ b;
}


BlockChainTopologyHelper::BlockChainTopologyHelper(int stable_n, int churn_n) {

	stableNodeN = stable_n;
	churnNodeN = churn_n;
	nodeN = stableNodeN + churnNodeN;

	nodes.Create(nodeN);

	InternetStackHelper stack;
	stack.Install(nodes);

	partiPointLast = stableNodeN - 1;
	partiPoint90 = (int) (stableNodeN - 1) * 0.9;
	partiPoint70 = (int) (stableNodeN - 1) * 0.7;

}

    
BlockChainTopologyHelper::~BlockChainTopologyHelper() {}


void BlockChainTopologyHelper::setupPBFTApp(PBFTCorrectHelper& pbft) {
  installedApps = pbft.Install(nodes);
}


void BlockChainTopologyHelper::setAddressHelper(Ipv4AddressHelper& addressHelper) {
  address = addressHelper;
}


// use inserted link information to build up the topology
// setup p2p devices, link them and save the address of interfaces
void BlockChainTopologyHelper::installLink() {

  setupNodeInfo();
	updateNodeMetric();
	
	for (size_t i = 0, sz = allLinkInfo.size(); i < sz; ++i) {

		auto link = allLinkInfo[i];

		p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(link.bw)));
		p2p.SetChannelAttribute("Delay", TimeValue(Seconds(link.delay)));
		
		NetDeviceContainer nLink = p2p.Install(nodes.Get(link.nodeA), nodes.Get(link.nodeB));
		Ipv4InterfaceContainer nInterface = address.Assign(nLink);
		address.NewNetwork();

		allLink.push_back(nLink);
		allInterface.push_back(nInterface);

		AddressEntry to,from;
		to.nodeFrom = link.nodeA;
		to.nodeTo = link.nodeB;
		to.addr = nInterface.GetAddress(1);
		from.nodeFrom = link.nodeB;
		from.nodeTo = link.nodeA;
		from.addr = nInterface.GetAddress(0);

		addressBook.push_back(to);
		addressBook.push_back(from);

	}

	setupAppPeer();

}


// tell appliaction topology informations
void BlockChainTopologyHelper::setupPeerMetric() {
	for (size_t i = 0, sz = allLinkInfo.size(); i < sz; ++i) {

		auto link = allLinkInfo[i];
		
		Ptr<PBFTCorrect> app;

		// only consider symmetric link for now 

		app = installedApps.Get(link.nodeA)->GetObject<PBFTCorrect>();
		app->AddDirectPeer(link.nodeB);
		app->AddPeerMetric(link.nodeB, link.linkMetric);
		
		app = installedApps.Get(link.nodeB)->GetObject<PBFTCorrect>();
		app->AddDirectPeer(link.nodeA);
		app->AddPeerMetric(link.nodeA, link.linkMetric);
	}
}


// tell application peer address 
void BlockChainTopologyHelper::setupAppPeer() {
	Ptr<PBFTCorrect> app;
	for (int i = 0; i < stableNodeN; ++i) {
		app = installedApps.Get(i)->GetObject<PBFTCorrect>();
		for (int j = 0; j < stableNodeN; ++j) {
			if (j != i) {
				Address addr = findAddress(i,j);
				if (!addr.IsInvalid()) {
					app->AddPeer(j, addr);
				}
			}
		}
	}
}


// helper func to look up address
Address BlockChainTopologyHelper::findAddress(int from, int to) {
	Address maddr; 
	for (size_t i = 0, sz = addressBook.size(); i < sz; ++i) {
	  auto entry = addressBook[i];
		if (entry.nodeTo == to) {
			maddr = entry.addr;
			if (entry.nodeFrom == from) {
				NS_ASSERT(!maddr.IsInvalid());
				return maddr;
			}
		}
	}

	std::cout << "No address between: " << from << " and " << to << std::endl;

	// return an invalid address
	return Address();

}


// called from external, say topology data provider or generator
// import topology in terms of link information
// seconds, bps
void BlockChainTopologyHelper::insertLinkInfo(int from, int to, double delay, int bw) {
	LinkInfo link(from, to , delay, bw, 1, 0.0);
	if (!related_to_churn_node(link)) ++stable_link_count;
	allLinkInfo.push_back(link);
}


void BlockChainTopologyHelper::setNodeBw(int bw) {
  nodeBw = bw;
}


void BlockChainTopologyHelper::setOverlayRoute() {

	updateLinkMetric();

	// choose broadcasters
	chooseCoreNodes();
	installCoreList();

	// prepare space
	tempRouteTable.clear();
	tempRouteTable.resize(stableNodeN);

	// generate the application-layer broadcast relay route 
	switch (topologyGenerateMethod1) {

		case SHORTEST_PATH_TREE:
			generateOverlayRoute_SPT();
		break;
		
		case SEQUENCIAL_AWARE:
			generateOverlayRoute_SA();
		break;

		case DISTRIBUTED:
			generatedOverlayRoute_DIS();
		break;

		case KADCAST:
			generateOverlayRoute_KAD();
		break;

		default:
		break;
	}
	
	setupPeerMetric();
	installRouteTable(LARGE_PACKET);


	// test
	
	averageMessageSize = smallMessageSize;

	// generate the application-layer broadcast relay route 
	switch (topologyGenerateMethod2) {

		case SHORTEST_PATH_TREE:
			generateOverlayRoute_SPT();
		break;
		
		case SEQUENCIAL_AWARE:
			generateOverlayRoute_SA();
		break;

		case DISTRIBUTED:
			generatedOverlayRoute_DIS();
		break;

		case KADCAST:
			generateOverlayRoute_KAD();
		break;

		default:
		break;
	}

	installRouteTable(SMALL_PACKET);

}


void BlockChainTopologyHelper::installShorestPath () {
	Ptr<PBFTCorrect> app;

	for (int src = 0; src < stableNodeN; ++src) {
		
		app = installedApps.Get(src)->GetObject<PBFTCorrect>();

		std::vector<double> D(stableNodeN);
		std::vector<int> P(stableNodeN);

		utilSP(D, P, src);

		app->installShortestPathRoute(std::move(P));

	}

	// for (int src = stableNodeN; src < nodeN; ++src) {
		
	// 	app = installedApps.Get(src)->GetObject<PBFTCorrect>();

	// 	std::vector<double> D(nodeN);
	// 	std::vector<int> P(nodeN);

	// 	utilSP(D, P, src);
	// 	app->installShortestPathRoute(std::move(P));
	// }

}


void BlockChainTopologyHelper::updateLinkMetric() {
	for (size_t i = 0, sz = allLinkInfo.size(); i < sz; ++i) {

		auto link = &allLinkInfo[i];
		if (related_to_churn_node(link)) continue;

		switch (linkMetricSetting) {
		
			case DELAY_ONLY:
			link->linkMetric = link->delay + averageMessageSize * 8 / (link->bw * 1000);
			break;

			case DELAY_BW_BALANCE:
			link->linkMetric = link->delay + averageMessageSize * 8 / (link->bw * link->share * 1000);
			break; 
				
			default:
			break;
		}
	}
}


void BlockChainTopologyHelper::updateNodeMetric() {

	// converge
	int max_round = 10;

	std::vector<double> accumulatedMetric(stableNodeN, 0), oldAccmulatedMetric(stableNodeN, 0);

	for (int i = 0; i < max_round; i++) {
		oldAccmulatedMetric = accumulatedMetric;
		for (size_t i = 0, sz = accumulatedMetric.size(); i < sz; ++i) {
			accumulatedMetric[i] = 0;
		} 
		for (size_t i = 0, sz = allLinkInfo.size(); i < sz; ++i) {
		  auto link = allLinkInfo[i];
			if (related_to_churn_node(&link)) continue;
			accumulatedMetric[link.nodeA] += link.bw / link.delay;
			accumulatedMetric[link.nodeA] += 0.05 * oldAccmulatedMetric[link.nodeB];
			accumulatedMetric[link.nodeB] += link.bw / link.delay;
			accumulatedMetric[link.nodeB] += 0.05 * oldAccmulatedMetric[link.nodeA];
		}
	}

	for (size_t i = 0, sz = allNodeInfo.size(); i < sz; ++i) {
		allNodeInfo[i].nodeMetric = accumulatedMetric[allNodeInfo[i].id];
	}

}


// To be tested
// require a full mesh topology
void BlockChainTopologyHelper::generateOverlayRoute_PLAIN() {
	
	clearState();
	
	for (size_t i = 0, sz = coreNodeList.size(); i < sz; ++i) {
		auto source = coreNodeList[i];
		for (int dst = 0; dst < stableNodeN; ++dst) {
			if (dst != source) {
				Route r(source, dst, source, source);
				routeTable[source][source].push_back(r);
				IncLinkShare(source, dst);
			}
		}
	}

	// updateDepth();

	updateSequencialMetric();

	// sort tranfer order

	for (auto &tree : routeTable) {
		for (auto &node : tree.second) {
			std::sort(node.begin(), node.end(),
				[this](Route a, Route b){return compareWRTSubtreeMetric(a, b);});
		}
	}

	updateSequencialMetric();

}


void BlockChainTopologyHelper::generateOverlayRoute_SPT() {

	clearState();

	// shortest path from source to other peers w.r.t link metric
	// D store distance, P store path

	for (auto source : coreNodeList) {

		updateLinkMetric();

		std::vector<double> D(stableNodeN);
		std::vector<int> P(stableNodeN);

		utilSP(D, P, source);

		// store forward policy 
		updateTempRouteVector(P, source);

		// store shortest path delay from source to node

		for (size_t n = 0; n < D.size(); ++n) {
			
			// note that D[source] = 0
			std::get<3>(sequentialMetricTable[source][n]) = D[n];

		}

	}

	// gather relay entry

	updateRouteVector();

	// updateDepth();

	updateSequencialMetric();

	// sort tranfer order 
	// todo: baseline should not use global infomation

	for (auto &tree : routeTable) {
		for (auto &node : tree.second) {
			std::sort(node.begin(), node.end(),
				[this](Route a, Route b){return compareWRTSubtreeMetric(a, b);});
		}
	}

	updateSequencialMetric();

}


void BlockChainTopologyHelper::generateOverlayRoute_SA() {

	// initiate with shortest path
	generateOverlayRoute_SPT();

	// updateDepth();

	updateSequencialMetric();

	// for now we treat each tree separetely one by one
 	for (auto src : coreNodeList) {

		// dev 
		std::cout << "SPT" << std::endl;
		peekTreeDebug(src, src, "SPT");
		std::cout << std::endl;

		// a simple greedy approach
		// try move the worst leaf node elsewhere to achieve a better min-max delay
		// stop when such place do not exist
		for (;;) {

			int new_prev = -2;
			int changer = getWorstMetric(src).first;

			if (changer != src) {
				// just sanity check

				new_prev = checkStability(src, changer);
			}

			if (new_prev < 0) break;
		}

		std::cout << "SA" << std::endl;
		peekTreeDebug(src, src, "SA");
		std::cout << std::endl;
		peekSubtreeWeightDistribution(src);

 	}
}


// ckeck if we can improve node's position
// return better position or -1 if it is already at best position
int BlockChainTopologyHelper::checkStability(int source, int node) {
	
	// double selfHeight = std::get<2>(sequentialMetricTable[source][node]); 
	// double currentPeak = std::get<3>(sequentialMetricTable[source][node]) + selfHeight;

	double currentWorst = getWorstMetric(source).second;

	int res = -1;
	int old_prev = getPrev(source, node);
	
	breadthFirstTopdownWalk(source, source, [this, source, node, currentWorst, old_prev, &res]
		(int new_prev){
		
		// debug
		NS_ASSERT(new_prev >= 0 && new_prev < stableNodeN);

		if (new_prev == old_prev) {
			// skip
			return true;
		}

		// check if new_prev is a better prev for node

		if (new_prev != node) {

			LinkInfo link;
			if (getLink(node, new_prev, link) == 0) {
				// no direct link, skip
				return true;
			}

			changePrev(source, node, new_prev);

			if (getWorstMetric(source).second < currentWorst) {
				// better previous node found, exit loop
				res = new_prev;
				return false;
			}
			else {
				// worser, change back
				changePrev(source, node, old_prev);
				return true;
			}

		}
		else {
			// skip self
			return true;
		}
	});

	return res;
}


void BlockChainTopologyHelper::generatedOverlayRoute_DIS() {

	// start from any tree structure
	// reuse the SPT code for simplicity
	// rebalance the tree from every node's point of view

	// initiate with shortest path
	generateOverlayRoute_SPT();

	// updateDepth();

	updateSequencialMetric();

	for (auto src : coreNodeList) {

		std::cout << "SPT" << std::endl;
		peekTreeDebug(src, src, "SPT");
		std::cout << std::endl;

		for (;;) {
			
			bool changed = false;

			breadthFirstTopdownWalk(src, src, [this, src, &changed](int cursor){

				auto rebalanceCond = getRebalanceCond(src, cursor);
				auto currentOt = std::get<0>(rebalanceCond);
				auto vasselCharTable = std::get<1>(rebalanceCond);
				auto vasselCharTableByWeight = std::get<1>(rebalanceCond);
				auto vasselCharTableR = std::get<1>(rebalanceCond);


				// offload the lightest appendent

				for (auto receiver : vasselCharTable) {

					// std::cout << "grant loop over receiver " << std::get<0>(receiver) << std::endl;

					for (auto grantee : vasselCharTableByWeight) {

					  //	std::cout << "grant loop over grantee " << std::get<0>(grantee) << std::endl;

						if (std::get<0>(grantee) == std::get<0>(receiver)) continue;

						// loi + loj > d_{j,m}

						bool grantCondition;
						if (getChildNumber(src, std::get<0>(receiver)) != 0) {
							grantCondition = (currentOt > std::get<2>(receiver)) && 
								(currentOt - std::get<2>(receiver) > 
								getNodeTransferDelay(std::get<0>(receiver), std::get<0>(grantee)) 
								// + getNodeTransferDelay(cursor, std::get<0>(grantee)) 
								+ delta);
						}
						else {
							grantCondition = (currentOt > std::get<2>(receiver)) &&
								(currentOt - std::get<2>(receiver) > 
								getNodeTransferDelay(std::get<0>(receiver), std::get<0>(grantee)) 
								// + getNodeTransferDelay(cursor, std::get<0>(grantee))
								+ processingDelay + delta);
						}

						if (grantCondition) {
							if (hasLink(std::get<0>(receiver), std::get<0>(grantee))) {
								
								// std::cout << "tree " << src << " grant " << std::get<0>(grantee) 
								// 	<< " to " << std::get<0>(receiver) << std::endl;
							  // std::cout << "debug " << currentOt << " " << std::get<2>(receiver) 
								// 	<< " " << getNodeTransferDelay(std::get<0>(receiver), std::get<0>(grantee)) 
								// 	+ getNodeTransferDelay(cursor, std::get<0>(grantee)) << std::endl;

								changePrev(src, std::get<0>(grantee), std::get<0>(receiver));
								changed = true;
								return false;
							}
						}
						else {
							break;
						}
					}				
				}

				// retract from the slowest appendent

				for (auto target : vasselCharTableR) {

				  //	std::cout << "retract loop over target " << std::get<0>(target) << std::endl;

					std::vector<std::pair<int, double> > weightList;
					for (auto secondaryVassal : routeTable[src][std::get<0>(target)]) {

						weightList.push_back(
							std::make_pair(
								secondaryVassal.dst,
								std::get<1>(sequentialMetricTable[src][secondaryVassal.dst])
							)
						);
					}

					std::sort(weightList.begin(), weightList.end(), 
						[=](std::pair<int, double> a, std::pair<int, double> b)
						{return a.second < b.second;});

					for (auto retractee : weightList) {

						// std::cout << "retract loop over retractee " << retractee.first << std::endl;

						// fixme: retract and grant

						bool retractCondition;
						retractCondition = (std::get<2>(target) > currentOt) && 
							(std::get<2>(target) - currentOt > getNodeTransferDelay(cursor, retractee.first)
							+ delta);

						if (retractCondition) {
						// if (std::get<1>(target) - currentFt > retractee.second + delta) {
							if (hasLink(cursor, retractee.first)) {

								// std::cout << "tree " << src << " retract " << retractee.first << " from "
								// 	 << std::get<0>(target) << std::endl;
							  // std::cout << "debug " << currentOt << " " << std::get<2>(target) << " "
								// 	<< getNodeTransferDelay(cursor, retractee.first)
								// 	+ getNodeTransferDelay(std::get<0>(target), retractee.first) << std::endl;

								changePrev(src, retractee.first, cursor);
								changed = true;
								return false;
							}
						}
						else {
							break;
						}
					}
				}

				return true;

			});

			// nothing can be changed 
			if (!changed) break;

		}

		std::cout << "DIS" << std::endl;
		peekTreeDebug(src, src, "DIS");
		std::cout << std::endl;
		peekSubtreeWeightDistribution(src);
	}
}

void BlockChainTopologyHelper::generateOverlayRoute_KAD() {
	while (!_generateOverlayRoute_KAD()) {}
}

bool BlockChainTopologyHelper::_generateOverlayRoute_KAD() {

	// force full mesh

	clearState();

	// force all core
  NS_ASSERT(chooseCore == ALL);
	
	// assign id

	k_bucket.clear();
	k_bucket.resize(stableNodeN);

	tempKadcastPath.clear();
	tempKadcastPath.resize(stableNodeN);

	for (int i = 0; i <stableNodeN; ++i) {
		tempKadcastPath[i].resize(stableNodeN);
		for (int j = 0; j < stableNodeN; ++j) {
			tempKadcastPath[i][j] = -1;
		}
		tempKadcastPath[i][i] = i;
	}

	kNodeList.clear();

	for (u_int8_t i = 0;; ++i) {
		idSpace[i] = i;
		if (i == 255) break;
	}

	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  shuffle(idSpace.begin(), idSpace.end(), std::default_random_engine(seed));

	for (int i = 0; i < stableNodeN; ++i) {
		kNodeList.push_back(std::make_pair(i, idSpace[i]));
	}
	
	// populate k-bucket 
	// 0 as bootstrip	


	for (int newComer = 1; newComer < stableNodeN; ++newComer) {

		// 1. connect to bootstrap node (assign 0)

		int distanceToRoot = xor_dis(getKadId(newComer), getKadId(0));

		if (k_bucket[newComer][disToBucketN(distanceToRoot)].size() < KAD_K) {
			k_bucket[newComer][disToBucketN(distanceToRoot)].insert(0);
		}
		if (k_bucket[0][disToBucketN(distanceToRoot)].size() < KAD_K) {
			k_bucket[0][disToBucketN(distanceToRoot)].insert(newComer);
		}

		// 2. find its own id

		kad_find(getKadId(newComer), newComer);
	}

	// 3. populate k_buckets

	globalPopulateKBucket();

	// std::cout << "kad bucket" << std::endl;
	// 	for (int i = 0; i < nodeN; ++i) {
	// 	printKBucket(i);
	// }

	// install route table

	for (int root = 0; root < stableNodeN; ++root) {
		generateKadCastRoute(root, root, 8);
		for (auto i : tempKadcastPath[root]) {
			if (i == -1) return false;
		}
		updateTempRouteVector(tempKadcastPath[root], root);
	}

	updateRouteVector();

	
	// send to bigger bucket first
	for (auto &tree : routeTable) {
		for (auto &node : tree.second) {
			std::sort(node.begin(), node.end(),
				[this](Route a, Route b){return getBucketNumber(a.self, a.dst) 
				> getBucketNumber(b.self, b.dst);});
		}
	}
	
	

	// updateDepth();

	updateSequencialMetric();

	for (int src = 0; src < stableNodeN; ++src) {
		std::cout << "KAD" << std::endl;
		peekTreeDebug(src, src, "KAD");
		std::cout << std::endl;
		peekSubtreeWeightDistribution(src);
	}



	return true;
}


// error !
void BlockChainTopologyHelper::generateKadCastRoute(int r, int s, int h) {
	for (int bucket = 0; bucket < h; bucket++) { // first h bucket
		if (!k_bucket[s][bucket].empty()) {
			
			// std::cout << "debug route gen: " << r << ',' << s << ',' << h << std::endl;
			// std::cout << "bucket:" << bucket << std::endl;
			// std::cout << "set " <<  *(k_bucket[s][bucket].begin()) << " as " << s << std::endl
			// 	<< std::endl;

			unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
			srand(seed);

			auto idx = rand() % k_bucket[s][bucket].size();
			auto delegate = *std::next(k_bucket[s][bucket].begin(), idx);
			
			tempKadcastPath[r][delegate] = s;
			if (bucket > 0) {
				generateKadCastRoute(r, delegate, bucket);
			}
		}
	}
}


u_int8_t BlockChainTopologyHelper::getKadId(int i) {
	for (auto k : kNodeList) {
		if (k.first == i) return k.second;
	}
	// error
	return 0;
}


int BlockChainTopologyHelper::getBucketNumber(int n, int d) {
	for (int i = 0; i < 8; ++i) {
		for (auto v: k_bucket[n][i]) {
			if (d == v) return i;
		}
	}
	return -1;
}


int BlockChainTopologyHelper::disToBucketN(int dis) {
	int l = 2, i = 0;
	for(;;) {
		if (dis < l) return i;
		l = l << 1;
		i++;
	}
}


// k nearset to q node in r's bucket
std::set<int> BlockChainTopologyHelper::kNearestNode(u_int8_t q, int recv) {

	auto kad_cmp = [](std::pair<int, int> a, std::pair<int, int> b){return a.second < b.second;};
	
	std::set<int> rv;
		
	// node, dis  
	std::set<std::pair<int, int>, decltype(kad_cmp)> resultDis(kad_cmp);

	for (auto b: k_bucket[recv]) {
		for (auto k: b) {
			resultDis.insert(std::make_pair(k, xor_dis(q, getKadId(k))));
		}
	}

	u_int i = 0;
	for (auto p = resultDis.begin(); p != resultDis.end() && i < KAD_K; p++, i++) {
		rv.insert(p->first);
	}
	return rv;

}


void BlockChainTopologyHelper::kad_find(u_int8_t target, int caller) {
 
	auto kad_cmp = +[](std::pair<int, int> a, std::pair<int, int> b){return a.second < b.second;};

	std::set<std::pair<int, int>, decltype(kad_cmp)> knownPeer(kad_cmp);
	std::set<std::pair<int, int>, decltype(kad_cmp)> query(kad_cmp);
	std::set<std::pair<int, int>, decltype(kad_cmp)> lastQuery(kad_cmp);

	for (;;) {
		lastQuery = query;

		knownPeer.clear();
		for (auto bucket: k_bucket[caller]) {
			for (auto peer: bucket) {
				knownPeer.insert(std::make_pair(peer, xor_dis(getKadId(peer), target)));
			}
		}

		u_int i = 0;
		for (auto it = knownPeer.begin(); i < KAD_ALPHA && it != knownPeer.end(); ++i, ++it) {
			query.insert(*it);
		}

		if (query == lastQuery) break;

		for (auto peer: query) {
			
			auto kNearestPeerList = kNearestNode(target, peer.first);

			for (auto newKnownPeer: kNearestPeerList) {

				if (newKnownPeer == caller) continue;

				// update k_bucket
				int distance = xor_dis(getKadId(caller), getKadId(newKnownPeer));

				if (k_bucket[caller][disToBucketN(distance)].size() < KAD_K) {  // bucket is not full
					k_bucket[caller][disToBucketN(distance)].insert(newKnownPeer);
				}

				if (k_bucket[newKnownPeer][disToBucketN(distance)].size() < KAD_K) {
					k_bucket[newKnownPeer][disToBucketN(distance)].insert(caller);
				}
			}
		}
	}
}


void BlockChainTopologyHelper::printKBucket(int i) {
	int k = 0;
	std::cout << "node " << i << "'s k-buckets:" << std::endl;
	for (auto bucket: k_bucket[i]) {
		std::cout << "bucket" << k++ << ": ";
		for (auto peer: bucket) {
			std::cout << peer << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}


void BlockChainTopologyHelper::globalPopulateKBucket() {
	for (int peer = 0; peer < stableNodeN; ++peer) {
		for (int bucketN = 0; bucketN < 8; ++ bucketN) {
			u_int max_size = (u_int)(1 << bucketN) < KAD_K ? (u_int)(1 << bucketN) : KAD_K;
			
			// k_bucket not full
			u_int bucketSz = k_bucket[peer][bucketN].size();
			if (bucketSz < max_size) {
				unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
				srand(seed);
				if (bucketSz == 0) {	// empty bucket
					u_int8_t offset = (u_int8_t) rand() % (1 << bucketN) + (1 << bucketN);
					u_int8_t address = peer ^ offset;
					kad_find(address, peer);
				}
				else {
					kad_find(getKadId(*std::next(k_bucket[peer][bucketN].begin(), rand() % bucketSz)), peer);
				}
			}
		}
	}
}


void BlockChainTopologyHelper::chooseCoreNodes() {

	coreNodeList.clear();
	
	switch (chooseCore) {
		
		case ALL:
			for (int i = 0; i < stableNodeN; ++i) {
				coreNodeList.push_back(i);
			}
			clusterN = stableNodeN;
		break;

		case SPECTRAL_CLUSTERING:
			{
			int n = allNodeInfo.size();

			std::vector<std::vector<double> > aff(n, std::vector<double>(n, 1));

			for (int i = 0; i < n; ++i) {
				aff[i][i] = 0;
			}

			for (auto &link : allLinkInfo) {
				if (related_to_churn_node(link)) continue;
				aff[link.nodeA][link.nodeB] = link.linkMetric;
				aff[link.nodeB][link.nodeA] = link.linkMetric;
			}

			/*
			for (int i = 0; i < n; ++i) {
				for (int j = 0; j < n; ++j) {
					std::cout << aff[i][j] << " ";
				}
				std::cout << std::endl;
			}
			*/

			SpectralClustering sc;
			sc.setAffineMatrix(aff);

			auto cluster = sc.spectral(clusterN, 0.005, 100000);

			std::vector<std::pair<double, int> > coreCandidate(clusterN, 
				std::pair<double, int>(0, 0));
			int clusterAssign;
			for (size_t i = 0; i < cluster.size(); ++i) {
				clusterAssign = cluster[i];
			  //	std::cout << clusterAssign << " ";
				if (allNodeInfo[i].nodeMetric > coreCandidate[clusterAssign].first) {
					coreCandidate[clusterAssign].first = allNodeInfo[i].nodeMetric;
					coreCandidate[clusterAssign].second = i;
				}
			}
			

			for (size_t i = 0; i < coreCandidate.size(); ++i) {
				coreNodeList.push_back(coreCandidate[i].second);
			}
			}
		break;		
		default:
		break;
	}

	// shuffle core list
	unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::shuffle(coreNodeList.begin(), coreNodeList.end(), std::default_random_engine(seed));

}


void BlockChainTopologyHelper::installCoreList() {
	for (int source = 0; source < stableNodeN; ++source) {

		Ptr<PBFTCorrect> app = installedApps.Get(source)->GetObject<PBFTCorrect>();

		bool isCore = false;
		for (auto i : coreNodeList) {
			if (source == i) {
				isCore = true;
				break;
			}
		}

		if (!isCore) {
			// if source is not a core node


			std::vector<double> D(stableNodeN);
			std::vector<int> P(stableNodeN);

			utilSP(D, P, source);

			for (auto coreNode : coreNodeList) {
				app->AddCorePeer(coreNode);
				app->AddCorePeerMetric(coreNode, D[coreNode]);
			}
		}
		else {
			for (auto coreNode : coreNodeList) {
				app->AddCorePeer(coreNode);
			}
		}
	}
}


void BlockChainTopologyHelper::setupNodeInfo() {

	for (int i = 0; i < stableNodeN; ++i) {
		NodeInfo node;
		node.id = i;
		node.degree = 0;
		allNodeInfo.push_back(node);
	}

	for (auto link = allLinkInfo.begin(); link != allLinkInfo.end(); ++link) {
		if (link->nodeA >= stableNodeN || link->nodeB >= stableNodeN) continue;
		for (auto node = allNodeInfo.begin(); node != allNodeInfo.end(); ++node) {
			if (link->nodeA == node->id || link->nodeB == node->id) {
				node->degree = node->degree + 1;
			}
		}
	}

	switch (broadwidthModel) {
	
		case NODE_P:
			
			/**
			 * if bw is modeled as each node has a maxium outbound bw shared
			 * equally between its outbound links
			 */

			for (auto &link : allLinkInfo) {
				if (link.nodeA >= stableNodeN || link.nodeB >= stableNodeN) continue;

				// suppose the outbound b.w. is the bottleneck and is capped for each node
				int degreeA = getDegree(link.nodeA);
				link.bw = (int) nodeBw / degreeA;
				
			}
		break;
		
		case NODE_S:

			for (auto &link : allLinkInfo) {
				if (link.nodeA >= stableNodeN || link.nodeB >= stableNodeN) continue;
				// focus on one link each time

				link.bw = (int) nodeBw;

			}
		break;

		case LINK:
			for (auto &link : allLinkInfo) {
				if (link.nodeA >= stableNodeN || link.nodeB >= stableNodeN) continue;
				link.bw /= 1000;
			}
		break;

		case CAPPED_BY_NODE:
			for (auto &link : allLinkInfo) {
				if (link.nodeA >= stableNodeN || link.nodeB >= stableNodeN) continue;

				link.bw /= 1000;

				if (link.bw > nodeBw) {
					link.bw = nodeBw;
				}

				// std::cout << link.bw << ' ';
			}
			// std::cout << std::endl;
		break;

		default:
			break;
	}

}


// return degree of  node i
// symmetric link
int BlockChainTopologyHelper::getDegree(int i) {
	
	for (auto node = allNodeInfo.begin(); node != allNodeInfo.end(); ++node) {
		if (node->id == i) return node->degree;
	}

	std::cerr << "Get degree bad node id" << std::endl;
	return 1;
}


void BlockChainTopologyHelper::IncLinkShare(int a, int b) {
  for (auto link = allLinkInfo.begin(); link != allLinkInfo.end(); ++link) {
		if (link->nodeA >= stableNodeN || link->nodeB >= stableNodeN) continue;
    if ((link->nodeA == a && link->nodeB == b) || (link->nodeB == a && link->nodeA == b)) {
      link->share = link->share + 1;
    }
  }
}


void BlockChainTopologyHelper::DecLinkShare(int a, int b) {
	for (auto link = allLinkInfo.begin(); link != allLinkInfo.end(); ++link) {
		if (link->nodeA >= stableNodeN || link->nodeB >= stableNodeN) continue;
    if ((link->nodeA == a && link->nodeB == b) || (link->nodeB == a && link->nodeA == b)) {
      link->share = link->share > 0 ? link->share - 1 : 0;
    }
  }
}


// return 1 if link exist and 0 if not
// refer glink to the requested link
int BlockChainTopologyHelper::getLink(int a, int b, LinkInfo &glink) {
	for (auto link = allLinkInfo.begin(); link != allLinkInfo.end(); ++link) {
		if (link->nodeA >= stableNodeN || link->nodeB >= stableNodeN) continue;
    if ((link->nodeA == a && link->nodeB == b) || (link->nodeB == a && link->nodeA == b)) {
			glink = *link;
			// direct link exist
			return 1;
		}
	}
	// no direct link
	return 0;
}


bool BlockChainTopologyHelper::hasLink(int a, int b) {
	LinkInfo link;
	return 1 == getLink(a, b, link);
}


double BlockChainTopologyHelper::getNodeTransferDelay(int a, int b) {
	LinkInfo link;
	NS_ASSERT(1 == getLink(a, b, link)); 
	return (double) averageMessageSize * 8.0 / (double) (link.bw * 1000);
}


double BlockChainTopologyHelper::getLinkDelay(int a, int b) {
	LinkInfo link;
	NS_ASSERT(1 == getLink(a, b, link));
	return link.delay;
}


int BlockChainTopologyHelper::getPrev(int root, int node) {

	if (node == root) return -1;

	for (int prevNode = 0; prevNode < stableNodeN; ++prevNode) {
		for (auto r : routeTable[root][prevNode]) {
			if (r.dst == node) return prevNode;
		}
	}

	std::cerr << "Bad previous node. Root: " << root << " node: " << node <<std::endl;  
	return -1;
	
}


std::pair<int, double> BlockChainTopologyHelper::getWorstMetric(int root) {
	switch (latencyOptPartiPoint) {
		
	case LOPP_LAST:
		return std::get<0>(treeMetric[root]);
		break;

	case LOPP_90:
		return std::get<1>(treeMetric[root]);
		break;
	
	case LOPP_70:
		return std::get<2>(treeMetric[root]);
		break;
	
	default:
		return std::get<0>(treeMetric[root]);
		break;
	}
}

void BlockChainTopologyHelper::utilSP(std::vector<double> &D, std::vector<int> &P, int source) {

	NS_ASSERT((int)D.size() == (int)P.size());

	int sz = D.size();

	for (size_t d = 0; d < (size_t) sz; ++d) D[d] = std::numeric_limits<double>::infinity();
	for (size_t p = 0; p < (size_t) sz; ++p) P[p] = -1;

	D[source] = 0;
	P[source] = source;

	for (int i = 0; i < (int) sz - 1; ++i) {

		for (auto link = allLinkInfo.begin(); link != allLinkInfo.end(); ++link) {
			
			if (sz == stableNodeN && (link->nodeA >= sz || link->nodeB >= sz)) continue;

			if (D[link->nodeA] + link->linkMetric < D[link->nodeB]) {
				D[link->nodeB] = D[link->nodeA] + link->linkMetric;
				P[link->nodeB] = link->nodeA;
			}

			if (D[link->nodeB] + link->linkMetric < D[link->nodeA]) {
				D[link->nodeA] = D[link->nodeB] + link->linkMetric;
				P[link->nodeA] = link->nodeB;
			}

		}
	}

}


// move a node to a new previous node and re-order the tree
// update all related global states
void BlockChainTopologyHelper::changePrev(int source, int changer, int new_prev) {

	// update routes, always placed at the last of same-order routes

	int old_prev = getPrev(source, changer);
	
	for (auto r = routeTable[source][old_prev].begin(); r != routeTable[source][old_prev].end();) {
		if (r->dst == changer) {
			Route nr = *r;

		  // std::cout << routeTable[source][old_prev].size() <<std::endl;
			
			nr.from = getPrev(source, new_prev);
			nr.self = new_prev;
			routeTable[source][new_prev].push_back(nr);

			r = routeTable[source][old_prev].erase(r);

		}
		else {
			++r;
		}
	}

	for (auto &r : routeTable[source][changer]) {
			r.from = new_prev;
	}

	// update load

	IncLinkShare(changer, new_prev);
	DecLinkShare(changer, old_prev);

	// update depth

	int depthDiff = std::get<0>(sequentialMetricTable[source][new_prev]) -
									std::get<0>(sequentialMetricTable[source][old_prev]);

	breadthFirstTopdownWalk(source, changer,
		[this, depthDiff, source](int n){
			std::get<0>(sequentialMetricTable[source][n]) += depthDiff;
			return true;
		});

	// update nodes transfer order
	for (auto &tree : routeTable) {
		for (auto &node : tree.second) {
			std::sort(node.begin(), node.end(),
				[this](Route a, Route b){return compareWRTSubtreeMetric(a, b);});
		}
	}

	// updateDepth();

	// update metric
	updateSequencialMetric();

}


int BlockChainTopologyHelper::getSequencialOrder(int src, int prev_node, int node) {
	
	for (size_t i = 0; i < routeTable[src][prev_node].size(); ++i) {
		if (routeTable[src][prev_node][i].dst == node) return i + 1;
	}
	
	std::cerr << "bad order, parameter:" << src << " "
		<< prev_node << " " << node << std::endl;
	return 0;

}

int BlockChainTopologyHelper::getChildNumber(int src, int node) {

	return routeTable[src][node].size();

}


// fixme
// too subtle
void BlockChainTopologyHelper::updateSequencialMetric() {
	
	for (auto src : coreNodeList) {
		for (auto n = 0; n < stableNodeN; ++n) {
			std::get<1>(sequentialMetricTable[src][n]) = 0;
			std::get<2>(sequentialMetricTable[src][n]) = 0;
			std::get<3>(sequentialMetricTable[src][n]) = 0;
		}
	}

	updateDepth();
	

	for (auto src : coreNodeList) {

		int depth = treeDepth[src];
		std::map<int, double> subNodes;

	  // std::cout << "tree: " << src << "depth: " << depth << std::endl;

		// cacalate delay cost of sub-tree in ground up order
		// not efficient
		while (depth >= 0) {

			for (int node = 0;  node < stableNodeN; ++node) {

				int nodeDepth = std::get<0>(sequentialMetricTable[src][node]);

				if (depth == nodeDepth) {

					subNodes.clear();

				  //	std::cout << node << " ";

					// from bottom to top
					double subMetric = 0.0;

					int subWeight = 1;

					if (routeTable[src][node].size() > 0) {
						subMetric += processingDelay;
					}

					for (auto r : routeTable[src][node]) {
						
						int subNode = r.dst;
						
						double transferDelay = getNodeTransferDelay(node, subNode);
						double linkDelay = getLinkDelay(node, subNode);
						
						// metric is the time interval from the previous node start sending to all nodes
						// in its sub-tree received 
						// root node's metric won't be updated right for it is not used
						std::get<1>(sequentialMetricTable[src][subNode]) += linkDelay;

						subWeight += std::get<4>(sequentialMetricTable[src][subNode]);

						/*
						std::cout << "debug subMetric: " << src << " " << node << " " << subNode << std::endl;
						std::cout << std::get<1>(sequentialMetricTable[src][subNode]) << " " << 
							transferDelay * getSequencialOrder(src, node, subNode) << " " << transferDelay << " "
							<< getSequencialOrder(src, node, subNode) << std::endl;
						*/

						//Todo: check if units are uniform
						subMetric = std::get<1>(sequentialMetricTable[src][subNode])
							 + transferDelay * getSequencialOrder(src, node, subNode);

						subNodes[subNode] = subMetric;
					}

					std::get<4>(sequentialMetricTable[src][node]) = subWeight;

				  auto maxMetric =	std::max_element(subNodes.begin(), subNodes.end(), 
						[](std::pair<int, double> a, std::pair<int, double> b){return a.second < b.second;});
					
					std::get<1>(sequentialMetricTable[src][node]) = maxMetric->second;
					std::get<2>(sequentialMetricTable[src][node]) = maxMetric->second;

				}
			}

		  // std::cout << std::endl;

			// process upper level
			depth--;
		}

		// calculate delay to reach in top down order
		breadthFirstTopdownWalk(src, src, [this, src](int n){
			if (n == src) {
				std::get<3>(sequentialMetricTable[src][n]) = 0;
			}
			else {
				int prev = getPrev(src, n);

				std::get<3>(sequentialMetricTable[src][n]) = 
					std::get<3>(sequentialMetricTable[src][prev]) + getLinkDelay(prev, n) +
					getNodeTransferDelay(prev, n) * getSequencialOrder(src, prev, n);
				
				if (prev != src) std::get<3>(sequentialMetricTable[src][n]) += processingDelay;

			}
			// continue
			return true;
		});

		for (size_t i = 0, sz = sequentialMetricTable[src].size(); i < sz; ++i) {
			double transfer_time = 0.0;
			for (auto r: routeTable[src][i]) {
				transfer_time += getNodeTransferDelay(i, r.dst);
			}
			std::get<5>(sequentialMetricTable[src][i]) = std::get<3>(sequentialMetricTable[src][i]) 
				+ transfer_time;
		}

		// store worst delay to reach

		std::vector<std::pair<int, double>> delayViewOfMetricTable;
		for (size_t i = 0, sz = sequentialMetricTable[src].size(); i < sz; ++i) {
			delayViewOfMetricTable.push_back(std::pair<int, double>(
				i,
				std::get<3>(sequentialMetricTable[src][i])
			));
		}

		std::sort(delayViewOfMetricTable.begin(), delayViewOfMetricTable.end(),
			[](const std::pair<int, double> &a, const std::pair<int, double> &b) {
				return a.second < b.second;
			}
		);

		std::get<0>(treeMetric[src]) = delayViewOfMetricTable[partiPointLast];
		std::get<1>(treeMetric[src]) = delayViewOfMetricTable[partiPoint90];
		std::get<2>(treeMetric[src]) = delayViewOfMetricTable[partiPoint70];

	}
}


// return true if a is greater than b and false else wise
bool BlockChainTopologyHelper::compareWRTSubtreeMetric(const Route &a, const Route &b) {

	NS_ASSERT(a.src == b.src && a.self == b.self);

	int source = a.src;

	int nodeA = a.dst;
	int nodeB = b.dst;

	// std::cout << "debug" << std::endl;
	// std::cout << " a: " << a.dst << " b: " << b.dst << std::endl;
	// std::cout << " a metric: " << sequentialMetricTable[source][nodeA].second
	//				  << " b metric: " << sequentialMetricTable[source][nodeB].second << std::endl;

	return std::get<1>(sequentialMetricTable[source][nodeA]) > 
		std::get<1>(sequentialMetricTable[source][nodeB]);

}


void BlockChainTopologyHelper::updateDepth() {

	std::vector<int> nextDepth;

	// ranged-for lead to mysterious out-of-bound error and i donot know why 
	for (size_t i = 0, sz = coreNodeList.size(); i < sz; ++i) {
		auto source = coreNodeList[i];
		int depth = 0;
		nextDepth.clear();
		nextDepth.push_back(source);
		
		while (!nextDepth.empty()) {

			int eraseBorder = nextDepth.size();

			// ranged-for lead to mysterious out-of-bound error and i donot know why
			for (size_t j = 0, sz2 = nextDepth.size(); j < sz2; ++j) {

				auto n = nextDepth[j];
				std::get<0>(sequentialMetricTable[source][n]) = depth;
				
				for (auto r : routeTable[source][n]) {
					nextDepth.push_back(r.dst);	
				}
				
			}

			nextDepth.erase(nextDepth.begin(), nextDepth.begin() + eraseBorder);

			depth++;
		}

		treeDepth[source] = depth - 1;

	}
}


std::tuple<
    double,
    std::vector<std::tuple<int, double, double, double> >,
    std::vector<std::tuple<int, double, double, double> >,
    std::vector<std::tuple<int, double, double, double> > >
BlockChainTopologyHelper::getRebalanceCond(int src, int cursor) {
	double currentOt = std::get<5>(sequentialMetricTable[src][cursor]);

	// <id, finish_time, offer_time, weight>
	std::vector<std::tuple<int, double, double, double> > vasselCharTable;
	
	for (auto vassal: routeTable[src][cursor]) {
		
		double nFt = std::get<2>(sequentialMetricTable[src][vassal.dst]) + 
									std::get<3>(sequentialMetricTable[src][vassal.dst]);
		
		double nOt = std::get<5>(sequentialMetricTable[src][vassal.dst]);

		double weight = std::get<1>(sequentialMetricTable[src][vassal.dst]);

		vasselCharTable.push_back(std::make_tuple(vassal.dst, nFt, nOt, weight));
	}

	// its tiny vector, so just use a full copy for different orders
	std::vector<std::tuple<int, double, double, double> > vasselCharTableByWeight = vasselCharTable;
	std::vector<std::tuple<int, double, double, double> > vasselCharTableR = vasselCharTable;

	// ascending in finish time
	std::sort(vasselCharTable.begin(), vasselCharTable.end(),
		[=](std::tuple<int, double, double, double> a, std::tuple<int, double, double, double> b)
		{return std::get<1>(a) < std::get<1>(b);});
	
	// decending in finish time
	std::reverse(vasselCharTableR.begin(), vasselCharTableR.end());

	// ascending in weight
	std::sort(vasselCharTableByWeight.begin(), vasselCharTableByWeight.end(),
		[=](std::tuple<int, double, double, double> a, std::tuple<int, double, double, double> b)
		{return std::get<2>(a) < std::get<2>(b);});

	return std::make_tuple(
		currentOt,
		vasselCharTable,
		vasselCharTableByWeight,
		vasselCharTableR
	);
}


// insert to a set to remove duplicates
void BlockChainTopologyHelper::updateTempRouteVector(std::vector<int> &P, int source) {

	// std::cout << "route vector for: " << source << " ";
	// for (auto i : P) std::cout << i << " ";
	// std::cout << std::endl;


	for (int i = 0; i < stableNodeN; ++i) {
		if (i != source) {				

			Route r;

			r.src = source;
		
			r.from = P[P[i]]; 
			r.self = P[i];
			r.dst = i;

			tempRouteTable[P[i]].insert(r);

		}
	}
}


// gather route entry and organize them
void BlockChainTopologyHelper::updateRouteVector() {
	
	for (int node = 0; node < stableNodeN; ++ node ) {
		for (auto r = tempRouteTable[node].begin(); r != tempRouteTable[node].end(); ++r) {
			
			routeTable[r->src][node].push_back(*r);
			IncLinkShare(node, r->dst);
		}
	}

}


void BlockChainTopologyHelper::installRouteTable(int level) {

	Ptr<PBFTCorrect> app;

	// install overlay route to nodes

	// for (auto src :coreNodeList) {
	// 	std::cout << "Install:" << std::endl;
	// 	peekTreeDebug(src, src);
	// 	std::cout << std::endl;
	// }

	switch (level) {
	
		case LARGE_PACKET:
			for (auto src : coreNodeList) {
				auto tree = routeTable[src];
				for (int node = 0; node < stableNodeN; ++node) {
					app = installedApps.Get(node)->GetObject<PBFTCorrect>();
					for (size_t i = 0, sz = tree[node].size(); i <sz; ++i) {
						auto r = tree[node][i];

						NS_ASSERT(hasLink(r.self, r.dst));

						app->insertRelayLargePacket(r.src, r.from, r.dst);
					}
				}
			}
		break;
		
		case SMALL_PACKET:
			for (auto src : coreNodeList) {
				auto tree = routeTable[src];
				for (int node = 0; node < stableNodeN; ++node) {
					app = installedApps.Get(node)->GetObject<PBFTCorrect>();
					for (size_t i = 0, sz = tree[node].size(); i <sz; ++i) {
						auto r = tree[node][i];

						NS_ASSERT(hasLink(r.self, r.dst));

						app->insertRelaySmallPacket(r.src, r.from, r.dst);
					}
				}
			}
		break;
	
	default:
		break;
	}

}


// traverse through the tree specified by root in breadth first topdown order
// and apply func f on each node
// func f parameters : root id, node id
// func f return false to early stop the traverse and true to continue
void BlockChainTopologyHelper::breadthFirstTopdownWalk(int root, int start,
	std::function<bool(int)> f) {

	std::queue<int> nextDepth;
	
	nextDepth.push(start);
	
	while ( !(nextDepth.empty()) ) {

		int n = nextDepth.front();
		nextDepth.pop();
		
		// do stuff
		if ( !f(n) ) return;

		// next level
		for (size_t i = 0, sz = routeTable[root][n].size(); i < sz; ++i) {
			nextDepth.push(routeTable[root][n][i].dst);
		}
	}
}


void BlockChainTopologyHelper::clearState() {

	tempRouteTable.clear();
	tempRouteTable.resize(stableNodeN);



	routeTable.clear();
	sequentialMetricTable.clear();

	for (auto src : coreNodeList) {

		if (routeTable.find(src) == routeTable.end()) {
			std::vector<std::vector<Route> > temp(stableNodeN);
			routeTable[src] = temp;
		}
		else {
			routeTable[src].resize(stableNodeN);
		}

		if (sequentialMetricTable.find(src) == sequentialMetricTable.end()) {
			std::vector<std::tuple<int, double, double, double, int, double> > temp(stableNodeN,
				std::tuple<int, double, double, double, int, double>(0,0,0,0,0,0));
			sequentialMetricTable[src] = temp;
		}
		else {
			sequentialMetricTable[src].resize(stableNodeN);
		}

		treeDepth.clear();
		treeDepth[src] = 0;

		treeMetric.clear();
	 	treeMetric[src] = std::tuple<metricCase, metricCase, metricCase>();

	}
}


void BlockChainTopologyHelper::peekTreeDebug(int root, int node, std::string label) {
	peekTreeDebug(root, node, 0);
	std::cout << "<" << label << ": ";
	for (auto l: tempLatencyPrintBuffer) std::cout << l << ", ";
	std::cout << '>' << std::endl;
}


void BlockChainTopologyHelper::peekTreeDebug(int root, int node, int level) {
	
	if (level == 0) {
		std::cout << "Worst delay: " << getWorstMetric(root).second << std::endl;
		std::cout << "Theoretical min-max delay: " << getLowerboundDiameter(root) << std::endl;

		tempLatencyPrintBuffer.clear();
	}

	std::string space(level * 2, ' ');
	auto latency = std::get<3>(sequentialMetricTable[root][node]);
	std::cout << space << "Node ID: " << node << ", Time to reach:" 
		<< latency << std::endl;
	tempLatencyPrintBuffer.push_back(latency);
	
	for (size_t i = 0, sz = routeTable[root][node].size(); i < sz; ++i) {
		auto r = routeTable[root][node][i];

		// recursion wont be too deep in this scenario
		peekTreeDebug(root, r.dst, level + 1);
	}

}


void BlockChainTopologyHelper::peekSubtreeWeightDistribution(int root) {
	std::cout << "subtree weight: ";
	std::vector<int> w(nodeN ,0);
	
	for (int i = 0; i < stableNodeN; ++i) {
		w[std::get<4>(sequentialMetricTable[root][i])-1]++;
	}
	for (int i = 0; i < stableNodeN; ++i) {
		std::cout << w[i] << " ";
	}
	std::cout << std::endl;
}


double BlockChainTopologyHelper::getLowerboundDiameter(int root) {

	double diameter = 0.0;

	std::vector<double> D(stableNodeN);
	std::vector<int> P(stableNodeN);
	utilSP(D, P, root);

	for (int i = 0; i < stableNodeN; ++i) {

		if (i == root) continue;

		double dis = 0.0;
		int c = i;
		int n;

		for (;;) {
			n = P[c];
			dis += getNodeTransferDelay(n, c) + getLinkDelay(n, c) + processingDelay;
			if (n == root) break;
			c = n;
		}

		if (dis > diameter) diameter = dis;
	}

	return diameter;
}


double BlockChainTopologyHelper::getLowerboundAnalysis(int root) {
	// todo
	return -1;
}


} // namespace ns3