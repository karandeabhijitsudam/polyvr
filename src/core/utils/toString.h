#ifndef TOSTRING_H_INCLUDED
#define TOSTRING_H_INCLUDED

#include <string>
#include <vector>
#include <OpenSG/OSGVector.h>
#include "core/math/VRMathFwd.h"

using namespace std;

vector<string> splitString(string s, char c = ' ');

string toString(const string& s);
string toString(const bool& b);
string toString(const int& i);
string toString(const size_t& i);
string toString(const unsigned int& i);
string toString(const double& f, int d = -1);
string toString(const float& f, int d = -1);
string toString(const OSG::Vec2f& v);
string toString(const OSG::Vec3f& v);
string toString(const OSG::Pnt3f& v);
string toString(const OSG::Vec4f& v);
string toString(const OSG::Vec2i& v);
string toString(const OSG::Vec3i& v);
string toString(const OSG::Vec4i& v);
string toString(const OSG::pose& p);
string toString(const OSG::posePtr& p);
string toString(const OSG::boundingbox& b);

// deprecated?
bool toBool(string s, int* N = 0);
int toInt(string s, int* N = 0);
unsigned int toUInt(string s, int* N = 0);
float toFloat(string s, int* N = 0);
double toDouble(string s, int* N = 0);
OSG::Vec2f toVec2f(string s);
OSG::Vec3f toVec3f(string s);
OSG::Vec4f toVec4f(string s);
OSG::Vec2i toVec2i(string s);
OSG::Vec3i toVec3i(string s);
OSG::Vec4i toVec4i(string s);
OSG::Pnt3f toPnt3f(string s);

template<typename T> void toValue(stringstream& s, T& t);

template<typename T> void toValue(string s, T& t){
    stringstream ss(s);
    toValue(ss,t);
}

/*void toValue(string s, string& s2);
stringstream toValue(string s, bool& b);
stringstream toValue(string s, int& i);
stringstream toValue(string s, float& f);
stringstream toValue(string s, OSG::Vec2f& v);
stringstream toValue(string s, OSG::Vec3f& v);
stringstream toValue(string s, OSG::Vec4f& v);
stringstream toValue(string s, OSG::Vec3i& v);
stringstream toValue(string s, OSG::posePtr& p);
stringstream toValue(string s, OSG::boundingbox& b);

void toValue(stringstream& ss, bool& b);
void toValue(stringstream& ss, int& i);
void toValue(stringstream& ss, float& f);
void toValue(stringstream& ss, OSG::Vec2f& v);
void toValue(stringstream& ss, OSG::Vec3f& v);
void toValue(stringstream& ss, OSG::Vec4f& v);
void toValue(stringstream& ss, OSG::Vec3i& v);
void toValue(stringstream& ss, OSG::posePtr& p);
void toValue(stringstream& ss, OSG::boundingbox& b);*/

#endif // TOSTRING_H_INCLUDED
