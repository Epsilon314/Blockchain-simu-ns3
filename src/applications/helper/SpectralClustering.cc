#include "SpectralClustering.h"
#include <iostream>

namespace ns3 {

void SpectralClustering::setAffineMatrix(std::vector<std::vector<double> > &m) {
  int n = m.size();
  Aff.resize(n, n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < n; ++j) {
      (Aff)(i, j) = m[i][j];
    }
  }
}

matrix SpectralClustering::diagonal(matrix &A) {
  int n = A.rows();
  double d;
  matrix D;
  D.resize(n, n);
  for (int i = 0; i < n; ++i) {
    d = 0;
    for (int j = 0; j < n; ++j) {
      d += (A)(i,j);
    }
    D(i, i) = d;
  }
  return D;
}


matrix SpectralClustering::laplacian(matrix &A) {
  return diagonal(A) - A;
}


std::pair<matrix, matrix> SpectralClustering::decompositon(matrix &A) {

  matrix D = diagonal(A);
  matrix L = D - A;

  Eigen::GeneralizedSelfAdjointEigenSolver<matrix> solve(L, D);

  return std::pair<matrix, matrix>(solve.eigenvalues(), solve.eigenvectors());
}


std::vector<int> SpectralClustering::spectral(int k, double thrd, int retry) {

  std::pair<matrix, matrix> decomp = decompositon(Aff);

  int n = Aff.rows();
  std::vector<std::vector<double> > eigenvectors(n, std::vector<double>(k));
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < k; ++j) {
      eigenvectors[i][j] = (decomp.second)(i, j);
    }
  }
  k_means(eigenvectors, k, thrd, retry);
  return clusterAssign;
}


double SpectralClustering::euclidean_dist(std::vector<double> a, std::vector<double> b) {
  double res = 0.0;
  for (size_t i = 0; i < a.size(); ++i) {
    double diff = a[i] - b[i];
    res += diff * diff;
  }
  return res;
}


void SpectralClustering::k_means_init(std::vector<std::vector<double> > &Nodes, int k) {

  nodes = Nodes;

  n_nodes = Nodes.size();
  dim = Nodes[0].size();
  k_cluster = k;

  clusterAssign.clear();
  clusterAssign.resize(n_nodes);

  means.clear();
  means.resize(k, std::vector<double>(dim));

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine generator = std::default_random_engine(seed);
  std::uniform_int_distribution<int> distri(0, k-1);

  for (auto &cluster : clusterAssign) {
    cluster = distri(generator);
  }
}


double SpectralClustering::caculate_means() {

  double res = 0.0;

  std::vector<int> cluster_node_count(k_cluster, 0);

  auto old_means = means;

  for (auto &cluster : means) {
    for (auto &coord : cluster) {
      coord = 0.0;
    }
  }

  for (int i = 0; i < n_nodes; ++i) {
    auto current_node_cluster_assign = clusterAssign[i];
    cluster_node_count[current_node_cluster_assign]++;
    for (size_t j = 0; j < means[current_node_cluster_assign].size(); ++ j) {
      means[current_node_cluster_assign][j] += nodes[i][j];
    }
  }

  for (int i = 0; i < k_cluster; ++i) {
    // std::cout << "cluster count" << cluster_node_count[i] << std::endl;
    if (cluster_node_count[i] > 0) {
      for (double &coord : means[i]) {
        coord = coord / cluster_node_count[i];
      }
    }
    else {
      return -1;
    }
    res += euclidean_dist(means[i], old_means[i]);
  }
  // std::cout << "res:" << res << std::endl;
  return res;
}


void SpectralClustering::assign_cluster() {
  
  double dist, s_dist = std::numeric_limits<double>::infinity();
  int new_assign;
  
  for (int i = 0; i < n_nodes; ++i) {
    for (int j = 0; j < k_cluster; ++j) {
      dist = euclidean_dist(nodes[i], means[j]);
      if (dist < s_dist) {
        new_assign = j;
        s_dist = dist;
      }
    }
    clusterAssign[i] = new_assign;
  }
}

  
void SpectralClustering::k_means(std::vector<std::vector<double> > &Nodes, int k, double thrd, int retry) {
  
  double dist;
  int tries = 0;
  
  while (tries++ < retry) {
    k_means_init(Nodes, k);

    dist = caculate_means();
    while (dist > thrd) {
      assign_cluster();
      dist = caculate_means();
      if (dist == -1) break;
    }
    if (dist == -1) continue;
    return; // successed
  }
  std::cerr<<"cluster failed"<<std::endl;
  exit(EXIT_FAILURE);
  // failed
}

} // namespace ns3