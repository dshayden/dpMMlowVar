#pragma once

#include <Eigen/Dense>
#include <iostream>
#include <vector>

#include <boost/random/mersenne_twister.hpp>
#include <boost/shared_ptr.hpp>

#include "sphere.hpp"
#include "dpmeans.hpp"
#include "dir.hpp"
#include "cat.hpp"

using namespace Eigen;
using std::cout;
using std::endl;

#ifdef BOOST_OLD
#define mt19937 boost::mt19937
#else
using boost::mt19937;
#endif

#define UNASSIGNED 4294967295

template<class T, class DS>
class DDPMeans : public DPMeans<T,DS>
{
public:
  DDPMeans(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx,
      T lambda, T Q, T tau, mt19937* pRndGen);
  virtual ~DDPMeans();

//  void initialize(const Matrix<T,Dynamic,Dynamic>& x);
  virtual void updateLabels();
  virtual void updateCenters();
  
  virtual void nextTimeStep(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx);
  virtual void updateState(); // after converging for a single time instant

  virtual uint32_t indOfClosestCluster(int32_t i, T& sim_closest);
  Matrix<T,Dynamic,Dynamic> prevCentroids(){ return psPrev_;};

protected:

  std::vector<T> ts_; // age of clusters - incremented each iteration
  std::vector<T> ws_; // weights of each cluster 
  T Q_; // Q parameter
  T tau_; // tau parameter 
  T Kprev_; // K before updateLabels()
  Matrix<T,Dynamic,Dynamic> psPrev_; // centroids from last set of datapoints
};

// -------------------------------- impl ----------------------------------
template<class T, class DS>
DDPMeans<T,DS>::DDPMeans(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx, 
    T lambda, T Q, T tau, mt19937* pRndGen)
  : DPMeans<T,DS>(spx,0,lambda,pRndGen), Q_(Q), tau_(tau)
{
  // compute initial counts for weight initialization
//#pragma omp parallel for
//  for (uint32_t k=0; k<this->K_; ++k)
//    for(uint32_t i=0; i<this->N_; ++i)
//      if(this->z_(i) == k)
//        this->Ns_(k) ++;
//  for (uint32_t k=0; k<this->K_; ++k)
//  {
//    ws_.push_back(this->Ns_(k));
//    ts_.push_back(1);
//  }
  this->Kprev_ = 0; // so that centers are initialized directly from sample mean
  psPrev_ = this->ps_;
}

template<class T, class DS>
DDPMeans<T,DS>::~DDPMeans()
{}

template<class T, class DS>
uint32_t DDPMeans<T,DS>::indOfClosestCluster(int32_t i, T& sim_closest)
{
  int z_i = this->K_;
  sim_closest = this->lambda_;
//  cout<<"K="<<this->K_<<" Ns:"<<this->Ns_.transpose()<<endl;
//  cout<<"cluster dists "<<i<<": "<<this->lambda_;
  for (uint32_t k=0; k<this->K_; ++k)
  {
    T sim_k = DS::dist(this->ps_.col(k), this->spx_->col(i));
    if(this->Ns_(k) == 0) // cluster not instantiated yet in this timestep
    {
      //TODO use gamma
//      T gamma = 1.0/(1.0/ws_[z_i] + ts_[z_i]*tau_);
      sim_k = sim_k/(tau_*ts_[k]+1.+ 1.0/ws_[k]) + Q_*ts_[k];
//      sim_k = sim_k/(tau_*ts_[k]+1.) + Q_*ts_[k];
//      sim_k = sim_k/(tau_*ts_[k]+1.) + Q_*ts_[k];
    }
//    cout<<" "<<sim_k;
    if(DS::closer(sim_k, sim_closest))
    {
      sim_closest = sim_k;
      z_i = k;
    }
  }
//  }cout<<endl;
  return z_i;
}

template<class T, class DS>
void DDPMeans<T,DS>::updateLabels()
{
  this->prevCost_ = this->cost_;
  this->cost_ = 0.; // TODO:  does this take into account that creating a cluster costs
  for(uint32_t i=0; i<this->N_; ++i)
  {
    T sim = 0.;
    uint32_t z_i = indOfClosestCluster(i,sim);
    this->cost_ += sim;
    if(z_i == this->K_) 
    { // start a new cluster
      Matrix<T,Dynamic,Dynamic> psNew(this->D_,this->K_+1);
      psNew.leftCols(this->K_) = this->ps_;
      psNew.col(this->K_) = this->spx_->col(i);
      this->ps_ = psNew;
      this->K_ ++;
      this->Ns_.conservativeResize(this->K_); 
      this->Ns_(z_i) = 1.;
    } else {
      if(this->Ns_[z_i] == 0)
      { // instantiated an old cluster
        T gamma = 1.0/(1.0/ws_[z_i] + ts_[z_i]*tau_);
        this->ps_.col(z_i)=(this->ps_.col(z_i)*gamma + this->spx_->col(i))/(gamma+1.);
      }
      this->Ns_(z_i) ++;
    }
    if(this->z_(i) != UNASSIGNED)
    {
      this->Ns_(this->z_(i)) --;
    }
    this->z_(i) = z_i;
  }
};

template<class T, class DS>
void DDPMeans<T,DS>::updateCenters()
{
#pragma omp parallel for 
  for(uint32_t k=0; k<this->K_; ++k)
  {
     Matrix<T,Dynamic,1> mean_k = DS::computeCenter(*this->spx_,this->z_,k,
       &this->Ns_(k));
//    Matrix<T,Dynamic,1> mean_k = this->computeCenter(k);
    if (this->Ns_(k) > 0) 
    { // have data to update kth cluster
      if(k < this->Kprev_){
        T gamma = 1.0/(1.0/ws_[k] + ts_[k]*tau_);
        this->ps_.col(k) = (this->ps_.col(k)*gamma+mean_k*this->Ns_(k))/
          (gamma+this->Ns_(k));
      }else{
        this->ps_.col(k)=mean_k;
      }
    }
  }
};

template<class T, class DS>
void DDPMeans<T,DS>::nextTimeStep(const shared_ptr<Matrix<T,Dynamic,Dynamic> >& spx)
{
  assert(this->D_ == spx->rows());
  this->spx_ = spx; // update the data
  this->N_ = spx->cols();
  this->z_.resize(this->N_);
  this->z_.fill(0);
//  this->z_.fill(UNASSIGNED);
};

template<class T, class DS>
void DDPMeans<T,DS>::updateState()
{
  for(uint32_t k=0; k<this->K_; ++k)
  {
    if (k<ws_.size() && this->Ns_(k) > 0)
    { // instantiated cluster from previous time; 
      ws_[k] = 1./(1./ws_[k] + ts_[k]*tau_) + this->Ns_(k);
      ts_[k] = 0; // re-instantiated -> age is 0
    }else if(k >= ws_.size()){
      // new cluster
      ts_.push_back(0);
      ws_.push_back(this->Ns_(k));
    }
    ts_[k] ++; // increment all ages
    cout<<"cluster "<<k
      <<"\tN="<<this->Ns_(k)
      <<"\tage="<<ts_[k]
      <<"\tweight="<<ws_[k]<<endl;
    cout<<"  center: "<<this->ps_.col(k).transpose()<<endl;
  }
  psPrev_ = this->ps_;
  this->Kprev_ = this->K_;
};
