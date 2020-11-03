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

    uint32_t ct = 0;
    for (auto &i : cityList) {
      i.second.second = ct++;
    }

    for (auto &link : linkList) {
      link.noFrom = cityList[link.idFrom].second;
      link.noTo = cityList[link.idTo].second;

    }

    return linkList;
  }

  void GeoSimulationTopologyHelper::setSelectedCity(std::set<uint32_t> s) {
    selectedCityList = s;
    selector = true;
  }

  bool GeoSimulationTopologyHelper::isSelected(uint32_t id) {
    return selectedCityList.find(id) != selectedCityList.end();
  }
  
}