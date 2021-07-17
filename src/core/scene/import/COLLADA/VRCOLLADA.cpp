#include "VRCOLLADA.h"
#include "core/scene/import/VRImportFwd.h"
#include "core/utils/toString.h"
#include "core/utils/xml.h"

#include "core/objects/object/VRObject.h"
#include "core/objects/material/VRMaterial.h"
#include "core/objects/geometry/VRGeometry.h"
#include "core/objects/geometry/VRGeoData.h"

#include <OpenSG/OSGColor.h>

#include <iostream>
#include <stack>

using namespace OSG;

namespace OSG {

class VRCOLLADA_Stream : public XMLStreamHandler {
    private:
        struct Source {
            int count = 0;
            int stride = 0;
            vector<float> data;
        };

        struct Node {
            string name;
            string data;
            map<string, XMLAttribute> attributes;
        };

        struct Input {
            string source;
            string type;
            int offset = 0;
            int set = 0;
        };

        struct Primitive {
            string name;
            int count;
            int stride = 3;
            vector<Input> inputs;
        };

        stack<Node> nodeStack;

        map<string, VRMaterialPtr> library_effects;
        map<string, VRGeometryPtr> library_geometries;
        VRMaterialPtr currentMaterial;
        VRGeometryPtr currentGeometry;
        VRGeoDataPtr currentGeoData;
        Primitive currentPrimitive;
        bool inPrimitive = false;

        map<string, Source> sources;
        string currentSource;

        VRObjectPtr root;
        stack<VRObjectPtr> objStack;

        string skipHash(const string& s) { return subString(s, 1, s.size()-1); }

    public:
        VRCOLLADA_Stream(VRObjectPtr root) : root(root) {}
        ~VRCOLLADA_Stream() {}

        static VRCOLLADA_StreamPtr create(VRObjectPtr root) { return VRCOLLADA_StreamPtr( new VRCOLLADA_Stream(root) ); }

        void startDocument() {}
        void endDocument() {}
        void startElement(const string& uri, const string& name, const string& qname, const map<string, XMLAttribute>& attributes);
        void endElement(const string& uri, const string& name, const string& qname);
        void characters(const string& chars);
        void processingInstruction(const string& target, const string& data) {}

        void warning(const string& chars) { cout << "VRCOLLADA_Stream Warning" << endl; }
        void error(const string& chars) { cout << "VRCOLLADA_Stream Error" << endl; }
        void fatalError(const string& chars) { cout << "VRCOLLADA_Stream Fatal Error" << endl; }
        void onException(exception& e) { cout << "VRCOLLADA_Stream Exception" << endl; }
};

}

void VRCOLLADA_Stream::startElement(const string& uri, const string& name, const string& qname, const map<string, XMLAttribute>& attributes) {
    cout << "startElement " << name << " " << nodeStack.size() << endl;
    Node n;
    n.name = name;
    n.attributes = attributes;
    nodeStack.push(n);

    if (name == "effect") {
        string id = n.attributes["id"].val;
        auto m = VRMaterial::create();
        library_effects[id] = m;
        currentMaterial = m;
    }

    if (name == "geometry") {
        string Name = n.attributes["name"].val;
        string id = n.attributes["id"].val;
        auto m = VRGeometry::create(Name);
        library_geometries[id] = m;
        currentGeometry = m;
        currentGeoData = VRGeoData::create();
    }

    if (name == "source") {
        currentSource = n.attributes["id"].val;
        if (sources.count(currentSource)) cout << " -- AAAAAAAAAAAAAAAA" << endl;
        sources[currentSource] = Source();
        cout << " new source " << currentSource << endl;
    }

    if (name == "accessor") {
        cout << "  accessor " << currentSource << endl;
        if (currentSource != "") {
            sources[currentSource].count  = toValue<int>(n.attributes["count"].val);
            sources[currentSource].stride = toValue<int>(n.attributes["stride"].val);
            cout << "   count " << sources[currentSource].count << endl;
            cout << "   stride " << sources[currentSource].stride << endl;
        }
    }

    if (name == "input") {
        string type = n.attributes["semantic"].val;
        string sourceID = skipHash(n.attributes["source"].val);
        int offset = toInt(n.attributes["offset"].val);

        if (inPrimitive) {
            Input input;
            input.type = type;
            input.source = sourceID;
            input.offset = offset;
            currentPrimitive.inputs.push_back(input);
        }

        if (sources.count(sourceID)) {
            auto& source = sources[sourceID];


            cout << " " << type << " " << sourceID << " " << source.stride << " " << source.count << endl;

            if (currentGeoData) {
                if (type == "POSITION" && source.stride == 3) {
                    cout << "source position data: " << toString(source.data) << endl;
                    for (int i=0; i<source.count; i++) {
                        int k = i*source.stride;
                        Vec3d pos(source.data[k], source.data[k+1], source.data[k+2]);
                        currentGeoData->pushVert(pos);
                    }
                }

                if (type == "NORMAL" && source.stride == 3) {
                    cout << "source normal data: " << toString(source.data) << endl;
                    for (int i=0; i<source.count; i++) {
                        int k = i*source.stride;
                        Vec3d norm(source.data[k], source.data[k+1], source.data[k+2]);
                        currentGeoData->pushNorm(norm);
                    }
                }

                if (type == "TEXCOORD" && source.stride == 2) {
                    cout << "source tc data: " << toString(source.data) << endl;
                    int tcSlot = toInt( n.attributes["set"].val );
                    for (int i=0; i<source.count; i++) {
                        int k = i*source.stride;
                        Vec2d tc(source.data[k], source.data[k+1]);
                        if (tcSlot == 0) currentGeoData->pushTexCoord(tc);
                        if (tcSlot == 1) currentGeoData->pushTexCoord2(tc);
                    }
                }
            }
        }
    }

    if (name == "node") {
        string Name = n.attributes["name"].val;
        auto parent = objStack.size() > 0 ? objStack.top() : root;
        auto obj = VRTransform::create(Name);
        parent->addChild(obj);
        objStack.push(obj);
    }

    if (name == "instance_geometry") {
        string geoID = skipHash(n.attributes["url"].val);
        objStack.top()->addChild( library_geometries[geoID]->duplicate() );
    }

    if (name == "triangles") {
        inPrimitive = true;
        currentPrimitive = Primitive();
        currentPrimitive.name = name;
        currentPrimitive.count = toInt(n.attributes["count"].val);
        currentPrimitive.stride = 3;
    }
}

void VRCOLLADA_Stream::characters(const string& chars) {
    nodeStack.top().data = chars;
}

void VRCOLLADA_Stream::endElement(const string& uri, const string& name, const string& qname) {
    auto node = nodeStack.top();
    nodeStack.pop();

    if (name == "node") {
        objStack.pop();
    }

    if (node.name == "effect") currentMaterial = 0;
    if (node.name == "geometry") {
        currentGeoData->apply(currentGeometry);
        currentGeometry = 0;
        currentGeoData = 0;
        sources.clear();
    }

    if (node.name == "color") {
        Color4f col = toValue<Color4f>(node.data);
        string sid = node.attributes["sid"].val;
        if (sid == "diffuse" && currentMaterial) currentMaterial->setDiffuse(Color3f(col[0], col[1], col[2]));
        //if (sid == "emission" && currentMaterial) currentMaterial->setEmissive(col);
    }

    if (node.name == "float") {
        //float f = toValue<float>(node.data);
        //string sid = node.attributes["sid"].val;
        //if (sid == "ior" && currentMaterial) currentMaterial->setRefraction(f);
    }

    if (node.name == "float_array") {
            cout << " --- " << currentSource << endl;
        if (currentSource != "") {
            cout << " -- " << node.data << " -> " << toString(toValue<vector<float>>(node.data)) << endl;
            sources[currentSource].data = toValue<vector<float>>(node.data);
        }
    }

    if (name == "triangle") { inPrimitive = false; }

    if (node.name == "p" && currentGeoData && inPrimitive) {
        auto data = toValue<vector<int>>(node.data);
        auto& parent = nodeStack.top();

        if (parent.name == "triangles") {
            currentGeoData->pushType(GL_TRIANGLES);
            currentGeoData->pushLength(3*currentPrimitive.count);
            for (int i=0; i<currentPrimitive.count; i++) { // for each triangle
                int k = i * currentPrimitive.stride * currentPrimitive.inputs.size();
                for (int j=0; j<currentPrimitive.stride; j++) { // for each vertex
                    int l = k + j * currentPrimitive.inputs.size();
                    for (auto& input : currentPrimitive.inputs) { // for each input
                        int m = l + input.offset;
                        if (input.type == "VERTEX") currentGeoData->pushIndex(data[m]);
                        if (input.type == "NORMAL") currentGeoData->pushNormalIndex(data[m]);
                        if (input.type == "TEXCOORD") currentGeoData->pushTexCoordIndex(data[m]);
                    }
                }
            }
        }
    }

    cout << "endElement " << name << " " << nodeStack.size() << endl;
}

void OSG::loadCollada(string path, VRObjectPtr root) {
    auto xml = XML::create();

    auto handler = VRCOLLADA_Stream::create(root);
    xml->stream(path, handler.get());
}
