#include "GeoSimulationTopologyHelper.h"

namespace ns3 {

  GeoSimulationTopologyHelper::GeoSimulationTopologyHelper(std::string jsonFile) {
    boost::property_tree::read_json(jsonFile, root);
  }

  GeoSimulationTopologyHelper::~GeoSimulationTopologyHelper() {
    
  }

  void GeoSimulationTopologyHelper::_read() {
    int srcId, dstId;
    std::string srcName, dstName;
    std::string max, min, avg;
    for (boost::property_tree::ptree::value_type& src : root.get_child("pingData")) {
      srcId = std::stoi(src.first);
      
      if (selector) {
        if (!isSelected(srcId)) continue;
      } 
      
      for (boost::property_tree::ptree::value_type& dst : src.second) {
        
        dstId = std::stoi(dst.first);
        
        if (selector) {
          if (!isSelected(dstId)) continue;
        }

        if (srcId == dstId) continue;
        
        srcName = dst.second.get_child("source_name").data();
        dstName = dst.second.get_child("destination_name").data();
        max = dst.second.get_child("max").data();
        min = dst.second.get_child("min").data();
        avg = dst.second.get_child("avg").data();
        if (max != "null" && min != "null" && avg != "null") {
          DelayInfo newDelayInfo;
          newDelayInfo.idFrom = srcId;
          newDelayInfo.idTo = dstId;
          newDelayInfo.cityFrom = srcName;
          newDelayInfo.cityTo = dstName;
          newDelayInfo.maxDelay = std::stod(max) / 1000.0;
          newDelayInfo.minDelay = std::stod(min) / 1000.0;
          newDelayInfo.avgDelay = std::stod(avg) / 1000.0;

          cityList.insert(std::pair<uint32_t, std::pair<std::string, uint32_t> >
            (dstId, std::pair<std::string, uint32_t>(dstName, 0)));

          linkList.push_back(newDelayInfo);
        }
      }
    }
  }

  std::vector<GeoSimulationTopologyHelper::DelayInfo> GeoSimulationTopologyHelper::parse() {
    
    _read();

    cityN = 0;
    for (auto &i : cityList) {
      i.second.second = cityN++;
    }

    for (auto &link : linkList) {
      link.noFrom = cityList[link.idFrom].second;
      link.noTo = cityList[link.idTo].second;
    }

    return linkList;
  }


  std::vector<GeoSimulationTopologyHelper::DelayInfo> GeoSimulationTopologyHelper::getClique(uint32_t n) {
    
    std::vector<DelayInfo> res;

    // origin id;
    std::set<uint32_t> addedNode;

    // origin id
    std::vector<std::pair<uint32_t, std::pair<double, int> > > nodeStatistic;

    _read();

    for (auto link: linkList) {
      bool found = false;
      for (auto &node: nodeStatistic) {
        if (node.first == link.idFrom) {
          node.second.first += link.avgDelay;
          node.second.second++;
          found = true;
          break;
        }
      }
      if (!found) {
        nodeStatistic.push_back(std::make_pair(link.idFrom, std::make_pair(link.avgDelay, 1)));
      }
    }

    for (auto &node: nodeStatistic) {
      node.second.first = node.second.first / (node.second.second + 2e-40);
    }

    std::sort(nodeStatistic.begin(), nodeStatistic.end(), []
      (std::pair<uint32_t, std::pair<double, int> > a, std::pair<uint32_t, std::pair<double, int> > b){
        return a.second.first < b.second.first;
      });

    uint32_t i = 0;
    for (auto add: nodeStatistic) {
      bool fullconnect = true;
      for (auto exist: addedNode) {
        if (!hasLink(add.first, exist)) {
          fullconnect = false;
          break;
        }
      }
      if (fullconnect) {
        addedNode.insert(add.first);
        cliqueCityList[add.first] = cityList[add.first];
        cliqueCityList[add.first].second = i;
        if (++i >= n) break;
      }
    }

    auto inClique = [addedNode](uint32_t n){return addedNode.find(n) != addedNode.end();};

    for (auto link: linkList) {
      if (inClique(link.idTo) && inClique(link.idFrom)) {
        link.noFrom = cliqueCityList[link.idFrom].second;
        link.noTo = cliqueCityList[link.idTo].second;
        res.push_back(link);
      }
    }

    return res;

  }


  void GeoSimulationTopologyHelper::setSelectedCity(std::set<uint32_t> s) {
    selectedCityList = s;
    selector = true;
  }

  bool GeoSimulationTopologyHelper::isSelected(uint32_t id) {
    return selectedCityList.find(id) != selectedCityList.end();
  }

  // origin id
  bool GeoSimulationTopologyHelper::hasLink(uint32_t a, uint32_t b) {
    for (auto link: linkList) {
      if ((link.idFrom == a && link.idTo == b) || (link.idFrom == b && link.idTo == a)) {
        return true;
      }
    }
    return false;
  }
  
}