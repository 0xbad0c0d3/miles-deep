/*
 * Created by Ryan Jay 15.11.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <dirent.h>
#include <iomanip>
#include <sstream>

using namespace std;

#define IMAGE_FORMAT "img_%05d.jpg"


float scoreMax(vector<float> x);
int scoreArgMax(vector<float> x);
string getFileName(const string& s);
string getFileExtension(const string& s);
string getBaseName(const string& s);
bool queryYesNo();
string PrettyTime(int seconds);
string getDirectory(const string& path);

int IndexOf(string label, vector<string> labels);

string FormatFileNumber(int file_no);

int CountFiles(string directory);

void CreateScreenShots(string movie_file, string screenshot_directory);

void PrintUsage(char *prog_name);

void PrintHelp();

string PrettyTime(int seconds);

vector<string> Split(const string &s, char delim);

vector<string> allExceptOther(vector<string> labels);

bool cleanDir(string path, string mask = "*");

bool mkdir_recursive(const char *dir);

#endif
