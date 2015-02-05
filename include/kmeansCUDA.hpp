#pragma once

#include <Eigen/Dense>
#include <iostream>

#include <boost/shared_ptr.hpp>

#include "ddpmeans.hpp"
#include "clDataGpu.hpp"
#include "gpuMatrix.hpp"

#include <euclideanData.hpp>
#include <sphericalData.hpp>

using namespace Eigen;
using std::cout;
using std::endl;

void spkmLabels_gpu( double *d_q,  double *d_p,  uint32_t *d_z,                      
        uint32_t K, uint32_t N);
void spkmLabels_gpu( float *d_q,  float *d_p,  uint32_t *d_z,                      
        uint32_t K, uint32_t N);

void kmeansLabels_gpu( double *d_q,  double *d_p,  uint32_t *d_z, uint32_t K,
    uint32_t N);
void kmeansLabels_gpu( float *d_q,  float *d_p,  uint32_t *d_z, uint32_t K,
    uint32_t N);

template<class T, class DS>
class KMeansCUDA : public KMeans<T,DS>
{
public:
  KMeansCUDA(const shared_ptr<ClDataGpu<T> >& cld);
  virtual ~KMeansCUDA();

  virtual void updateLabels();
  virtual void nextTimeStepGpu(T* d_x, uint32_t N, uint32_t step, uint32_t offset);

  void getZfromGpu() {this->cld_->z();};
  uint32_t* d_z(){ return this->cdl_->d_z();};

protected:

  GpuMatrix<T> d_p_;
  virtual void setupComputeLabelsGPU();
};

// ------------------------- impl --------------------------------------
template<class T, class DS>
KMeansCUDA<T,DS>::KMeansCUDA( const shared_ptr<ClDataGpu<T> >& cld)
  : KMeans<T,DS>(cld), d_p_(this->D_,cld->K())
{}

template<class T, class DS>
void KMeansCUDA<T,DS>::nextTimeStepGpu(T* d_x, uint32_t N, uint32_t step,
    uint32_t offset) 
{ 
  this->cls_.clear();
  for (uint32_t k=0; k<K_; ++k)
    cls_.push_back(shared_ptr<typename DS::DependentCluster >(new typename DS::DependentCluster()));

  this->cld_->updateData(d_x,N,step,offset);
  this->N_ = this->cld_->N();
  this->cld_->randomLabels(K_);
  this->cld_->updateLabels(K_);
  this->cld_->computeSS();
  for(uint32_t k=0; k<this->K_; ++k)
    this->cls_[k]->updateCenter(this->cld_,k);
};

template<class T, class DS>
void KMeansCUDA<T,DS>::setupComputeLabelsGPU();
{
  Matrix<T,Dynamic,Dynamic> ps(this->D_,this->K_);
  for(uint32_t k=0; k<this->K_; ++k)
    ps.col(k) = this->cls_[k]->centroid();
  d_p_.set(ps);
};

template<class T, class DS>
void KMeansCUDA<T,DS>::updateLabels()
{
  this->setupComputeLabelsGPU();
  assert(false);
};

template<>
void KMeansCUDA<double,Spherical<double> >::updateLabels()
{
  this->setupComputeLabelsGPU();
  spkmLabels_gpu(this->cld->d_x(),d_p_.data(),this->cld->d_z(),
      this->cld->N());
}

template<>
void KMeansCUDA<float,Spherical<float> >::updateLabels()
{
  this->setupComputeLabelsGPU();
  spkmLabels_gpu(this->cld->d_x(),d_p_.data(),this->cld->d_z(),
      this->cld->N());
}

template<>
void KMeansCUDA<double,Euclidean<double> >::updateLabels()
{
  this->setupComputeLabelsGPU();
  kmeansLabels_gpu(this->cld->d_x(),d_p_.data(),this->cld->d_z(),
      this->cld->N());
}

template<>
void KMeansCUDA<float,Euclidean<float> >::updateLabels()
{
  this->setupComputeLabelsGPU();
  kmeansLabels_gpu(this->cld->d_x(),d_p_.data(),this->cld->d_z(),
      this->cld->N());
}