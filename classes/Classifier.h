//
// Created by Daniil Skomarovskij on 05/10/2018.
//

#ifndef MILES_DEEP_CLASSIFIER_H
#define MILES_DEEP_CLASSIFIER_H

#include <caffe/caffe.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <stdlib.h>
#include <unistd.h>

#include <fstream>
#include <boost/thread.hpp>
#include "Cutter.hpp"
#include "util.hpp"

using namespace caffe;  // NOLINT(build/namespaces)
using namespace std;
using std::string;


class Classifier {
public:
    Classifier(const string &model_file,
               const string &trained_file,
               const string &mean_file,
               const string &label_file);

    ScoreList Classify(const vector <cv::Mat> &imgs);

    std::vector <string> labels_;

private:
    void SetMean(const string &mean_file);

    std::vector <vector<float>> Predict(const vector <cv::Mat> &imgs);

    void WrapInputLayer(std::vector <cv::Mat> *input_channels, int n);

    void Preprocess(const cv::Mat &img,
                    std::vector <cv::Mat> *input_channels);

private:
    boost::shared_ptr <Net<float>> net_;
    cv::Size input_geometry_;
    int num_channels_;
    cv::Mat mean_;
};


#endif //MILES_DEEP_CLASSIFIER_H
