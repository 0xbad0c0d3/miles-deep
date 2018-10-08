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
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <boost/thread.hpp>
#include <getopt.h>

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#else
    #ifdef HAS_INOTIFY_H
    #include <sys/inotify.h>
    #include <sys/select.h>
    #define INOTIFY_EVENT_BUFFER sizeof(struct inotify_event) + NAME_MAX + 1
    #endif
#endif

#include "classes/Classifier.h"

using namespace caffe;  // NOLINT(build/namespaces)
using namespace std;
using std::string;

int global_ffmpeg_done = -1;

#ifndef HAS_INOTIFY_H

inline bool FileExists(const std::string &name) {
    ifstream f(name.c_str());
    return f.good();
}

#endif

int main(int argc, char **argv) {
    int batch_size = 32;
    int MAX_IMG_IDX = 99999;
    int report_interval = 100;
#ifdef APPLE

#endif
#ifndef HAS_INOTIFY_H
    int sleep_time = 1;
#else
    int inotify_h;
    struct inotify_event *event __attribute__ ((aligned(8)));
    fd_set event_waiter;
#endif
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
    movie_file = argv[optind];


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
    boost::thread first(CreateScreenShots, movie_file, screenshot_directory);
    //first.join();  //uncomment to make predictions wait for screenshots

    int epoch = 0;
    bool no_more = false;
    ScoreList score_list;

    //loop till all screenshots have been
    //extracted and classified
    //wait for screenshots from ffmpeg thread
#ifdef HAS_INOTIFY_H
    inotify_h = inotify_init();
    if(inotify_h == -1){
        cerr << "Error initilizing INOTIFY: " << errno << endl;
        exit(EXIT_FAILURE);
    }
    event = (inotify_event*)malloc(INOTIFY_EVENT_BUFFER);
#endif
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
#ifndef HAS_INOTIFY_H
            while (!FileExists(the_image_path)) {
                //if ffmpeg is done getting screenshots quit waiting
                if (idx >= global_ffmpeg_done) {
                    no_more = true;
                    break;
                }
                cout << " Waiting for: " + the_image_path << endl;
                sleep(static_cast<unsigned int>(sleep_time));
            }
#else
            int the_image_wd = inotify_add_watch(inotify_h, the_image_path.c_str(), IN_CLOSE|IN_ONESHOT);
            if (the_image_wd == -1){
                cerr << "Error adding watch for " << the_image_path << ": " << errno << endl;
                exit(EXIT_FAILURE);
            }
            cout << " Waiting for: " + the_image_path << endl;
            FD_ZERO(&event_waiter);
            FD_SET(inotify_h, &event_waiter);
            struct timeval timeout = {.tv_sec = 5, 0};
            int select_rv = select(1, &event_waiter, nullptr, nullptr, &timeout);
            CHECK(select_rv != -1) << "Waiting failed" << endl;
            if (FD_ISSET(inotify_h, &event_waiter)){
                read(inotify_h, event, INOTIFY_EVENT_BUFFER);
                cout << "Image " << event->name << " is ready" << endl;
            } else {
                cerr << "Error waiting for " << the_image_path << endl;
                exit(EXIT_FAILURE);
            }
            no_more = idx >= global_ffmpeg_done;
#endif

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
    cutter->min_coverage = (float)min_coverage;
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

