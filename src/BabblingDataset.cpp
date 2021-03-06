#include "image_processing/BabblingDataset.h"

#if CV_MAJOR_VERSION == 4
#include "opencv2/imgcodecs/imgcodecs_c.h" // for CV_LOAD_IMAGE_COLOR and others, since OpenCV 4 alpha.
#endif


using namespace image_processing;

bool BabblingDataset::_load_data_structure(const std::string &meta_data_filename){
    std::cout << "_load_data_structure" << std::endl;


    YAML::Node meta_data = YAML::LoadFile(meta_data_filename);
    if(meta_data.IsNull()){
        std::cerr << "file not found" << std::endl;
        return false;
    }

   _data_structure = meta_data["data_structure"];

   _load_hyperparameters(meta_data["experiment"]);

   return true;
}

void BabblingDataset::_load_iteration_folders(const std::string& arch_name){
    std::cout << "_load_iteration_folders" << std::endl;

    boost::filesystem::directory_iterator end_itr;
    for(boost::filesystem::directory_iterator itr(arch_name); itr != end_itr; ++itr){
        std::vector<std::string> split_str;
        boost::split(split_str,itr->path().string(),boost::is_any_of("/"));
        boost::split(split_str,split_str.back(),boost::is_any_of("_"));
        if(!std::strcmp(split_str[0].c_str(),"iteration"))
            _iterations_folders.emplace(std::stoi(split_str[1]),itr->path().string());
    }
}

bool BabblingDataset::_load_data_iteration(const std::string &foldername, rgbd_set_t& rgbd_set,
                                           rect_trajectories_t& rect_traj,
                                           arm_trajectories_t &arm_traj){

    std::cout << "_load_data_iteration" << std::endl;

    std::string folder;

    if(_data_structure["folder_prefix"].IsDefined())
        folder = foldername + _data_structure["folder_prefix"].as<std::string>();
    else folder = foldername;

    if(!boost::filesystem::exists(folder)){
        std::cerr << "unable to open folder " << foldername << std::endl;
        return false;
    }

    _load_motion_rects(folder+ "/" + _data_structure["motion"].as<std::string>(),rect_traj);
    _load_rgbd_images(folder,rect_traj,rgbd_set);
    _load_arm_trajectories(folder + "/" + _data_structure["joints_values"].as<std::string>(),arm_traj);

    return true;
}

bool BabblingDataset::_load_motion_rects(const std::string &filename, rect_trajectories_t& rect_traj){
    std::cout << "_load_motion_rects" << std::endl;

    MotionDetection md;

    YAML::Node node = YAML::LoadFile(filename);
    if(node.IsNull()){
        std::cerr << "unable to open file " << filename << std::endl;
        return false;
    }

    auto itr = node.begin();
//    std::string key = itr->first.as<std::string>() ;
//    double init_time = node[key]["timestamp"]["sec"].as<double>()
//            + node[key]["timestamp"]["nsec"].as<double>()*1e-9;
    for(; itr != node.end(); ++itr){
        std::string key = itr->first.as<std::string>();
        double time = node[key]["timestamp"]["sec"].as<double>() + node[key]["timestamp"]["nsec"].as<double>()*1e-9/* - init_time*/;
        std::vector<cv::Rect> rect_vect;
        for(auto itr_rect = node[key]["rects"].begin(); itr_rect != node[key]["rects"].end(); ++itr_rect){
            std::string rect_key = itr_rect->first.as<std::string>();
            cv::Rect rect;
            rect.x = node[key]["rects"][rect_key]["x"].as<int>();
            rect.y = node[key]["rects"][rect_key]["y"].as<int>();
            rect.width = node[key]["rects"][rect_key]["width"].as<int>();
            rect.height = node[key]["rects"][rect_key]["height"].as<int>();
            rect_vect.push_back(rect);
        }
        md.rect_clustering(rect_vect);
        rect_traj.emplace(time,rect_vect);
    }

    return true;
}

bool BabblingDataset::_load_hyperparameters(const YAML::Node& hyperparam){
    std::cout << "_load_camera_param" << std::endl;

    _camera_parameter = hyperparam["camera_parameters"];
    _supervoxel_parameter = hyperparam["sv"];
    _soi_parameter = hyperparam["soi"];
    YAML::Node workspace_parameter = hyperparam["workspace"];
    _workspace_parameter = workspace_t(true,
            workspace_parameter["sphere"]["x"].as<float>(),
            workspace_parameter["sphere"]["y"].as<float>(),
            workspace_parameter["sphere"]["z"].as<float>(),
            workspace_parameter["sphere"]["radius"].as<float>(),
            workspace_parameter["sphere"]["threshold"].as<float>(),
    {workspace_parameter["csg_intersect_cuboid"]["x_min"].as<float>(),
     workspace_parameter["csg_intersect_cuboid"]["x_max"].as<float>(),
     workspace_parameter["csg_intersect_cuboid"]["y_min"].as<float>(),
     workspace_parameter["csg_intersect_cuboid"]["y_max"].as<float>(),
     workspace_parameter["csg_intersect_cuboid"]["z_min"].as<float>(),
     workspace_parameter["csg_intersect_cuboid"]["z_max"].as<float>()});


    return true;
}

bool BabblingDataset::_load_rgbd_images(const std::string& foldername, const rect_trajectories_t& rects, rgbd_set_t &rgbd_set){
    std::cout << "_load_rgbd_images" << std::endl;

    YAML::Node rgb_node = _data_structure["rgb"];
    YAML::Node depth_node = _data_structure["depth"];
    std::map<double,cv::Mat> rgb_set;
    std::map<double,cv::Mat> depth_set;
    std::vector<std::string> split_string;

    //extract rgb images
    std::string f_name(rgb_node.as<std::string>());
    boost::split(split_string,f_name,boost::is_any_of("."));
    if(split_string.size() == 1){ //if contained in a folder
        std::string folder = foldername + "/rgb";
        if(!boost::filesystem::exists(folder)){
            std::cerr << "unable to find " << folder << std::endl;
            return false;
        }
        boost::filesystem::directory_iterator end_itr;

        for(boost::filesystem::directory_iterator itr(folder); itr != end_itr; ++itr){
            boost::split(split_string,itr->path().string(),boost::is_any_of("/"));
            boost::split(split_string,split_string.back(),boost::is_any_of("."));
            boost::split(split_string,split_string.front(),boost::is_any_of("_"));
            double time = std::stod(split_string[0]) + std::stod(split_string[1])*1e-9;
            if(rects.find(time) == rects.end())
                continue;

            cv::Mat image = cv::imread(itr->path().string(),CV_LOAD_IMAGE_COLOR);

            rgb_set.emplace(time,image);
        }
    }else if (split_string[1] == "yml"){//if contained in a file
        YAML::Node rgb_file = YAML::LoadFile(foldername + "/" + rgb_node.as<std::string>());
        for(YAML::iterator it = rgb_file.begin(); it != rgb_file.end(); it++){
            std::vector<uchar> vec_data = YAML::DecodeBase64(it->second["rgb"].as<std::string>());
            cv::Mat image = cv::imdecode(vec_data,cv::IMREAD_UNCHANGED);
            double time = it->second["timestamp"]["sec"].as<double>() +
                    it->second["timestamp"]["nsec"].as<double>()*1e-9;
            rgb_set.emplace(time,image);
        }
    }

    //Extract depth images
    f_name = depth_node.as<std::string>();
    boost::split(split_string,f_name,boost::is_any_of("."));
    if(split_string.size() == 1){//if contained in a folder
        std::string folder = foldername + "/depth";
        if(!boost::filesystem::exists(folder)){
            std::cerr << "unable to find " << folder << std::endl;
            return false;
        }
        boost::filesystem::directory_iterator end_itr;
        for(boost::filesystem::directory_iterator itr(folder); itr != end_itr; ++itr){
            std::vector<std::string> split_string;
            boost::split(split_string,itr->path().string(),boost::is_any_of("/"));
            boost::split(split_string,split_string.back(),boost::is_any_of("."));
            boost::split(split_string,split_string.front(),boost::is_any_of("_"));
            double time = std::stod(split_string[0]) + std::stod(split_string[1])*1e-9;

            if(rects.find(time) == rects.end())
                continue;

            cv::Mat depth_img = cv::imread(itr->path().string(),CV_LOAD_IMAGE_UNCHANGED | CV_LOAD_IMAGE_ANYDEPTH);

            depth_img = cv::Mat(depth_img.rows,depth_img.cols,CV_32FC1,depth_img.data).clone();

            depth_set.emplace(time,depth_img);
        }
    }else if(split_string[1] == "yml"){//if contained in a file
        YAML::Node depth_file = YAML::LoadFile(foldername + "/" + depth_node.as<std::string>());
        for(YAML::iterator it = depth_file.begin(); it != depth_file.end(); it++){
            std::vector<uchar> vec_data = YAML::DecodeBase64(it->second["depth"].as<std::string>());
            cv::Mat image = cv::imdecode(vec_data,cv::IMREAD_UNCHANGED);
            double time = it->second["timestamp"]["sec"].as<double>() +
                    it->second["timestamp"]["nsec"].as<double>()*1e-9;
            depth_set.emplace(time,image);
        }
    }

    //Merged both rgb_set and depth set in a rgbd_set
    for(auto itr = rgb_set.begin(); itr != rgb_set.end(); ++itr){
        cv::Mat rgb, depth;
        if(depth_set.find(itr->first) != depth_set.end())
            depth = depth_set[itr->first];

        rgb = itr->second;

        rgbd_set.emplace(itr->first,std::make_pair(rgb,depth));
    }

    for(auto itr = depth_set.begin(); itr != depth_set.end(); ++itr){
        cv::Mat rgb, depth;
        if(rgbd_set.find(itr->first) != rgbd_set.end())
            continue;

        depth = itr->second;

        rgbd_set.emplace(itr->first,std::make_pair(rgb,depth));
    }


    return true;
}

bool BabblingDataset::_load_arm_trajectories(const std::string &filename, arm_trajectories_t &arm_traj){
    std::cout << "_load_arm_trajectories" << std::endl;

    YAML::Node controller_feedback = YAML::LoadFile(filename);
    if(controller_feedback.IsNull())
        return false;

    for(YAML::iterator it = controller_feedback.begin(); it != controller_feedback.end(); ++it){
        std::vector<double> traj;
        for(int i = 0; i < it->second["joints_values"].size();i++)
            traj.push_back(it->second["joints_values"]["joint_"+std::to_string(i)].as<double>());
//        for(YAML::iterator it_traj = it->second["joints_values"].begin();
//            it_traj != it->second["joints_values"].end(); ++it_traj)
//            traj.push_back(it_traj->second.as<double>());

        double time = it->second["timestamp"]["sec"].as<double>() +
                it->second["timestamp"]["nsec"].as<double>()*1e-9;
        arm_traj.emplace(time,traj);
    }

    return true;
}

void BabblingDataset::rgbd_to_pointcloud(const cv::Mat& rgb, const cv::Mat& depth, PointCloudT::Ptr ptcl){
//    std::cout << "_rgbd_to_pointcloud" << std::endl;

    double center_x = _camera_parameter["depth"]["principal_point"]["x"].as<double>();
    double center_y = _camera_parameter["depth"]["principal_point"]["y"].as<double>();
    double focal_x = _camera_parameter["depth"]["focal_length"]["x"].as<double>();
    double focal_y = _camera_parameter["depth"]["focal_length"]["y"].as<double>();
    float bad_point = std::numeric_limits<float>::quiet_NaN();

    int rgb_cn = rgb.channels();

    ptcl->width = rgb.cols;
    ptcl->height = rgb.rows;

    for(int i = 0; i < rgb.rows; i++){
        uchar* rgb_rowPtr = reinterpret_cast<uchar*>(rgb.row(i).data);
        float* depth_rowPtr = reinterpret_cast<float*>(depth.row(i).data);


        for(int j = 0; j < rgb.cols; j++){
            PointT pt;
            float z = depth_rowPtr[j];
            if(z != z){
                pt.x = pt.y = pt.z = bad_point;
                continue;
            }else{
                pt.x = (i - center_x)*z/focal_x;
                pt.y = (j - center_y)*z/focal_y;
                pt.z = z;
            }


            pt.r = rgb_rowPtr[j*rgb_cn + 2];
            pt.g = rgb_rowPtr[j*rgb_cn + 1];
            pt.b = rgb_rowPtr[j*rgb_cn + 0];

            pt.a = 255;

            ptcl->push_back(pt);
        }
    }


//    pcl::PassThrough<PointT> passFilter;


//    passFilter.setInputCloud(ptcl);
//    passFilter.setFilterFieldName("z");
//    passFilter.setFilterLimits(_workspace_parameter.area[4],_workspace_parameter.area[5]);
//    passFilter.filter(*ptcl);
//    _workspace_parameter.filter(ptcl);

}

bool BabblingDataset::load_dataset(const std::string& meta_data_filename,const std::string& arch_name, int iteration){
    std::cout << "load_dataset" << std::endl;

    if(!boost::filesystem::exists(arch_name)){
        std::cerr << "unable to find " << arch_name << std::endl;
        return false;
    }

    if(_data_structure.size() == 0){
        if(!_load_data_structure(meta_data_filename)){
            std::cerr << "unable to load meta data file " << meta_data_filename << std::endl;
            return false;
        }
    }

    if(_iterations_folders.empty()){
        _load_iteration_folders(arch_name);
    }

    return load_dataset(iteration);

}

bool BabblingDataset::load_dataset(int iteration){
    std::cout << "load_dataset 2" << std::endl;

    rect_trajectories_t rects;
    rgbd_set_t images;
    arm_trajectories_t arm_traj;

    if(iteration > 0){

        if(!_load_data_iteration(_iterations_folders[iteration],images,rects,arm_traj))
            return false;
        _per_iter_rect_set.emplace(iteration,rects);
        _per_iter_rgbd_set.emplace(iteration,images);
        _per_iter_arm_traj.emplace(iteration,arm_traj);
    }else{
        for(auto itr = _iterations_folders.begin(); itr != _iterations_folders.end(); ++itr){
            if(!_load_data_iteration(itr->second,images,rects,arm_traj))
                return false;
            _per_iter_rect_set.emplace(itr->first,rects);
            _per_iter_rgbd_set.emplace(itr->first,images);
            _per_iter_arm_traj.emplace(iteration,arm_traj);
            rects.clear();
            images.clear();
            arm_traj.clear();
        }
    }

    return true;
}

std::pair<double,BabblingDataset::cloud_set_t>
BabblingDataset::extract_cloud(const rgbd_set_t::const_iterator &rgbd_iter,
                            const rect_trajectories_t::const_iterator &rect_iter){

    std::pair<double,cloud_set_t> res(rgbd_iter->first,cloud_set_t(rect_iter->second.size()));

    PointCloudT::Ptr cloud_tmp(new PointCloudT);
    for(size_t i = 0; i < rect_iter->second.size(); ++i){
        rgbd_to_pointcloud(cv::Mat(rgbd_iter->second.first,rect_iter->second[i]),
                cv::Mat(rgbd_iter->second.second,rect_iter->second[i]),
                cloud_tmp);
        res.second[i] = *cloud_tmp;
   }
   cloud_tmp.reset();

    return res;
}

void BabblingDataset::extract_cloud_trajectories(cloud_trajectories_set_t &cloud_traj){
    std::cout << "extract_cloud_trajectories" << std::endl;


    for(auto itr = _per_iter_rect_set.begin(); itr != _per_iter_rect_set.end();++itr){
        cloud_traj.emplace(itr->first,cloud_trajectories_t());
        for(auto rect_itr = itr->second.begin(); rect_itr != itr->second.end();++rect_itr)
            cloud_traj[itr->first].emplace(rect_itr->first,extract_cloud(
                                               _per_iter_rgbd_set[itr->first].find(rect_itr->first),rect_itr).second);
    }
}
