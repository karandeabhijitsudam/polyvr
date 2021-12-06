#include "VRCOLLADA.h"
#include "VRCOLLADA_Material.h"
#include "VRCOLLADA_Geometry.h"
#include "VRCOLLADA_Kinematics.h"

#include "core/scene/import/VRImportFwd.h"
#include "core/utils/system/VRSystem.h"
#include "core/utils/toString.h"
#include "core/utils/xml.h"
#include "core/utils/VRScheduler.h"
#include "core/math/pose.h"

#include "core/objects/object/VRObject.h"
#include "core/objects/VRTransform.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/geometry/VRGeoData.h"

#include <OpenSG/OSGColor.h>

#include <iostream>
#include <stack>

using namespace OSG;

namespace OSG {

class VRCOLLADA_Scene {
    private:
        VRObjectPtr root;
        map<string, VRObjectPtr> objects;
        map<string, VRObjectPtr> library_nodes;
        map<string, VRObjectPtr> library_scenes;
        string currentSection;

        VRSchedulerPtr scheduler;
        stack<VRObjectPtr> objStack;
        VRGeometryPtr lastInstantiatedGeo;

    public:
        VRCOLLADA_Scene() { scheduler = VRScheduler::create(); }

        void setRoot(VRObjectPtr r) { root = r; }
        void closeNode() { objStack.pop(); }
        VRObjectPtr top() { return objStack.size() > 0 ? objStack.top() : 0; }

        bool hasScene(string name) { return library_scenes.count(name); }

        void setCurrentSection(string s) {
            currentSection = s;
            objStack = stack<VRObjectPtr>();
            lastInstantiatedGeo = 0;
        }

		void finalize() { scheduler->callPostponed(true); }

		void applyMatrix(Matrix4d m) {
            auto t = dynamic_pointer_cast<VRTransform>(top());
            if (t) {
                auto M = t->getMatrix();
                M.mult(m);
                t->setMatrix(M);
            }
		}

        void setMatrix(string data) {
            Matrix4d m = toValue<Matrix4d>(data);
            m.transpose();
            applyMatrix(m);
        }

        void translate(string data) { // TODO: read in specs what these are all about, like this it messes up the AML test scene
            Vec3d v = toValue<Vec3d>(data);
            Matrix4d m;
            m.setTranslate(v);
            applyMatrix(m);
        }

        void rotate(string data) { // TODO: read in specs what these are all about, like this it messes up the AML test scene
            Vec4d v = toValue<Vec4d>(data);
            double a = v[3]/180.0*Pi;
            Quaterniond q(Vec3d(v[0], v[1], v[2]), a);

            Matrix4d m;
            m.setRotate(q);
            applyMatrix(m);
        }

        void setMaterial(string mid, VRCOLLADA_Material* materials, VRGeometryPtr geo = 0) {
            if (!geo) geo = lastInstantiatedGeo;
            auto mat = materials->getMaterial(mid);
            if (!mat) {
                if (geo) geo->addTag("COLLADA-postponed");
                scheduler->postpone( bind(&VRCOLLADA_Scene::setMaterial, this, mid, materials, geo) );
                return;
            }
            if (geo) geo->remTag("COLLADA-postponed");
            if (geo) geo->setMaterial(mat);
        }

        void newNode(string id, string name) {
            if (name == "") name = id;
            auto obj = VRTransform::create(name);

            auto parent = top();
            if (parent) parent->addChild(obj);
            else {
                if (currentSection == "library_nodes") library_nodes[id] = obj;
                if (currentSection == "library_visual_scenes") library_scenes[id] = obj;
            }

            objStack.push(obj);
        }

        void instantiateGeometry(string gid, VRCOLLADA_Geometry* geometries, VRObjectPtr parent = 0) {
            if (!parent) parent = top();
            auto geo = geometries->getGeometry(gid);
            if (!geo) {
                if (parent) parent->addTag("COLLADA-postponed");
                scheduler->postpone( bind(&VRCOLLADA_Scene::instantiateGeometry, this, gid, geometries, parent) );
                return;
            }
            lastInstantiatedGeo = dynamic_pointer_cast<VRGeometry>( geo->duplicate() );
            if (parent) parent->remTag("COLLADA-postponed");
            if (parent) parent->addChild( lastInstantiatedGeo );
        }

        void instantiateNode(string url, string fPath, VRObjectPtr parent = 0) {
            if (!parent) parent = top();

            if (!library_nodes.count(url)) {
                string file = splitString(url, '#')[0];
                string node = splitString(url, '#')[1];

                if (file != "") { // reference another file
                    // TODO, use node to get correct object
                    VRTransformPtr obj = VRTransform::create(file);
                    map<string, string> options;
                    loadCollada( fPath + "/" + file, obj, options );
                    library_nodes[url] = obj;
                } else {} // should already be in library_nodes
            }

            if (parent && library_nodes.count(url)) {
                auto prototype = library_nodes[url];
                if (prototype->hasTag("COLLADA-postponed")) {
                    scheduler->postpone( bind(&VRCOLLADA_Scene::instantiateNode, this, url, fPath, parent) );
                } else parent->addChild( prototype->duplicate() );
            }
        }

        void instantiateScene(string url) {
            if (library_scenes.count(url)) root->addChild(library_scenes[url]);
        }
};

class VRCOLLADA_Stream : public XMLStreamHandler {
    private:
        struct Node {
            string name;
            string data;
            map<string, XMLAttribute> attributes;
        };

        string fPath;
        map<string, string> options;
        stack<Node> nodeStack;

        string currentSection;
        VRCOLLADA_Material materials;
        VRCOLLADA_Geometry geometries;
        VRCOLLADA_Scene scene;
        //VRCOLLADA_Kinematics kinematics;

        string skipHash(const string& s) { return (s[0] == '#') ? subString(s, 1, s.size()-1) : s; }

    public:
        VRCOLLADA_Stream(VRObjectPtr root, string fPath, map<string, string> opts) : fPath(fPath), options(opts) {
            scene.setRoot(root);
            materials.setFilePath(fPath);
        }

        ~VRCOLLADA_Stream() {}

        static VRCOLLADA_StreamPtr create(VRObjectPtr root, string fPath, map<string, string> opts) { return VRCOLLADA_StreamPtr( new VRCOLLADA_Stream(root, fPath, opts) ); }

        void startDocument() override {}

        void endDocument() override {
            materials.finalize();
            geometries.finalize();
            scene.finalize();
        }

        void startElement(const string& uri, const string& name, const string& qname, const map<string, XMLAttribute>& attributes) override;
        void endElement(const string& uri, const string& name, const string& qname) override;
        void characters(const string& chars) override;
        void processingInstruction(const string& target, const string& data) override {}

        void warning(const string& chars) override { cout << "VRCOLLADA_Stream Warning" << endl; }
        void error(const string& chars) override { cout << "VRCOLLADA_Stream Error" << endl; }
        void fatalError(const string& chars) override { cout << "VRCOLLADA_Stream Fatal Error" << endl; }
        void onException(exception& e) override { cout << "VRCOLLADA_Stream Exception" << endl; }
};

}

void VRCOLLADA_Stream::startElement(const string& uri, const string& name, const string& qname, const map<string, XMLAttribute>& attributes) {
    //cout << "startElement " << name << " " << nodeStack.size() << endl;
    Node parent;
    if (nodeStack.size() > 0) parent = nodeStack.top();

    Node n;
    n.name = name;
    n.attributes = attributes;
    nodeStack.push(n);

    // enter section
    if (name == "asset") currentSection = name;
    if (name == "library_geometries") currentSection = name;
    if (name == "library_materials" || name == "library_effects" || name == "library_images") currentSection = name;
    if (name == "library_joints" || name == "library_kinematics_models" || name == "library_articulated_systems" || name == "library_kinematics_scenes") currentSection = name;
    if (name == "scene" || name == "library_visual_scenes" || name == "library_nodes") {
        currentSection = name;
        scene.setCurrentSection(name);
    }

    // materials and textures
    if (name == "surface") materials.addSurface(parent.attributes["sid"].val);
    if (name == "sampler2D") materials.addSampler(parent.attributes["sid"].val);
    if (name == "effect") materials.newEffect( n.attributes["id"].val );
    if (name == "material") materials.newMaterial( n.attributes["id"].val, n.attributes["name"].val );
    if (name == "instance_effect") materials.setMaterialEffect( skipHash(n.attributes["url"].val) );

    // geometric data
    if (name == "geometry") geometries.newGeometry(n.attributes["name"].val, n.attributes["id"].val);
    if (name == "source") geometries.newSource(n.attributes["id"].val);
    if (name == "accessor") geometries.handleAccessor(n.attributes["count"].val, n.attributes["stride"].val);
    if (name == "input") geometries.handleInput(n.attributes["semantic"].val, skipHash(n.attributes["source"].val), n.attributes["offset"].val, n.attributes["set"].val);
    if (name == "triangles") geometries.newPrimitive(name, n.attributes["count"].val);
    if (name == "trifans") geometries.newPrimitive(name, n.attributes["count"].val);
    if (name == "tristrips") geometries.newPrimitive(name, n.attributes["count"].val);
    if (name == "polylist") geometries.newPrimitive(name, n.attributes["count"].val);

    // scene graphs
    if (name == "instance_geometry") scene.instantiateGeometry(skipHash(n.attributes["url"].val), &geometries);
    if (name == "instance_material") scene.setMaterial(skipHash(n.attributes["target"].val), &materials);
    if (name == "instance_node") scene.instantiateNode(skipHash(n.attributes["url"].val), fPath);
    if (name == "node" || name == "visual_scene") scene.newNode(n.attributes["id"].val, n.attributes["name"].val);

    // actual scene
    if (name == "instance_visual_scene") {
        string sceneName = skipHash(n.attributes["url"].val);
        if (options.count("scene"))
            if (scene.hasScene(options["scene"]))
                sceneName = options["scene"];
        scene.instantiateScene(sceneName);
    }
}

void VRCOLLADA_Stream::characters(const string& chars) {
    nodeStack.top().data += chars;
}

void VRCOLLADA_Stream::endElement(const string& uri, const string& name, const string& qname) {
    auto node = nodeStack.top();
    nodeStack.pop();
    Node parent;
    if (nodeStack.size() > 0) parent = nodeStack.top();

    if (node.name == "init_from" && parent.name == "surface") materials.setSurfaceSource(node.data);
    if (node.name == "source" && parent.name == "sampler2D") materials.setSamplerSource(node.data);

    if (node.name == "effect") materials.closeEffect();
    if (node.name == "geometry") geometries.closeGeometry();
    if (node.name == "init_from" && parent.name == "image") materials.loadImage(parent.attributes["id"].val, node.data);
    if (node.name == "color") {
        string type;
        if (node.attributes.count("sid")) type = node.attributes["sid"].val;
        else type = parent.name;
        materials.setColor(type, toValue<Color4f>(node.data));
    }
    if (node.name == "texture") materials.setTexture(node.attributes["texture"].val);
    if (node.name == "float_array") geometries.setSourceData(node.data);
    if (node.name == "p") geometries.handleIndices(node.data);
    if (node.name == "triangles") geometries.closePrimitive();
    if (node.name == "trifans") geometries.closePrimitive();
    if (node.name == "tristrips") geometries.closePrimitive();
    if (node.name == "polylist") geometries.closePrimitive();
    if (node.name == "vcount") geometries.handleVCount(node.data);

    if (node.name == "float") {
        float f = toValue<float>(node.data);
        string sid = node.attributes["sid"].val;
        if (sid == "shininess") materials.setShininess(f);
    }

    if (node.name == "node" || node.name == "visual_scene") scene.closeNode();
    if (node.name == "matrix") scene.setMatrix(node.data);
    if (node.name == "translate") scene.translate(node.data);
    if (node.name == "rotate") scene.rotate(node.data);
}

void OSG::loadCollada(string path, VRObjectPtr root, map<string, string> options) {
    auto xml = XML::create();
    string fPath = getFolderName(path);
    auto handler = VRCOLLADA_Stream::create(root, fPath, options);
    xml->stream(path, handler.get());
}

string create_timestamp() {
    time_t _tm = time(0);
    struct tm * curtime = localtime ( &_tm );
    ostringstream ss;
    ss << put_time(curtime, "%Y-%m-%dT%H:%M:%S"); // 2021-12-06T13:51:03
    return string(ss.str());
}

void OSG::writeCollada(VRObjectPtr root, string path, map<string, string> options) {
    string version = "1.5.0"; // "1.4.1"
    if (options.count("version")) version = options["version"];
    ofstream stream(path);

    function<void(VRObjectPtr, int)> writeSceneGraph = [&](VRObjectPtr node, int indent) -> void {
        string identStr = "";
        for (int i=0; i< indent; i++) identStr += "\t";
        string name = node->getName();
        VRTransformPtr trans = dynamic_pointer_cast<VRTransform>(node);
        VRGeometryPtr geo = dynamic_pointer_cast<VRGeometry>(node);
        VRMaterialPtr mat = geo->getMaterial();
        string matName = mat->getName();

        stream << identStr << "<node id=\"" << name << "_visual_scene_node\" name=\"" << name << "\">" << endl;

        if (trans) {
            Matrix4d m = trans->getMatrix();
            stream << identStr << "\t<matrix>";
            stream << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << " ";
            stream << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << " ";
            stream << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << " ";
            stream << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3];
            stream << "</matrix>" << endl;
        }

        if (geo) {
            stream << identStr << "\t<instance_geometry url=\"#" << name << "\">" << endl;
            stream << identStr << "\t\t<bind_material>" << endl;
            stream << identStr << "\t\t\t<technique_common>" << endl;
            stream << identStr << "\t\t\t\t<instance_material symbol=\"" << matName << "\" target=\"#" << matName << "\"/>" << endl;
            stream << identStr << "\t\t\t</technique_common>" << endl;
            stream << identStr << "\t\t</bind_material>" << endl;
            stream << identStr << "\t</instance_geometry>" << endl;
        }

		if (node->getChildrenCount() > 0) {
            for (auto child : node->getChildren()) {
                writeSceneGraph(child, indent+1);
            }
		}
		stream << identStr << "</node>" << endl;
    };

    stream << "﻿<?xml version=\"1.0\" encoding=\"utf-8\"?>" << endl;
    if (version == "1.4.1") stream << "<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"" << version << "\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">" << endl;
    else                    stream << "﻿<COLLADA xmlns=\"http://www.collada.org/2008/03/COLLADASchema\" version=\"" << version << "\">" << endl;

    string timestamp = create_timestamp();
    stream << "﻿\t<asset>" << endl;
    stream << "﻿\t\t<contributor>" << endl;
    stream << "﻿\t\t\t<author>PolyVR User</author>" << endl;
	stream << "﻿\t\t\t<author_website>https://github.com/Victor-Haefner/polyvr</author_website>" << endl;
    stream << "﻿\t\t\t<authoring_tool>PolyVR</authoring_tool>" << endl;
    stream << "﻿\t\t</contributor>" << endl;
    stream << "﻿\t\t<created>" << timestamp << "</created>" << endl;
    stream << "﻿\t\t<modified>" << timestamp << "</modified>" << endl;
    stream << "﻿\t\t<unit name=\"meter\" meter=\"1\"/>" << endl;
    stream << "﻿\t\t<up_axis>Y_UP</up_axis>" << endl;
    stream << "﻿\t</asset>" << endl;

    auto geos = root->getChildren(true, "Geometry", true);
    map<string, VRMaterialPtr> materials;
    for (auto obj : geos) {
        VRGeometryPtr geo = dynamic_pointer_cast<VRGeometry>(obj);
        auto mat = geo->getMaterial();
        materials[mat->getName()] = mat;
    }

    stream << "﻿\t<library_effects>" << endl;
    for (auto mat : materials) {
        string name = mat.second->getName();
        Color3f d = mat.second->getDiffuse();
        float t = mat.second->getTransparency();

        stream << "﻿\t\t<effect id=\"" << name << "_effect\">" << endl;
        stream << "﻿\t\t\t<profile_COMMON>" << endl;
        stream << "﻿\t\t\t\t<technique sid=\"common\">" << endl;
        stream << "﻿\t\t\t\t\t<lambert>" << endl;
        stream << "﻿\t\t\t\t\t\t<emission>" << endl;
        stream << "﻿\t\t\t\t\t\t\t<color sid=\"emission\">0 0 0 1</color>" << endl;
        stream << "﻿\t\t\t\t\t\t</emission>" << endl;
        stream << "﻿\t\t\t\t\t\t<diffuse>" << endl;
        stream << "﻿\t\t\t\t\t\t\t<color sid=\"diffuse\">" << d[0] << " " << d[1] << " " << d[2] << " " << t << "</color>" << endl;
        stream << "﻿\t\t\t\t\t\t</diffuse>" << endl;
        stream << "﻿\t\t\t\t\t\t<index_of_refraction>" << endl;
        stream << "﻿\t\t\t\t\t\t\t<float sid=\"ior\">1.45</float>" << endl;
        stream << "﻿\t\t\t\t\t\t</index_of_refraction>" << endl;
        stream << "﻿\t\t\t\t\t</lambert>" << endl;
        stream << "﻿\t\t\t\t</technique>" << endl;
        stream << "﻿\t\t\t</profile_COMMON>" << endl;
        stream << "﻿\t\t</effect>" << endl;
    }
    stream << "﻿\t</library_effects>" << endl;

    stream << "﻿\t<library_materials>" << endl;
    for (auto mat : materials) {
        string name = mat.second->getName();
        stream << "﻿\t\t<material id=\"" << name << "\" name=\"" << name << "\">" << endl;
        stream << "﻿\t\t\t<instance_effect url=\"#" << name << "_effect\"/>" << endl;
        stream << "﻿\t\t</material>" << endl;
    }
    stream << "﻿\t</library_materials>" << endl;


    stream << "\t<library_geometries>" << endl;
    for (auto obj : geos) {
        VRGeometryPtr geo = dynamic_pointer_cast<VRGeometry>(obj);
        string name = geo->getName();
        VRGeoData data(geo);
        auto mat = geo->getMaterial();

        stream << "\t\t<geometry id=\"" << name << "\" name=\"" << name << "\">" << endl;
        stream << "\t\t\t<mesh>" << endl;

        stream << "\t\t\t\t<source id=\"" << name << "_positions\">" << endl;
        stream << "\t\t\t\t\t<float_array count=\"" << data.size()*3 << "\" id=\"" << name << "_positions_array\">";
        for (int i=0; i<data.size(); i++) {
            if (i > 0) stream << " ";
            Pnt3d p = data.getPosition(i);
            stream << p[0] << " " << p[1] << " " << p[2];
        }
        stream << "</float_array>" << endl;
		stream << "\t\t\t\t\t<technique_common>" << endl;
		stream << "\t\t\t\t\t\t<accessor source=\"#box_normals_array\" count=\"" << data.size() << "\" stride=\"3\">" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"X\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"Y\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"Z\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t</accessor>" << endl;
		stream << "\t\t\t\t\t</technique_common>" << endl;
        stream << "\t\t\t\t</source>" << endl;

        stream << "\t\t\t\t<source id=\"" << name << "_normals\">" << endl;
        stream << "\t\t\t\t\t<float_array count=\"" << data.sizeNormals()*3 << "\" id=\"" << name << "_normals_array\">";
        for (int i=0; i<data.sizeNormals(); i++) {
            if (i > 0) stream << " ";
            Vec3d n = data.getNormal(i);
            stream << n[0] << " " << n[1] << " " << n[2];
        }
        stream << "</float_array>" << endl;
		stream << "\t\t\t\t\t<technique_common>" << endl;
		stream << "\t\t\t\t\t\t<accessor source=\"#box_normals_array\" count=\"" << data.sizeNormals() << "\" stride=\"3\">" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"X\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"Y\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t\t<param name=\"Z\" type=\"float\"/>" << endl;
		stream << "\t\t\t\t\t\t</accessor>" << endl;
		stream << "\t\t\t\t\t</technique_common>" << endl;
        stream << "\t\t\t\t</source>" << endl;

        stream << "\t\t\t\t<vertices id=\"" << name << "_vertices\">" << endl;
		stream << "\t\t\t\t\t<input semantic=\"POSITION\" source=\"#" << name << "_positions\" />" << endl;
		stream << "\t\t\t\t</vertices>" << endl;

        for (int i=0; i<data.getNTypes(); i++) {
            int type = data.getType(i);
            int length = data.getLength(i);

            int faceCount = length/3;
            string typeName = "triangles";
            if (type == GL_TRIANGLE_FAN) { typeName = "trifans"; faceCount = 1; }
            if (type == GL_TRIANGLE_STRIP) { typeName = "tristrips"; faceCount = 1; }
            if (type == GL_QUADS) { typeName = "triangles"; faceCount = length/4 * 2; } // times two because we convert to triangles
            if (type == GL_QUAD_STRIP) { typeName = "quadstrips"; faceCount = 1; } // probably not supported..

            stream << "\t\t\t\t<" << typeName << " count=\"" << faceCount << "\" material=\"" << mat->getName() << "\">" << endl;
            stream << "\t\t\t\t\t<input offset=\"0\" semantic=\"VERTEX\" source=\"#" << name << "_vertices\" />" << endl;
            stream << "\t\t\t\t\t<input offset=\"1\" semantic=\"NORMAL\" source=\"#" << name << "_normals\" />" << endl;
            stream << "\t\t\t\t\t<p>";
            if (type == GL_QUADS) { // convert to triangles on the fly
                for (int i=0; i<length/4; i++) {
                    if (i > 0) stream << " ";
                    stream << data.getIndex(i*4+0) << " " << data.getIndex(i*4+0, NormalsIndex) << " ";
                    stream << data.getIndex(i*4+1) << " " << data.getIndex(i*4+1, NormalsIndex) << " ";
                    stream << data.getIndex(i*4+2) << " " << data.getIndex(i*4+2, NormalsIndex) << " ";
                    stream << data.getIndex(i*4+0) << " " << data.getIndex(i*4+0, NormalsIndex) << " ";
                    stream << data.getIndex(i*4+2) << " " << data.getIndex(i*4+2, NormalsIndex) << " ";
                    stream << data.getIndex(i*4+3) << " " << data.getIndex(i*4+3, NormalsIndex);
                }
            } else {
                for (int i=0; i<length; i++) {
                    if (i > 0) stream << " ";
                    stream << data.getIndex(i) << " " << data.getIndex(i, NormalsIndex);
                }
            }
            stream << "</p>" << endl;
            stream << "\t\t\t\t</" << typeName << ">" << endl;
        }

        stream << "\t\t\t</mesh>" << endl;
        stream << "\t\t</geometry>" << endl;
    }
    stream << "\t</library_geometries>" << endl;

    // the scenes, actually only a single one..
	stream << "\t<library_visual_scenes>" << endl;
	stream << "\t\t<visual_scene id=\"the_visual_scene\">" << endl;
	writeSceneGraph(root, 3);
	stream << "\t\t</visual_scene>" << endl;
	stream << "\t</library_visual_scenes>" << endl;

    // the scene
    stream << "\t<scene>" << endl;
    stream << "\t\t<instance_visual_scene url=\"#the_visual_scene\" />" << endl;
    stream << "\t</scene>" << endl;

    stream << "﻿</COLLADA>" << endl;
}


