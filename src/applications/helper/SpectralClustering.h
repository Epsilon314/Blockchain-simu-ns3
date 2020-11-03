#ifndef SPECTRALCLUSTERING_H
#define SPECTRALCLUSTERING_H

#include <eigen3/Eigen/Dense>
#include <vector>
#include <random>
#include <chrono>

namespace ns3 {

typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> matrix;

class SpectralClustering {

public:

  SpectralClustering() {}
  ~SpectralClustering() {}

  std::vector<int> spectral(int k, double thrd, int retry);
  void k_means(std::vector<std::vector<double> > &Nodes, int k, double thrd = 0.001, int retry = 10000);

  void setAffineMatrix(std::vector<std::vector<double> > &m);
  
private:

  matrix Aff;

  std::vector<std::vector<double> > nodes;
  int k_cluster, n_nodes, dim;
  
  std::vector<int> clusterAssign;
  std::vector<std::vector<double> > means;
  
  matrix diagonal(matrix &A);
  matrix laplacian(matrix &A);
  std::pair<matrix, matrix> decompositon(matrix &A);

  void k_means_init(std::vector<std::vector<double> > &Nodes, int k);

  double caculate_means();
  void assign_cluster();

  double euclidean_dist(std::vector<double> a, std::vector<double> b);
};

}// namespace ns3

#endif