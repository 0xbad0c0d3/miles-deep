/*
 * Created by Ryan Jay 15.11.16
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
#include "sys/stat.h"
#include "version.h"
#include <string.h>

#include "util.hpp"

using namespace std;

extern int global_ffmpeg_done;

float scoreMax(vector<float> x) {
    return *max_element(x.begin(), x.end());
}

int scoreArgMax(vector<float> x) {
    return static_cast<int>(distance(x.begin(), max_element(x.begin(), x.end())));
}

string getFileName(const string &s) {
    char sep = '/';

#ifdef _WIN32
    sep = '\\';
#endif

    size_t i = s.rfind(sep, s.length());
    if (i != string::npos)
        return (s.substr(i + 1, s.length() - i));
    return (s);
}

string getFileExtension(const string &s) {
    char sep = '.';

    size_t i = s.rfind(sep, s.length());
    if (i != string::npos)
        return (s.substr(i, s.length() - i));
    return (s);
}

string getBaseName(const string &s) {
    char sep = '.';

    size_t i = s.rfind(sep, s.length());
    if (i != string::npos)
        return (s.substr(0, i));
    return (s);
}

bool queryYesNo() {
    cout << "Replace movie with cut? This will delete the movie. [y/N]?";
    string input;
    getline(cin, input);
    if (input == "YES" || input == "Yes" || input == "yes"
        || input == "Y" || input == "y")
        return true;
    else
        return false;
}

string PrettyTime(int seconds) {
    int s, h, m;
    string pTime = "";

    m = (seconds / 60);
    h = int(m / 60) % 60;
    m = int(m % 60);
    s = int(seconds % 60);

    if (h > 0)
        pTime += to_string(h) + "h";
    if (seconds >= 60)
        pTime += to_string(m) + "m";
    pTime += to_string(s) + "s";
    return (pTime);

}

std::string getDirectory(const std::string &path) {
    int found = path.find_last_of("/\\");
    if (found < 0)
        return (".");
    else
        return (path.substr(0, (unsigned long) found));
}

int IndexOf(string label, vector<string> labels) {
    for (int k = 0; k < labels.size(); k++)
        if (label == labels[k])
            return k;

    cerr << "Label not found in list: " << label << endl;
    exit(EXIT_FAILURE);
}

string FormatFileNumber(int file_no) {
    ostringstream out;
    out << std::internal << std::setfill('0') << std::setw(5) << file_no;
    return out.str();
}

int CountFiles(string directory) {
    DIR *dir;
    struct dirent *ent;
    int n = 0;
    if ((dir = opendir(directory.c_str())) != nullptr) {
        n = 0;
        while ((ent = readdir(dir)) != nullptr) n++;
        closedir(dir);
        return n - 2; //-2 for . and ..
    } else {
        cerr << "Could not open directory: " << directory << endl;
        exit(EXIT_FAILURE);
    }
}

bool is_dir(const char *dir) {
    struct stat d_stat = {};
    stat(dir, &d_stat);
    return (d_stat.st_mode & S_IFDIR) == S_IFDIR;
}

bool mkdir_recursive(const char *dir) {
    char tmp[256];
    char *p = nullptr;
    size_t len;
    int rv = -1;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (is_dir(tmp)) {
                rv = 0;
            } else {
                rv = mkdir(tmp, S_IRWXU);
            }
            if (rv == -1) {
                cerr << "Could not create directory: " << tmp << endl;
                break;
            }
            *p = '/';
        }
    }
    return rv == 0 && mkdir(tmp, S_IRWXU) == 0;
}

void CreateScreenShots(string movie_file, string screenshot_directory) {

    if (is_dir(screenshot_directory.c_str())) {
        cleanDir(screenshot_directory);
    }
    if (!mkdir_recursive(screenshot_directory.c_str())) {
        cerr << "Could not create screenshots directory: " << screenshot_directory << endl;
        exit(EXIT_FAILURE);
    }

    string screenshot_cmd = "ffmpeg -loglevel 8 -i \"" + movie_file + "\" -vf fps=1 -q:v 1 " +
                            screenshot_directory + IMAGE_FORMAT;
    if (system(screenshot_cmd.c_str())) {
        cerr << "Error getting screenshots from: " << movie_file << endl;
        exit(EXIT_FAILURE);
    }

    global_ffmpeg_done = CountFiles(screenshot_directory);

}

void PrintUsage(char *prog_name) {
    cout << "REV: " << MILES_DEEP_REVISION << endl;
    cout << "Usage: " << prog_name << " [-t target|-x|-a|-f <file>] [-b batch_size] [-o output_dir] [options] movie_file"
         << endl;
    cout << "-h\tPrint more help information about options" << endl;
}

void PrintHelp() {
    cout << "REV: " << MILES_DEEP_REVISION << endl;
    cout << endl;
    cout << "Main Options" << endl;
    cout << "-t\tComma separated list of the Targets to search for (default:blowjob_handjob)" << endl;
    cout << "-x\tRemove all non-sexual scenes. Same as all targets except \'other\'. Ignores -t." << endl;
    cout << "-a\tCreate a tag file with the cuts for all categories. Ignores -t and -x" << endl;
    cout << "-f\tThe same as -a but will write result into the option argument file " << endl;
    cout << "-b\tBatch size (default: 32) - decrease if you run out of memory" << endl;
    cout << "-o\tOutput directory (default: same as input)" << endl;
    cout << "-d\tTemporary Directory (default: /tmp)" << endl;
    cout << "--\tTemporary Directory (default: /tmp)" << endl;
    cout << endl;
    cout << "Cutting Options" << endl;
    cout << "-u\tMinimum cUt in seconds (default: 4)" << endl;
    cout << "-g\tMax Gap (default: 2)- the largest section of non-target frames in a cut" << endl;
    cout << "-s\tMinimum Score (default: 0.5) - minimum value considered a match [0-1]" << endl;
    cout << "-v\tMinimum coVerage of target frames in a cut (default: 0.4) [0-1]" << endl;
    cout << "-c\tDon't Concatenate. Output cut directory (default: off)" << endl;
    cout << "-n\tDoN't ask to remove original movie file (default: off)" << endl;
    cout << endl;
    cout << "Model Options" << endl;
    cout << "-m\tMean file .binaryproto" << endl;
    cout << "-p\tDefinition of model .prototxt" << endl;
    cout << "-w\tWeights for model .caffemodel" << endl;
    cout << "-l\tLabel file" << endl;
}

vector<string> Split(const string &s, char delim) {
    stringstream ss(s);
    string item;
    vector<string> tokens;
    while (getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

vector<string> allExceptOther(vector<string> labels) {
    vector<string> output;
    for (const auto &label : labels) {
        if (label != "other") {
            output.push_back(label);
        }
    }
    return (output);
}

bool cleanDir(string path, string mask) {
    bool result = false;
    DIR *dir;
    dirent *ent;
    if ((dir = opendir(path.c_str())) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            if (strcmp(ent->d_name, "..") != 0 && strcmp(ent->d_name, ".") != 0) {
                char *filePath = (char *) malloc(path.length() + strlen(ent->d_name) + 2);
                sprintf(filePath, "%s/%s", path.c_str(), ent->d_name);
                if (ent->d_type == DT_DIR) {
                    cleanDir(filePath, mask);
                } else {
                    remove(filePath);
                }
                free(filePath);
            }
        }
        closedir(dir);
        result = remove(path.c_str()) == 0;
    } else {
        cerr << "Could not open directory: " << path << endl;
        exit(EXIT_FAILURE);
    }
    return result;
}