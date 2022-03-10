#include "VRSpreadsheet.h"
#include "toString.h"

#include <iostream>
#include <OpenSG/OSGVector.h>

#include "xml.h"
#include "system/VRSystem.h"
#include "zipper/unzipper.h"

using namespace OSG;

VRSpreadsheet::VRSpreadsheet() {}
VRSpreadsheet::~VRSpreadsheet() {}

VRSpreadsheetPtr VRSpreadsheet::create() { return VRSpreadsheetPtr( new VRSpreadsheet() ); }
VRSpreadsheetPtr VRSpreadsheet::ptr() { return static_pointer_cast<VRSpreadsheet>(shared_from_this()); }

void VRSpreadsheet::read(string path) {
    if (!exists(path)) {
        cout << "VRSpreadsheet::read failed, path not found: '" << path << "'" << endl;
        return;
    }

    cout << "VRSpreadsheet::read: " << path << endl;
    string ext = getFileExtension(path);

    auto extractZipEntry = [](Unzipper& z, string name) {
        vector<unsigned char> vec;
        z.extractEntryToMemory(name, vec);
        return string(vec.begin(), vec.end());
    };

    if (ext == ".xlsx") {
        Unzipper z(path);
        auto entries = z.entries();

        sheets.clear();

        string workbookData1 = extractZipEntry(z, "xl/workbook.xml");
        string workbookData2 = extractZipEntry(z, "xl/_rels/workbook.xml.rels");
        auto sheets = parseWorkbook(workbookData1, workbookData2);

        string stringsData = extractZipEntry(z, "xl/sharedStrings.xml");
        auto strings = parseStrings(stringsData);

        for (auto s : sheets) {
            string path = "xl/" + s.first;
            string data = extractZipEntry(z, path);
            addSheet(s.second, data, strings);
        }

        return;
    }

    cout << "VRSpreadsheet::read Error: unknown extention " << ext << endl;
}

Vec2i VRSpreadsheet::convCoords(string d) {
    string c, r;
    for (char v : d) {
        if (v >= '0' && v <= '9') r += v;
        else c += v;
    }

    int R = toInt(r)-1; // because they start at 1

    vector<int> cv;
    for (char v : c) cv.push_back(int(v-'A'));

    int C = 0;
    int P = 1;
    for (int i = cv.size() - 1; i >= 0; i--) {
        C += P * cv[i];
        P *= 26;
    }

    return Vec2i(C, R);
}

vector<string> VRSpreadsheet::parseStrings(string& data) {
    XML xml;
    xml.parse(data);

    vector<string> res;
    for (auto si : xml.getRoot()->getChildren()) {
        string s = si->getChild(0)->getText();
        res.push_back(s);
    }

    return res;
}

map<string, string> VRSpreadsheet::parseWorkbook(string& data, string& relsData) {
    XML xml;
    xml.parse(data);
    XML rels;
    rels.parse(relsData);

    map<string, string> paths;
    auto relations = rels.getRoot();
    for (auto rel : relations->getChildren()) {
        string rID = rel->getAttribute("Id");
        paths[rID] = rel->getAttribute("Target");
    }

    map<string, string> res;
    auto sheets = xml.getRoot()->getChild("sheets");
    for (auto sheet : sheets->getChildren()) {
        string state = sheet->getAttribute("state");
        if (state == "hidden") continue;

        string name = sheet->getAttribute("name");
        string sheetId = sheet->getAttribute("id");
        string sheetPath = paths[sheetId];
        res[sheetPath] = name;
    }

    return res;
}

void VRSpreadsheet::addSheet(string& name, string& data, vector<string>& strings) {
    auto& sheet = sheets[name];
    sheet.name = name;
    sheet.xml = XML::create();
    sheet.xml->parse(data);
    auto root = sheet.xml->getRoot();

    sheet.NCols = 0;
    sheet.NRows = 0;

    auto sdata = root->getChild("sheetData");
    if (!sdata) {
        cout << "Warning, sheet " << name << " has no root, skip!" << endl;
        return;
    }

    for (auto row : sdata->getChildren()) {
        Row R;
        for (auto col : row->getChildren()) {
            Cell C;
            C.ID   = col->getAttribute("r");
            C.type = col->getAttribute("t");
            auto V = col->getChild("v");
            C.data = V ? V->getText() : "";
            if (C.type == "s") C.data = strings[toInt(C.data)];
            R.cells.push_back(C);
        }
        sheet.rows.push_back(R);
        sheet.NRows++;
        sheet.NCols = max(sheet.NCols, R.cells.size());
    }

    cout << " VRSpreadsheet::addSheet " << name << " " << Vec2i(sheet.NRows, sheet.NCols) << endl;
}

vector<string> VRSpreadsheet::getSheets() {
    vector<string> res;
    for (auto s : sheets) res.push_back(s.first);
    return res;
}

size_t VRSpreadsheet::getNColumns(string sheet) {
    if (!sheets.count(sheet)) return 0;
    //cout << "VRSpreadsheet::getNColumns of " << sheet << endl;
    //cout << sheets[sheet].xml->getRoot()->getChild("sheetData")->toString() << endl;
    return sheets[sheet].NCols;
}

size_t VRSpreadsheet::getNRows(string sheet) {
    if (!sheets.count(sheet)) return 0;
    return sheets[sheet].NRows;
}

vector<string> VRSpreadsheet::getRow(string sheet, size_t i) {
    vector<string> res;
    if (!sheets.count(sheet)) return res;
    if (i >= sheets[sheet].rows.size()) return res;

    auto& row = sheets[sheet].rows[i];
    for (auto& cell : row.cells) {
        res.push_back(cell.data);
    }
    return res;
}

vector< vector<string> > VRSpreadsheet::getRows(string sheet) {
    vector< vector<string> > res;
    if (!sheets.count(sheet)) return res;

    for (auto& row : sheets[sheet].rows) {
        vector<string> data;
        for (auto& cell : row.cells) {
            data.push_back(cell.data);
        }
        res.push_back(data);
    }
    return res;
}

string VRSpreadsheet::getCell(string sheet, size_t i, size_t j) {
    if (!sheets.count(sheet)) return "";
    if (j >= sheets[sheet].NRows) return "";
    if (i >= sheets[sheet].NCols) return "";
    if (j >= sheets[sheet].rows.size()) return "";
    if (i >= sheets[sheet].rows[j].cells.size()) return "";
    return sheets[sheet].rows[j].cells[i].data;
}

VRSpreadsheet::Sheet::Sheet() {}
VRSpreadsheet::Sheet::Sheet(string n) : name(n) {}
