#ifndef GEOSIMULATIONTOPOLOGYHELPER_H
#define GEOSIMULATIONTOPOLOGYHELPER_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <map>
#include <vector>
#include <set>

namespace ns3 {

class GeoSimulationTopologyHelper {

public:

  GeoSimulationTopologyHelper(std::string jsonFile);
  ~GeoSimulationTopologyHelper();

  struct DelayInfo {
    std::string cityFrom;
    std::string cityTo;

    // unique id from the input file, not continous
    uint32_t idFrom;
    uint32_t idTo;

    // re-numbered id after selection
    uint32_t noFrom;
    uint32_t noTo;

    double minDelay;
    double maxDelay;
    double avgDelay;
  };

  void setSelectedCity(std::set<uint32_t> s);
  std::map<uint32_t, std::pair<std::string, uint32_t> > getSelectedCityResult() {
    return cityList;
  }

  std::vector<DelayInfo> parse();
  
  std::map<uint32_t, std::pair<std::string, uint32_t> > getCityList() {return cityList;}


private:

  boost::property_tree::ptree root;

  std::vector<DelayInfo> linkList;
  std::map<uint32_t, std::pair<std::string, uint32_t> > cityList;

  bool selector = false;
  std::set<uint32_t> selectedCityList;

  void _read();

  bool isSelected(uint32_t id);

};



} // namespace ns3
#endif 