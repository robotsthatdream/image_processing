#ifndef _SURFACE_OF_INTEREST_H
#define _SURFACE_OF_INTEREST_H

#include "SupervoxelSet.h"
#include "HistogramFactory.hpp"
#include <boost/random.hpp>
#include <ctime>
#include <image_processing/features.hpp>
#include <tbb/tbb.h>

namespace image_processing {


typedef struct SvFeature{

    SvFeature(){}
    SvFeature(std::vector<double> c, std::vector<double> n) :
        color(c), normal(n){}

    std::vector<double> color;
    std::vector<double> normal;
}SvFeature;

inline std::ostream& operator<<(std::ostream& os, const SvFeature& feature){

    os << "color : ";
    for(auto c : feature.color)
        os << c << ";";

    os << "normal : ";
    for(auto n : feature.normal)
        os << n << ";";

    return os;
}

/**
 * @brief class to build a relevance map : which is a segmentation between different categories.
 */
class SurfaceOfInterest : public SupervoxelSet
{
public:

    typedef std::map<uint32_t,std::vector<double>> relevance_map_t;
    /**< map of probabilities associate to each supervoxel (key sv label). Value : vector of probabilities. The size is equal to the number of class */

    /**
     * @brief default constructor
     */
    SurfaceOfInterest() : SupervoxelSet(){
        srand(time(NULL));
        _gen.seed(rand());
    }

    /**
     * @brief constructor with a given input cloud
     * @param input cloud
     */
    SurfaceOfInterest(const PointCloudT::Ptr& cloud) : SupervoxelSet(cloud){
        srand(time(NULL));
        _gen.seed(rand());
    }
    /**
     * @brief copy constructor
     * @param soi
     */
    SurfaceOfInterest(const SurfaceOfInterest& soi) :
        SupervoxelSet(soi),
        _weights(soi._weights),
        _labels(soi._labels),
        _labels_no_soi(_labels_no_soi),
        _gen(soi._gen){}

    /**
     * @brief constructor with a SupervoxelSet
     * @param super
     */
    SurfaceOfInterest(const SupervoxelSet& super) : SupervoxelSet(super){
        srand(time(NULL));
        _gen.seed(rand());
    }

    /**
     * @brief methode to find soi, with a list of keypoints this methode search in which supervoxel are this keypoints
     * @param key_pts
     */
    void find_soi(const PointCloudXYZ::Ptr key_pts);

    /**
     * @brief NAIVE POLICY generate the soi for a pure random choice (i.e. all supervoxels are soi)
     * @param workspace
     * @return if the generation of soi is successful
     */
    bool generate(workspace_t& workspace);

    /**
     * @brief LEARNING POLICY generate the soi with a classifier specify in argument
     * @param the classifier
     * @param workspace
     * @return if the generation of soi is successful
     */
    template <typename classifier_t>
    bool generate(const std::string &modality, classifier_t& classifier, workspace_t& workspace, float init_val = 1.){
        if(!computeSupervoxel(workspace))
            return false;

        init_weights(modality,classifier.get_nbr_class(),init_val);
        compute_weights(modality, classifier);
        return true;
    }

    /**
     * @brief KEY POINTS POLICY generate the soi with key points. Soi will be supervoxels who contains at least one key points.
     * @param key points
     * @param workspace
     * @return if the generation of soi is successful
     */
    bool generate(const PointCloudXYZ::Ptr key_pts, workspace_t &workspace);

    /**
     * @brief EXPERT POLICY generate the soi by deleting the background
     * @param pointcloud of the background
     * @param workspace
     * @return if the generation of soi is successful
     */
    bool generate(const PointCloudT::Ptr background, workspace_t& workspace);

    /**
     * @brief reduce the set of supervoxels to set of surface of interest. Only supervoxels with weight above certain threshold
     * @param modality
     * @param threshold
     * @param cat
     */
    void reduce_to_soi(const std::string &modality, double threshold = 0.5,int cat = 1);

    void init_weights(const std::string& modality,int nbr_class, float value = 1.);

    /**
     * @brief methode compute the weight of each supervoxels. The weights represent the probability for a soi to be explored.
     * All weights are between 0 and 1.
     * @param lbl label of the explored supervoxel
     * @param interest true if the explored supervoxel is interesting false otherwise
     */
    template <typename classifier_t>
    void compute_weights(const std::string& modality, const classifier_t &classifier){


        if(_features.begin()->second.find(modality) == _features.begin()->second.end()){
            std::cerr << "SurfaceOfInterest Error: unknow modality : " << modality << std::endl;
            return;
        }

        if(_weights.find(modality) != _weights.end())
            _weights[modality].clear();
        else
            _weights[modality] = relevance_map_t();

        std::vector<uint32_t> lbls;
        for(const auto& sv : _supervoxels){
            lbls.push_back(sv.first);
            _weights[modality].emplace(sv.first,std::vector<double>(classifier.get_nbr_class(),0.5));
        }

        tbb::parallel_for(tbb::blocked_range<size_t>(0,lbls.size()),
                          [&](const tbb::blocked_range<size_t>& r){
            for(size_t i = r.begin(); i != r.end(); ++i){
                //        for(size_t i = 0; i != lbls.size(); ++i){
                _weights[modality][lbls[i]] = classifier.compute_estimation(
                            _features[lbls[i]][modality]);
            }
        });

    }


    /**
     * @brief variant of compute weights with composition of classifiers
     * @param modality
     * @param classifier
     * @param comp_classifier
     */
    template <typename classifier_t>
    void compute_weights(const std::string& modality, const classifier_t &classifier,
                         const classifier_t &comp_classifier){


        if(_features.begin()->second.find(modality) == _features.begin()->second.end()){
            std::cerr << "SurfaceOfInterest Error: unknow modality : " << modality << std::endl;
            return;
        }

        if(_weights.find(modality) != _weights.end())
            _weights[modality].clear();
        else
            _weights[modality] = relevance_map_t();

        std::vector<uint32_t> lbls;
        for(const auto& sv : _supervoxels){
            lbls.push_back(sv.first);
            _weights[modality].emplace(sv.first,std::vector<double>(classifier.get_nbr_class(),0.5));
        }


        tbb::parallel_for(tbb::blocked_range<size_t>(0,lbls.size()),
                          [&](const tbb::blocked_range<size_t>& r){
            for(size_t i = r.begin(); i != r.end(); ++i){
                //        for(size_t i = 0; i != lbls.size(); ++i){

                std::vector<double> comp_est = comp_classifier.compute_estimation(
                            _features[lbls[i]][modality]);
                std::vector<double> estimations = classifier.compute_estimation(
                            _features[lbls[i]][modality]);
                for(int k = 0; k < estimations.size(); k++)
                    estimations[k] = comp_est[k]*estimations[k];
                _weights[modality][lbls[i]] = estimations;
            }
        });
    }

    template<typename classifier_t>
    /**
     * @brief compute weights for multi-classifier system
     * @param multiclassifier system
     */
    void compute_weights(classifier_t classifier){

        if(_weights.find("merge") != _weights.end())
            _weights["merge"].clear();
        else
            _weights["merge"] = relevance_map_t();

        std::vector<uint32_t> lbls;
        for(const auto& sv : _supervoxels){
            lbls.push_back(sv.first);
            _weights["merge"].emplace(sv.first,std::vector<double>(classifier.get_nbr_class(),0));
        }
        tbb::parallel_for(tbb::blocked_range<size_t>(0,lbls.size()),
                          [&](const tbb::blocked_range<size_t>& r){
            for(size_t i = r.begin(); i != r.end(); ++i){
                _weights["merge"][lbls[i]] = classifier.compute_estimation(
                            _features[lbls[i]]);
            }
        });

    }

    template <typename classifier_t>
    /**
     * @brief compute weights for a list of classifier each specific to one kind of feature
     * @param classifier associated with a feature.
     */
    void compute_weights(std::map<std::string,classifier_t>& classifiers){
        for(auto& classi: classifiers)
        {
            if(_features.begin()->second.find(classi.first) == _features.begin()->second.end()){
                std::cerr << "SurfaceOfInterest Error: unknow modality : " << classi.first << std::endl;
                continue;
            }

            if(_weights.find(classi.first) != _weights.end())
                _weights[classi.first].clear();
            else
                _weights[classi.first] = relevance_map_t();


            for(const auto& sv : _supervoxels){
                _weights[classi.first].emplace(sv.first,
                                               classi.second.compute_estimation(_features[sv.first][classi.first]));
            }
        }
    }

    /**
     * @brief choose randomly one soi
     * @param supervoxel
     * @param label of chosen supervoxel in the soi set
     */
    bool choice_of_soi(const std::string &modality, pcl::Supervoxel<PointT> &supervoxel, uint32_t& lbl);
    bool choice_of_soi_by_uncertainty(const std::string &modality, pcl::Supervoxel<PointT> &supervoxel, uint32_t &lbl);

    /**
     * @brief delete the background of the input cloud
     * @param a pointcloud
     */
    void delete_background(const PointCloudT::Ptr background);

    /**
     * @brief compute the pointcloud colored by the weights of the given modality
     * @param modality
     * @return the pointcloud colored by the weights of the given modality
     */
    pcl::PointCloud<pcl::PointXYZI> getColoredWeightedCloud(const std::string &modality,int lbl);

    /**
     * @brief return a map that link a supervoxel to the id of an object
     * @param modality
     * @param saliency_threshold
     */
    std::map<pcl::Supervoxel<PointT>::Ptr, int> get_supervoxels_clusters(const std::string &modality, double &saliency_threshold,int lbl);

    /**
     * @brief get the weights of all modality
     * @return the weights of all modality
     */
    std::map<std::string,relevance_map_t> get_weights(){return _weights;}

    /**
     * @brief neighbor bluring propagate weights of each supervoxels to its neighbor. Experimental function.
     * @param modality
     * @param constant of incrementation or decrementation to modify the weights of the neighbprs
     * @param label of the considered class
     */
    void neighbor_bluring(const std::string& modality, double cst, int lbl);

    /**
     * @brief produce a binary map based on an adpative threshold thanks to neighborhood. Experimental function.
     * @param feature considered
     * @param label of the considered class
     */
    void adaptive_threshold(const std::string& modality, int lbl);

    /**
     * @brief compute a average relevance map based on several relevance maps
     * @param list of relevance maps
     * @return
     */
    pcl::PointCloud<pcl::PointXYZI> cumulative_relevance_map(std::vector<pcl::PointCloud<pcl::PointXYZI>> list_weights);

    /**
     * @brief compute regions of salient supervoxels for the given modality and threshold
     * @param modality
     * @param saliency threshold
     * @return a set of regions (a region is a vector of supervoxels' labels)
     */
    std::vector<std::set<uint32_t>> extract_regions(const std::string &modality, double saliency_threshold, int class_lbl);

    /**
     * @brief compute the closest region in a vector for the given center
     * @param vector of regions
     * @param center
     * @return the indice of the closest region in the input vector or -1 if the input vector is empty
     */
    size_t get_closest_region(const std::vector<std::set<uint32_t>> regions, const Eigen::Vector4d center);

    /**
     * @brief compute non salient supervoxels for the given modality and threshold
     * @param modality
     * @param saliency threshold
     * @return a set of supervoxels' labels that are not salient
     */
    std::set<uint32_t> extract_background(const std::string &modality, double saliency_threshold, int class_lbl);

private :
    std::vector<uint32_t> _labels;
    std::vector<uint32_t> _labels_no_soi;
    std::map<std::string,relevance_map_t> _weights;

    boost::random::mt19937 _gen;

};

}

#endif //_SURFACE_OF_INTEREST_H
