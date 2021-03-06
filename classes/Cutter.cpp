/*
 * Created by Ryan Jay 30.10.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>

#include "Cutter.hpp"
#include "util.hpp"

using namespace std;

//find the cuts using the winners and their values (returns the cut_list)
int Cutter::findTheCuts(int score_list_size, const vector<int> &winners, const vector<float> &vals,
                        const vector<int> &target_on, string target,
                        CutList *cut_list) {

    int cut_start = -1;
    int gap = 0;
    int win_sum = 0;
    float val_sum = 0.0;
    int total_size = 0;

    for (int i = 0; i < score_list_size; i++) {
        if (cut_start >= 0) {
            if (target_on[winners[i]]) {
                win_sum++;
                val_sum += vals[i];
            }

            if (!target_on[winners[i]] || vals[i] < threshold || i == score_list_size - 1) {
                if (i < score_list_size - 1) gap++;

                if (gap > max_gap || i == score_list_size - 1) {
                    if (cut_start < i - gap - min_cut) {
                        //output cut
                        int win_size = (i - gap) - cut_start + 1;
                        float score_avg = val_sum / (float) win_sum;
                        float coverage = (float) win_sum / (float) win_size;

                        if (coverage >= min_coverage) {
                            cout << PrettyTime(cut_start) << " - " << PrettyTime(i - gap)
                                 << ": size= " << PrettyTime(win_size) << " coverage= " << coverage
                                 << " score= " << score_avg << '\n';
                            total_size += win_size;
                            Cut cut;
                            cut.s = cut_start;
                            cut.e = i - gap;
                            cut.score = score_avg;
                            cut.coverage = coverage;
                            cut.label = target;
                            cut_list->push_back(cut);
                        }
                    }
                    cut_start = -1;
                    gap = 0;
                    val_sum = 0.0;
                    win_sum = 0;
                }
            } else {
                gap = 0;
            }

        } else if (target_on[winners[i]] && vals[i] >= threshold) {
            cut_start = i;
            gap = 0;
            val_sum = 0.0;
            win_sum = 0;
        }
    }

    return (total_size);

}


void Cutter::TagTargets() {
    //path stuff with movie file
    string tag_path;
    char sep = '/';
#ifdef _WIN32
    char  sep = '\\';
#endif
    string movie_base = getBaseName(getFileName(movie_file));
    string movie_directory = getDirectory(movie_file);
    string tag_movie = movie_base + ".tag";
    if (output_dir.empty()) {
        output_dir = movie_directory;
    }
    if (!tag_file.empty()) {
        tag_movie = tag_file;
        if (tag_file[0] == '/') {
            tag_path = tag_file;
        } else {
            tag_path = output_dir + sep + tag_movie;
        }
    } else {
        tag_path = output_dir + sep + tag_movie;
    }

    //find winners and their scores
    vector<int> winners(score_list.size());
    vector<float> vals(score_list.size());
    for (int i = 0; i < score_list.size(); i++) {
        winners[i] = scoreArgMax(score_list[i]);
        vals[i] = scoreMax(score_list[i]);
    }

    //open tag output file
    ofstream f(tag_path);
    if (!f) {
        cerr << "Cannot open file: " << tag_path << endl;
        exit(EXIT_FAILURE);
    }

    //write header
    f << getFileName(movie_file) << ",";
    for (const auto &label : labels)
        f << label << ",";
    f << endl;

    //find the predicted cuts for each target
    vector<int> target_time(static_cast<unsigned long>(total_targets), 0);
    CutList cut_list;
    for (int i = 0; i < total_targets; i++) {

        vector<int> target_on(static_cast<unsigned long>(total_targets), 0);
        target_on[i] = 1;

        cout << "Target [" << labels[i] << "]" << endl;

        target_time[i] = findTheCuts(static_cast<int>(score_list.size()), winners, vals, target_on, labels[i],
                                     &cut_list);


        cout << "Total cut length: " << PrettyTime(target_time[i]) << endl;
        cout << endl;

    }

    //write target total information
    f << score_list.size() << ",";
    for (int i = 0; i < total_targets; i++)
        f << target_time[i] << ",";
    f << endl;

    //sort the list based on cut start time
    sort(cut_list.begin(), cut_list.end(), [](const Cut &x, const Cut &y) { return (x.s < y.s); });


    //write cutlist to tag file
    f << "label,start,end,score,coverage" << endl;
    for (const auto &this_cut : cut_list) {
        f << this_cut.label << "," << this_cut.s << "," << this_cut.e
          << "," << this_cut.score << "," << this_cut.coverage << endl;
    }

    cout << "Writing tag data to: " << tag_path << endl;
    f.close();

}


void Cutter::CutMovie() {

    //path stuff with movie file
    char sep = '/';
#ifdef _WIN32
    char  sep = '\\';
#endif
    string movie_base = getBaseName(getFileName(movie_file));
    string movie_type = getFileExtension(movie_file);
    string cut_movie = movie_base + ".cut";
    string temp_base = temp_dir + sep + "cuts";
    string temp_path = temp_base + sep + cut_movie;
    string movie_directory = getDirectory(movie_file);

    //will come from input
    vector<int> target_on(static_cast<unsigned long>(total_targets), 0);
    for (int i : target_list)
        target_on[i] = 1;


    mkdir_recursive(temp_base.c_str());

    //init
    CutList cut_list;
    bool did_concat = true;


    //find winners and their scores
    vector<int> winners(score_list.size());
    vector<float> vals(score_list.size());

    for (int i = 0; i < score_list.size(); i++) {
        winners[i] = scoreArgMax(score_list[i]);
        vals[i] = scoreMax(score_list[i]);
    }

    int total = findTheCuts(static_cast<int>(score_list.size()), winners, vals, target_on, "", &cut_list);
    cout << "Total cut length: " << PrettyTime(total) << endl;
    //make the cuts
    if (!cut_list.empty()) {
        cout << "Making the cuts" << endl;
        string part_file_path = temp_dir + sep + "cuts.txt";
        ofstream part_file;
        part_file.open(part_file_path.c_str());
        if (!part_file.is_open()) {
            cout << "Cannot open file for writing: " << part_file_path << endl;
            exit(EXIT_FAILURE);
        }


        //use output_seek for wmv. fixed bug where cuts would freeze
        bool output_seek = false;
        if (movie_type == ".wmv" || movie_type == ".WMV" || movie_type == ".Wmv") {
            output_seek = true;
            movie_type = ".mkv";
        }


        //output a file for each cut in the list
        for (int i = 0; i < cut_list.size(); i++) {

            Cut this_cut = cut_list[i];

            string part_name = temp_path + '.' + to_string(i) + movie_type;
            cout << "   Creating piece: " << part_name << endl;

            string cut_command;
            if (output_seek)
                cut_command = "ffmpeg -loglevel 8 -y -i \"" + movie_file + "\" -ss " +
                              to_string(this_cut.s) + " -t " + to_string(this_cut.e - this_cut.s) +
                              " -c copy \"" + part_name + "\"";
            else
                cut_command = "ffmpeg -loglevel 8 -y -ss " + to_string(this_cut.s) +
                              " -i \"" + movie_file + "\" -t " + to_string(this_cut.e - this_cut.s) +
                              " -c copy -avoid_negative_ts 1 \"" + part_name + "\"";

            if (system(cut_command.c_str())) {
                cerr << "Error cutting piece : " << part_name << endl;
                exit(EXIT_FAILURE);
            }

            //write piece to cuts.txt as instructions for concatenation
            part_file << "file \'" << part_name << "\'" << endl;
        }
        part_file.close();

        //default behavior (output cut where input movie is located)
        if (output_dir.empty())
            output_dir = movie_directory;


        if (do_concat) {
            cout << "Concatenating parts in " << part_file_path << endl;
            cout << "Final output: " << output_dir << sep << cut_movie << movie_type << endl;

            string concat_command = "ffmpeg -loglevel 16 -f concat -safe 0 -i " + part_file_path +
                                    " -c copy \"" + output_dir + sep + cut_movie + movie_type + "\"";
            if (system(concat_command.c_str())) {
                cerr << "Didn't concatenate pieces from: " << part_file_path << endl;
                did_concat = false;
            }
        } else {
            //copy cut directory to output_dir instead of concatenating
            cout << "Final cut directory: " << output_dir << sep << cut_movie << endl;
            string copy_directory_cmd = "cp -r " + temp_dir + sep + "cuts/ \"" +
                                        output_dir + sep + cut_movie + "\"";
            if (system(copy_directory_cmd.c_str())) {
                cerr << "Can't copy cut directory to: "
                     << output_dir << sep << cut_movie << endl;
                //dont exit so we still clear cut directory
                did_concat = false;
            }
        }

    } else {
        cout << "No cuts found." << endl;
        return;
    }


    //ask about removing original and only keeping cut
    if (remove_original && did_concat && queryYesNo()) {
        if (remove(movie_file.c_str()) == -1) {
            cerr << "Error removing input movie: " << errno << endl;
            exit(EXIT_FAILURE);
        }
    }


    //clean up cuts directory and cuts.txt file
    if (cleanDir(temp_dir, "cuts.txt") && cleanDir(temp_base)) {
        cerr << "Error cleaning up temporary cut piece files" << endl;
        exit(EXIT_FAILURE);
    }
}

