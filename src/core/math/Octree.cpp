#include "Octree.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <time.h>
#include <sstream> //for std::stringstream
#include <string>  //for std::string

#include "core/utils/toString.h"
#include "core/objects/geometry/VRGeoData.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/material/VRMaterial.h"

OSG_BEGIN_NAMESPACE

OctreeNode::OctreeNode(OctreePtr tree, float res, float s, int lvl) : resolution(res), size(s), level(lvl) { this->tree = tree; }
OctreeNode::~OctreeNode() {
    for (auto c : children) if (c) delete c;
}

int OctreeNode::getOctant(Vec3d p) {
    Vec3d rp = p - center;

    int o = 0;
    if (rp[0] < 0) o+=1;
    if (rp[1] < 0) o+=2;
    if (rp[2] < 0) o+=4;
    return o;
}

Vec3d lvljumpCenter(float s2, Vec3d rp) {
    Vec3d c(s2,s2,s2);
    if (rp[0] < 0) c[0]-=s2*2;
    if (rp[1] < 0) c[1]-=s2*2;
    if (rp[2] < 0) c[2]-=s2*2;
    return c;
}

bool OctreeNode::inBox(Vec3d p, Vec3d c, float size) {
    if (abs(2*p[0] - 2*c[0]) > size) return false;
    if (abs(2*p[1] - 2*c[1]) > size) return false;
    if (abs(2*p[2] - 2*c[2]) > size) return false;
    return true;
}

OctreeNode* OctreeNode::get(Vec3d p, bool checkPosition) {
    if ( !inBox(p, center, size) && checkPosition ) {
        if (parent) return parent->get(p, true);
        else return 0;
    }

    if (size > resolution) {
        int o = getOctant(p);
        if (!children[o]) return this;
        return children[o]->get(p, false);
    }

    return this;
}

OctreeNode* OctreeNode::add(Vec3d p, void* d, int targetLevel, bool checkPosition) {
    Vec3d rp = p - center;
    int currentLevel = level;

    if ( !inBox(p, center, size) && checkPosition ) { // not in node
        //cout << " OctreeNode::add " << p << " not in node " << currentLevel << " " << targetLevel << endl;
        if (parent == 0) { // no parent, create it
            //cout << "  OctreeNode::add parent" << endl;
            parent = new OctreeNode(tree.lock(), resolution, 2*size, currentLevel+1);
            Vec3d c = center + lvljumpCenter(size*0.5, rp);
            parent->center = c;
            int o = parent->getOctant(center);
            parent->children[o] = this;
            tree.lock()->updateRoot();
        }
        return parent->add(p, d, targetLevel, true); // go a level up
    }

    if (size > resolution && (currentLevel != targetLevel || targetLevel == -1)) {
        //cout << " OctreeNode::add " << p << " below " << currentLevel << " " << targetLevel << endl;
        int o = getOctant(p);
        if (children[o] == 0) {
            children[o] = new OctreeNode(tree.lock(), resolution, size*0.5, currentLevel-1);
            Vec3d c = center + lvljumpCenter(size*0.25, rp);
            children[o]->center = c;
            children[o]->parent = this;
        }
        return children[o]->add(p, d, targetLevel, false);
    }

    //cout << "  OctreeNode::add " << p << " here! " << currentLevel << " " << targetLevel << endl;
    data.push_back(d);
    points.push_back(p);
    return this;
}

void OctreeNode::set(OctreeNode* node, Vec3d p, void* d) { node->data.clear(); node->points.clear(); node->data.push_back(d); node->points.push_back(p); }

vector<OctreeNode*> OctreeNode::getAncestry() {
    vector<OctreeNode*> res;
    auto p = parent;
    while (p) { res.push_back(p); p = p->parent; }
    return res;
}

vector<OctreeNode*> OctreeNode::getChildren() {
    return vector<OctreeNode*>(children, children+8);
}

vector<OctreeNode*> OctreeNode::getPathTo(Vec3d p) {
    vector<OctreeNode*> res;
    auto o = get(p);
    if (!o) return res;

    res.push_back(o);
    while (o->parent) {
        o = o->parent;
        res.push_back(o);
    }
    std::reverse(res.begin(), res.end());
    return res;
}

void gatherSubtree(OctreeNode* o, vector<OctreeNode*>& res) {
    for (auto c : o->getChildren()) {
        if (c) {
            res.push_back(c);
            gatherSubtree(c, res);
        }
    }
}

vector<OctreeNode*> OctreeNode::getSubtree() {
    vector<OctreeNode*> res;
    gatherSubtree(this, res);
    return res;
}

Vec3d OctreeNode::getCenter() { return center; }
Vec3d OctreeNode::getLocalCenter() {
    if (parent) return center - parent->center;
    else return center;
}

void OctreeNode::remData(void* d) {
    data.erase(std::remove(data.begin(), data.end(), d), data.end());
}

OctreeNode* OctreeNode::getParent() { return parent; }

float OctreeNode::getSize() { return size; }

// sphere center, box center, sphere radius, box size
bool sphere_box_intersect(Vec3d Ps, Vec3d Pb, float Rs, float Sb)  {
    float r2 = Rs * Rs;
    Vec3d diag(Sb*0.5, Sb*0.5, Sb*0.5);
    Vec3d Bmin = Pb - diag;
    Vec3d Bmax = Pb + diag;
    float dmin = 0;
    if( Ps[0] < Bmin[0] ) dmin += ( Ps[0] - Bmin[0] )*( Ps[0] - Bmin[0] );
    else if( Ps[0] > Bmax[0] ) dmin += ( Ps[0] - Bmax[0] )*( Ps[0] - Bmax[0] );
    if( Ps[1] < Bmin[1] ) dmin += ( Ps[1] - Bmin[1] )*( Ps[1] - Bmin[1] );
    else if( Ps[1] > Bmax[1] ) dmin += ( Ps[1] - Bmax[1] )*( Ps[1] - Bmax[1] );
    if( Ps[2] < Bmin[2] ) dmin += ( Ps[2] - Bmin[2] )*( Ps[2] - Bmin[2] );
    else if( Ps[2] > Bmax[2] ) dmin += ( Ps[2] - Bmax[2] )*( Ps[2] - Bmax[2] );
    return dmin <= r2;
}

void OctreeNode::findInSphere(Vec3d p, float r, int d, vector<void*>& res) { // TODO: optimize!!
    if (!sphere_box_intersect(p, center, r, size)) return;

    float r2 = r*r;
    for (unsigned int i=0; i<data.size(); i++) {
        if ((points[i]-p).squareLength() <= r2)
            res.push_back(data[i]);
    }

    if (level == d && d != -1) return;
    for (int i=0; i<8; i++) {
        if (children[i]) children[i]->findInSphere(p, r, d, res);
    }
}

// box min, box max, octree box center, octree box size
bool box_box_intersect(Vec3d min, Vec3d max, Vec3d Bpos, float Sb)  {
    Vec3d Bdiag(Sb, Sb, Sb);
    Vec3d Bmin = Bpos - Bdiag*0.5;
    Vec3d Bmax = Bpos + Bdiag*0.5;

    Vec3d Apos = (max + min)*0.5;
    Vec3d Adiag = max-min;

    Vec3d diff = (Apos-Bpos)*2;
    Vec3d ABdiag = Adiag+Bdiag;
    return (abs(diff[0]) <= ABdiag[0]) && (abs(diff[1]) <= ABdiag[1]) && (abs(diff[2]) <= ABdiag[2]);
}

void OctreeNode::findInBox(const Boundingbox& b, int d, vector<void*>& res) { // TODO: optimize!!
    if (!box_box_intersect(b.min(), b.max(), center, size)) return;

    for (unsigned int i=0; i<data.size(); i++) {
        if (b.isInside( points[i] )) res.push_back(data[i]);
    }

    if (level == d && d != -1) return;
    for (int i=0; i<8; i++) {
        if (children[i]) children[i]->findInBox(b, d, res);
    }
}

string OctreeNode::toString(int indent) {
    auto pToStr = [](void* p) {
        const void * address = static_cast<const void*>(p);
        std::stringstream ss;
        ss << address;
        return ss.str();
    };

    string res = "\nOc ";
    for (int i=0; i<indent; i++) res += " ";
    res += "size: " + ::toString(size) + " center: " + ::toString(center);
    if (data.size() > 0) res += "\n";
    for (unsigned int i=0; i<data.size(); i++) if(data[i]) res += " " + pToStr(data[i]);
    return res;
}

void OctreeNode::print(int indent) {
    cout << toString(indent) << flush;
    for (int i=0; i<8; i++) {
        if (children[i] != 0) children[i]->print(indent+1);
    }
}

vector<void*> OctreeNode::getData() { return data; }

vector<void*> OctreeNode::getAllData() {
    vector<void*> res;
    for (auto c : getSubtree()) {
        auto d = c->getData();
        res.insert(res.end(), d.begin(), d.end());
    }
    return res;
}


Octree::Octree(float res, float s, string n) : resolution(res), firstSize(s), name(n) { if (s < res) firstSize = res; }
Octree::~Octree() { if (root) delete root; }

OctreePtr Octree::create(float resolution, float size, string n) {
    auto o = OctreePtr( new Octree(resolution, size, n) );
    o->clear();
    return o;
}

OctreePtr Octree::ptr() { return shared_from_this(); }

float Octree::getSize() { return root->getSize(); }
void Octree::clear() { if (root) delete root; root = new OctreeNode(ptr(), resolution, firstSize, 0); }

OctreeNode* Octree::get(Vec3d p, bool checkPosition) { return root->get(p, checkPosition); }

OctreeNode* Octree::add(Vec3d p, void* data, int targetLevel, bool checkPosition) {
    return getRoot()->add(p, data, targetLevel, checkPosition);
}

void Octree::addBox(const Boundingbox& b, void* d, int targetLevel, bool checkPosition) {
    const Vec3d min = b.min();
    const Vec3d max = b.max();
    add(min, d, targetLevel, checkPosition);
    add(Vec3d(max[0],min[1],min[2]), d, targetLevel, checkPosition);
    add(Vec3d(max[0],min[1],max[2]), d, targetLevel, checkPosition);
    add(Vec3d(min[0],min[1],max[2]), d, targetLevel, checkPosition);
    add(max, d, targetLevel, checkPosition);
    add(Vec3d(max[0],max[1],min[2]), d, targetLevel, checkPosition);
    add(Vec3d(min[0],max[1],min[2]), d, targetLevel, checkPosition);
    add(Vec3d(min[0],max[1],max[2]), d, targetLevel, checkPosition);
}

OctreeNode* Octree::getRoot() { return root; }
void Octree::updateRoot() { while (auto p = root->getParent()) root = p; }

vector<void*> Octree::getAllData() { return getRoot()->getAllData(); }

vector<void*> Octree::radiusSearch(Vec3d p, float r, int d) {
    vector<void*> res;
    getRoot()->findInSphere(p, r, d, res);
    return res;
}

vector<void*> Octree::boxSearch(const Boundingbox& b, int d) {
    vector<void*> res;
    getRoot()->findInBox(b, d, res);
    return res;
}

void Octree::test() {
    int Nv = 100000;
    float sMax = 4;
    Vec3d p(1,2,3);
    float r = 0.1;
    resolution = 0.0001;

    clear();
    srand(time(0));

    vector<Vec3d> Vec3fs;
    vector<void*> data;
    for (int i=0; i<Nv; i++) { // create random Vec3fs
        float x = rand()*sMax/RAND_MAX;
        float y = rand()*sMax/RAND_MAX;
        float z = rand()*sMax/RAND_MAX;
        auto d = (void*)new int(i);
        auto p = Vec3d(x,y,z);
        Vec3fs.push_back(p);
        data.push_back(d);
        add(p,d);
    }

    //getRoot()->print();

    int t0,t1,t2;
    vector<void*> radSearchRes_brute;

    t0=clock();
    vector<void*> radSearchRes_tree = radiusSearch(p, r);
    t1=clock();
    for (int i=0; i<Nv; i++) { // radius search brute forced
        auto p2 = Vec3fs[i];
        if (p2.dist(p) <= r) radSearchRes_brute.push_back( data[i] );
    }
    t2=clock();

    cout << "\ntest took " << t1-t0 << " octree range search && " << t2-t1 << " brute force\n";

    // validate results

    if (radSearchRes_brute.size() != radSearchRes_tree.size()) {
        cout << "\nOctreeNode test failed: result vector has wrong length " << radSearchRes_brute.size() << " " << radSearchRes_tree.size() << " !";
        return;
    }

    std::sort(radSearchRes_tree.begin(), radSearchRes_tree.end());
    std::sort(radSearchRes_brute.begin(), radSearchRes_brute.end());

    for (unsigned int i=0; i<radSearchRes_brute.size(); i++) {
        if (radSearchRes_tree[i] != radSearchRes_brute[i]) {
            cout << "\nOctreeNode test failed: mismatching test data!" << radSearchRes_tree[i] << "  " << radSearchRes_brute[i];
            return;
        }
    }

    cout << "\nOctreeNode test passed with " << radSearchRes_tree.size() << " found Vec3fs!\n";
}

VRGeometryPtr Octree::getVisualization() {
    VRGeoData data;
    auto nodes = root->getSubtree();
    nodes.push_back(root);
    for (auto c : nodes) {
        Pnt3d p = c->getCenter();
        float s = c->getSize()*0.499;
        int ruf = data.pushVert(p + Vec3d( 1, 1, 1)*s);
        int luf = data.pushVert(p + Vec3d(-1, 1, 1)*s);
        int rub = data.pushVert(p + Vec3d( 1, 1,-1)*s);
        int lub = data.pushVert(p + Vec3d(-1, 1,-1)*s);
        int rdf = data.pushVert(p + Vec3d( 1,-1, 1)*s);
        int ldf = data.pushVert(p + Vec3d(-1,-1, 1)*s);
        int rdb = data.pushVert(p + Vec3d( 1,-1,-1)*s);
        int ldb = data.pushVert(p + Vec3d(-1,-1,-1)*s);
        data.pushQuad(ruf, luf, lub, rub); // side up
        data.pushQuad(rdf, ldf, ldb, rdb); // side down
        data.pushQuad(luf, lub, ldb, ldf); // side left
        data.pushQuad(ruf, rub, rdb, rdf); // side right
        data.pushQuad(rub, lub, ldb, rdb); // side behind
        data.pushQuad(ruf, luf, ldf, rdf); // side front
    }

    auto g = data.asGeometry("octree");
    auto m = VRMaterial::create("octree");
    m->setLit(false);
    m->setWireFrame(true);
    g->setMaterial(m);
    return g;
}


OSG_END_NAMESPACE
