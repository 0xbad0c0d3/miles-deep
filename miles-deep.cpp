/*
 * Created by Ryan Jay 30.10.16
 * Covered by the GPL. v3 (see included LICENSE)
 */
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
#include <cerrno>
#include <cstring>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <boost/thread.hpp>
#include <getopt.h>
#include <regex>

#include "classes/Classifier.h"

using namespace caffe;  // NOLINT(build/namespaces)
using namespace std;
using std::string;

static std::regex filter_movie_filename_re("([;<>|\"])");

int global_ffmpeg_done = -1;

inline bool FileExists(const std::string &name) {
    ifstream f(name.c_str());
    return f.good();
}

int main(int argc, char **argv) {
    int batch_size = 32;
    int MAX_IMG_IDX = 99999;
    int report_interval = 100;
    int sleep_time = 1;
    int min_cut = 4;
    int max_gap = 2;
    double min_score = 0.5;
    double min_coverage = 0.4;
    vector<string> target_list;
    target_list.push_back("blowjob_handjob");  //the default target
    string movie_file;
    string screenshot_directory = "/screenshots/";

    string model_dir = "model/";
    string model_weights = model_dir + "weights.caffemodel";
    string model_def = model_dir + "deploy.prototxt";
    string mean_file = model_dir + "mean.binaryproto";
    string label_file = model_dir + "labels.txt";
    string output_directory = "";
    string tag_file = "";
    string temp_directory = "/tmp";
    bool auto_tag = false;
    bool do_concat = true;
    bool remove_original = true;

    /* options descriptor */
    static struct option longopts[] = {
            {"autotag-file",   required_argument, nullptr, 'f'},
            {"temp-directory", required_argument, nullptr, 'd'},
            {nullptr, 0,                          nullptr, 0}
    };

    //parse command line flags
    int opt;
    bool set_all_but_other = false;
    while ((opt = getopt_long(argc, argv, "f:act:b:d:o:m:ng:s:hxp:w:u:l:v:", longopts, nullptr)) != -1) {
        switch (opt) {
            case 'a':
                auto_tag = true;
                break;
            case 'f':
                auto_tag = true;
                tag_file = optarg;
                break;
            case 'c':
                do_concat = false;
                break;
            case 't':
                target_list = Split(optarg, ',');
                break;
            case 'b':
                batch_size = atoi(optarg);
                break;
            case 'd':
                temp_directory = optarg;
                break;
            case 'o':
                output_directory = optarg;
                break;
            case 'u':
                min_cut = atoi(optarg);
                break;
            case 'g':
                max_gap = atoi(optarg);
                break;
            case 's':
                min_score = atof(optarg);
                break;
            case 'v':
                min_coverage = atof(optarg);
                break;
            case 'm':
                mean_file = optarg;
                break;
            case 'p':
                model_def = optarg;
                break;
            case 'w':
                model_weights = optarg;
                break;
            case 'l':
                label_file = optarg;
                break;
            case 'n':
                remove_original = false;
                break;
            case 'h':
                PrintHelp();
                exit(0);
            case 'x':
                set_all_but_other = true;
                break;
            default: /* '?' */
                PrintUsage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        cerr << "No input movie file." << endl;
        PrintUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
    if (temp_directory.c_str()[temp_directory.length() - 1] == '/') {
        temp_directory = temp_directory.substr(0, temp_directory.length() - 1);
    }
    screenshot_directory = temp_directory + screenshot_directory;
    movie_file = std::regex_replace(argv[optind], filter_movie_filename_re, "\\$1");


    //keep Caffe quiet
    FLAGS_minloglevel = 3;
    ::google::InitGoogleLogging(argv[0]);

    //create the classifier
    Classifier classifier(model_def, model_weights, mean_file, label_file);

    if (set_all_but_other)
        target_list = allExceptOther(classifier.labels_);

    //print targets
    if (auto_tag)
        cout << "Auto-tag mode" << endl;
    else {
        cout << "Targets: [";
        for (int i = 0; i < target_list.size(); i++) {
            cout << target_list[i];
            if (i < target_list.size() - 1)
                cout << ", ";
        }
        cout << "]" << endl;
    }

    global_ffmpeg_done = MAX_IMG_IDX;
    cleanDir(screenshot_directory);
    boost::thread first(CreateScreenShots, movie_file, screenshot_directory);
    //first.join();  //uncomment to make predictions wait for screenshots

    int epoch = 0;
    bool no_more = false;
    ScoreList score_list;

    //loop till all screenshots have been
    //extracted and classified
    //wait for screenshots from ffmpeg thread

    while (true) {
        vector<cv::Mat> imgs;
        //fill a batch with screenshots to classify
        for (int i = 0; i < batch_size; i++) {

            int idx = epoch * batch_size + i + 1;

            //print some progress updates
            if (idx % report_interval == 0) {
                if (global_ffmpeg_done < MAX_IMG_IDX)
                    cout << PrettyTime(idx) << "/" << PrettyTime(global_ffmpeg_done) << endl;
                else
                    cout << PrettyTime(idx) << endl;
            }
            char buf[64];
            sprintf(buf, IMAGE_FORMAT, idx);
            string the_image(buf);
            string the_image_path = screenshot_directory + the_image;

            //wait for screenshots from ffmpeg thread
            while (!FileExists(the_image_path)) {
                //if ffmpeg is done getting screenshots quit waiting
                if (idx >= global_ffmpeg_done) {
                    no_more = true;
                    break;
                }
                cout << " Waiting for: " + the_image_path << endl;
                sleep(static_cast<unsigned int>(sleep_time));
            }

            if (!no_more) {
                cv::Mat img = cv::imread(the_image_path, -1);
                CHECK(!img.empty()) << "Unable to decode image " << the_image_path;
                imgs.push_back(img);
            } else
                break;
        }

        //don't try to classify an empty batch
        if (imgs.empty())
            break;

        //perform classification
        ScoreList ordered_preds = classifier.Classify(imgs);
        for (const auto &ordered_pred : ordered_preds)
            score_list.push_back(ordered_pred);

        if (no_more)
            break;

        epoch += 1;
    }

    //Either create a file out the cuts for all targets
    //or make the cuts from the input list
    Cutter *cutter = new Cutter;
    cutter->movie_file = movie_file;
    cutter->tag_file = tag_file;
    cutter->output_dir = output_directory;
    cutter->labels = classifier.labels_;
    cutter->temp_dir = temp_directory;
    cutter->max_gap = max_gap;
    cutter->min_cut = min_cut;
    cutter->threshold = static_cast<float>(min_score);
    cutter->min_coverage = static_cast<float>(min_coverage);
    cutter->score_list = score_list;

    if (auto_tag) {
        cutter->TagTargets();
    } else {
        //make the cuts based on the predictions
        cutter->do_concat = do_concat;
        cutter->remove_original = remove_original;
        for (const auto &i : target_list) {
            int target_idx = IndexOf(i, classifier.labels_);
            cutter->target_list.push_back(target_idx);
        }
        cutter->CutMovie();
    }
    delete cutter;

    //clean up screenshots
    cleanDir(screenshot_directory);

}

