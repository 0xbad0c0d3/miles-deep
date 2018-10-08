/*
 * Created by Ryan Jay 30.10.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#ifndef CUT_MOVIE_HPP
#define CUT_MOVIE_HPP

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

using namespace std;

typedef struct {
    int s;
    int e;
    float score;
    float coverage;
    string label;
} Cut;

typedef vector<vector<float> > ScoreList;
typedef vector<Cut> CutList;


class Cutter {

public:
    string movie_file;
    vector<int> target_list;
    string output_dir = "";
    string temp_dir = "/tmp";
    int total_targets = 6;
    int min_cut = 5;
    int max_gap = 2;
    float threshold = 0.5;
    float min_coverage = 0.4;
    bool do_concat = true;
    bool remove_original = true;
    ScoreList score_list;
    vector<string> labels;
    string tag_file;

public:
    void CutMovie();

    void TagTargets();

protected:
    int findTheCuts(int score_list_size, const vector<int> &winners, const vector<float> &vals,
                    const vector<int> &target_on, string target,
                    CutList *cut_list);
};

#endif
