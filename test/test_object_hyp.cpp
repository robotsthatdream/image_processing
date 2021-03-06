#include <iostream>

#include <pcl/console/parse.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/sample_consensus/sac_model_sphere.h>
#include <pcl/visualization/pcl_visualizer.h>

#include "../include/image_processing/SurfaceOfInterest.h"
#include <boost/archive/text_iarchive.hpp>
#include <cmm/gmm.hpp>

namespace ip = image_processing;

int main(int argc, char **argv) {

    if (argc != 4) {
        std::cerr << "Usage : \n\t- pcd file\n\t- gmm archive\n\t- label"
                  << std::endl;
        return 1;
    }

    std::string pcd_file = argv[1];
    std::string gmm_archive = argv[2];
    std::string label = argv[3];

    //* Load pcd file into a pointcloud
    ip::PointCloudT::Ptr input_cloud(new ip::PointCloudT);
    pcl::io::loadPCDFile(pcd_file, *input_cloud);
    //*/

    std::cout << "pcd file loaded:" << pcd_file << std::endl;

    //* Load the CMMs classifier from the archive
    std::ifstream ifs(gmm_archive);
    if (!ifs) {
        std::cerr << "Unable to open archive : " << gmm_archive << std::endl;
        return 1;
    }
    cmm::CollabMM gmm;
    boost::archive::text_iarchive iarch(ifs);
    iarch >> gmm;
    //*/

    std::cout << "classifier archive loaded:" << gmm_archive << std::endl;

    //* Generate relevance map on the pointcloud
    ip::SurfaceOfInterest soi(input_cloud);
    std::cout << "computing supervoxel" << std::endl;
    soi.computeSupervoxel();

    std::cout << soi.getSupervoxels().size() << " supervoxels extracted"
              << std::endl;

    std::cout << "computed supervoxel" << std::endl;
    std::cout << "computing meanFPFHLabHist" << std::endl;
    soi.compute_feature("meanFPFHLabHist");
    std::cout << "computed meanFPFHLabHist" << std::endl;
    std::cout << "computing meanFPFHLabHist weights" << std::endl;
    soi.compute_weights<cmm::CollabMM>("meanFPFHLabHist", gmm);
    std::cout << "computed meanFPFHLabHist weights" << std::endl;
    //*/

    std::cout << "relevance_map extracted" << std::endl;

    //* Generate objects hypothesis
    std::vector<std::set<uint32_t>> obj_hypotheses;
    obj_hypotheses = soi.extract_regions("meanFPFHLabHist", 0.5, 1);
    //*/

    // obj_hypotheses

    std::cout << obj_hypotheses.size() << " objects hypothesis extracted"
              << std::endl;

    std::string windowTitle;

    {
        std::stringstream ss;
        ss << "Object fit viewer : " << label;
        windowTitle = ss.str();
    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(
        new pcl::visualization::PCLVisualizer(label));
    viewer->setBackgroundColor(0, 0, 0);

    pcl::PointCloud<pcl::PointXYZRGB> relevance_map_cloud;

    {
        std::string modality = "meanFPFHLabHist";
        int lbl = 1;

        /* Draw all points in dark blueish tint, to see overall scene. */
        ip::PointCloudT::Ptr input_cloud = soi.getInputCloud();

        {
            pcl::PointXYZRGB pt;
            for (auto it_p = input_cloud->begin(); it_p != input_cloud->end();
                 it_p++) {
                // auto current_p = it_p->second;
                pt.x = it_p->x;
                pt.y = it_p->y;
                pt.z = it_p->z;

                pt.r = it_p->r / 8;
                pt.g = it_p->g / 4;
                pt.b = (it_p->r + it_p->g + it_p->b) / 6;
                relevance_map_cloud.push_back(pt);
            }
        }

        ip::SupervoxelArray supervoxels = soi.getSupervoxels();
        ip::SurfaceOfInterest::relevance_map_t weights_for_this_modality =
            soi.get_weights()[modality];

        /* Draw all supervoxels points in various colors. */

        boost::random::mt19937 _gen;
        boost::random::uniform_int_distribution<> dist(4, 7);
        // _gen.seed(0); No seed, we want it deterministic.

        int kept = 0;
        for (auto it_sv = supervoxels.begin(); it_sv != supervoxels.end();
             it_sv++) {
            int current_sv_label = it_sv->first;
            pcl::Supervoxel<ip::PointT>::Ptr current_sv = it_sv->second;
            float c = weights_for_this_modality[it_sv->first][lbl];

            if (c < 0.5) {
                // std::cout << " skipping sv of label " << current_sv_label <<
                // " weight " << c << std::endl;
                continue;
            }
            // std::cout << " KEEPING sv of label " << current_sv_label << "
            // weight " << c << std::endl;
            ++kept;

            // Colors between quarter and half the max.  Not too weak, not too
            // bright.
            int r = float(dist(_gen) << 2) * (c + 1.0);
            int g = float(dist(_gen) << 2) * (c + 1.0);
            int b = float(dist(_gen) << 2) * (c + 1.0);

            pcl::PointXYZRGB pt;
            for (auto v : *(current_sv->voxels_)) {
                pt.x = v.x;
                pt.y = v.y;
                pt.z = v.z;
                pt.r = r;
                pt.g = g;
                pt.b = b;
                relevance_map_cloud.push_back(pt);
            }
        }
        std::cout << "Thresholding kept " << kept << " supervoxels out of "
                  << supervoxels.size() << std::endl;

        // vector < PointCloud<PointXYZ>::Ptr, Eigen::aligned_allocator
        // <PointCloud <PointXYZ>::Ptr > > sourceClouds;

        /* Populate again with cloud fitted with shape. */

        /* We have to express what supervoxels belong together.

           We could copy points, or just set indices, which saves memory.
           Actually, PCL uses indices anyway.

           We don't have to filter again because extract_regions already does
           it.

        */

        // Rappel : typedef std::map<uint32_t, pcl::Supervoxel<PointT>::Ptr>
        // SupervoxelArray;

        // for(auto it_sv = supervoxels.begin(); it_sv != supervoxels.end();
        // it_sv++)
        /* each object */
        for (int obj_index_i; obj_index_i < obj_hypotheses.size();
             obj_index_i++)
        // for(auto it_obj_hyp = obj_hypotheses.begin(); it_obj_hyp !=
        // obj_hypotheses.end(); it_obj_hyp++)
        {
            std::string obj_index_i_s = std::to_string(obj_index_i);
            std::set<uint32_t> *p_obj_hyp = &(obj_hypotheses[obj_index_i]);

            int r = float(dist(_gen) << 4);
            int g = float(dist(_gen) << 4);
            int b = float(dist(_gen) << 4);

            std::cout << std::endl
                      << "Begin new obj hyp, id=" << obj_index_i_s << ", "
                                                                      "color = "
                      << r << "," << g << "," << b << std::endl;

            pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_xyz(
                new pcl::PointCloud<pcl::PointXYZ>);

            if (p_obj_hyp->size() <= 1) {
                std::cerr << "Skipping hypothesis object id=" << obj_index_i_s
                          << " because too few supervoxels: "
                          << p_obj_hyp->size() << std::endl;
                continue;
            }

            {
                int kept = 0;
                pcl::PointXYZ pt;
                for (auto it_sv = supervoxels.begin();
                     it_sv != supervoxels.end(); it_sv++) {
                    int current_sv_label = it_sv->first;
                    pcl::Supervoxel<ip::PointT>::Ptr current_sv = it_sv->second;

                    if (p_obj_hyp->find(current_sv_label) == p_obj_hyp->end()) {
                        // std::cout << "Supervoxel " << current_sv_label << "
                        // not part of current object, skipping." << std::endl;
                        continue;
                    }
                    ++kept;

                    std::cout << "Supervoxel labelled " << current_sv_label
                              << " part of current object, including, "
                                 "will add "
                              << current_sv->voxels_->size() << " point(s)."
                              << std::endl;
                    for (auto v : *(current_sv->voxels_)) {
                        pt.x = v.x;
                        pt.y = v.y;
                        pt.z = v.z;
                        cloud_xyz->push_back(pt);
                    }
                }
                std::cout << "Gathered " << kept
                          << " supervoxels into a point cloud of size "
                          << cloud_xyz->size() << std::endl;
            }

            if (cloud_xyz->size() < 20) {
                std::cerr
                    << "Skipping hypothesis object id=" << obj_index_i_s
                    << " because supervoxels combined into too few points: "
                    << cloud_xyz->size() << std::endl;
                continue;
            }

            std::vector<int> inliers;

            pcl::SampleConsensusModelSphere<pcl::PointXYZ>::Ptr model_s(
                new pcl::SampleConsensusModelSphere<pcl::PointXYZ>(cloud_xyz));

            pcl::RandomSampleConsensus<pcl::PointXYZ> ransac(model_s);
            ransac.setDistanceThreshold(.001);
            ransac.computeModel();
            ransac.getInliers(inliers);

            Eigen::VectorXf coeff;
            ransac.getModelCoefficients(coeff);

            std::cerr << "coeff: " << coeff << std::endl;

            Eigen::VectorXf coeff_refined;
            model_s->optimizeModelCoefficients(inliers, coeff, coeff_refined);
            // EXPECT_EQ (4, coeff_refined.size ());

            std::cerr << "coeff_refined: " << coeff_refined << std::endl;

            pcl::PointCloud<pcl::PointXYZ> proj_points;
            model_s->projectPoints(inliers, coeff_refined, proj_points, false);

            pcl::PointXYZ sphereCenter(coeff[0], coeff[1], coeff[2]);

            std::string sphereId("sphere" + obj_index_i_s);

            std::cerr << "will add sphere with id: " << sphereId << std::endl;

            viewer->addSphere(sphereCenter, coeff[3], ((float)r) / 255.0,
                              ((float)g) / 255.0, ((float)b) / 255.0, sphereId);

            viewer->setShapeRenderingProperties(
                pcl::visualization::PCL_VISUALIZER_REPRESENTATION,
                pcl::visualization::PCL_VISUALIZER_REPRESENTATION_WIREFRAME,
                sphereId);

            std::cerr << "added sphere with id: " << sphereId << std::endl;

            pcl::PointXYZRGB pt;

            for (auto v : proj_points) {
                pt.x = v.x;
                pt.y = v.y;
                pt.z = v.z;

                pt.r = r;
                pt.g = g;
                pt.b = b;
                relevance_map_cloud.push_back(pt);
            }

            // copies all inliers of the model computed to another PointCloud
            pcl::PointCloud<pcl::PointXYZ>::Ptr final(
                new pcl::PointCloud<pcl::PointXYZ>);
            pcl::copyPointCloud<pcl::PointXYZ>(*cloud_xyz, inliers, * final);

            r = r * 2;
            g = g * 2;
            b = b * 2;

            std::cout << std::endl
                      << "Adding to output cloud obj hyp, id=" << obj_index_i_s
                      << ", color = " << r << "," << g << "," << b << std::endl;

            for (auto v : *(final)) {
                pt.x = v.x;
                pt.y = v.y;
                pt.z = v.z;

                pt.r = r;
                pt.g = g;
                pt.b = b;
                relevance_map_cloud.push_back(pt);
            }
            std::cout << "End new obj hyp, id=" << obj_index_i_s << "."
                      << std::endl;
        }
    }

    pcl::PointCloud<pcl::PointXYZRGB>::Ptr relevance_map_cloud_ptr(
        &relevance_map_cloud);

    viewer->addPointCloud<pcl::PointXYZRGB>(relevance_map_cloud_ptr, "cloud");
    // viewer->addCoordinateSystem (1.0);
    viewer->setCameraPosition(0, 0, 0, 0, 0, 1, 0, -1, 0);

    viewer->setPointCloudRenderingProperties(
        pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4, "cloud");
    while (!viewer->wasStopped()) {
        viewer->spinOnce(100);
        boost::this_thread::sleep(boost::posix_time::microseconds(100000));
    }
    return 0;
}
