#ifndef VRSYSTEM_H_INCLUDED
#define VRSYSTEM_H_INCLUDED

#include <string>
#include <vector>

using namespace std;

string getSystemVariable(string name);

void printBacktrace();

bool exists(string path);
bool isFile(string path);
bool isFolder(string path);
bool isSamePath(string path1, string path2);
bool makedir(string path);
bool removeFile(string path);
string canonical(string path);
string absolute(string path);
string getFileName(string path, bool withExtension = true);
string getFileExtension(string path);
string getFolderName(string path);
string readFileContent(string fileName, bool binary = true);

vector<string> openFolder(string folder);

string systemCall(string cmd);
bool compileCodeblocksProject(string path);

void fileReplaceStrings(string filePath, string oldString, string newString);

void initTime();
long long getTime();
long long getCPUTime();
void getMemUsage(double& vm_usage, double& resident_set);
void doFrameSleep(double tFrame, double fps);

void startMemoryDog();

#endif // VRSYSTEM_H_INCLUDED
