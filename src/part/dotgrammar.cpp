/* This file is part of KGraphViewer.
   Copyright (C) 2006-2007 Gael de Chalendar <kleag@free.fr>

   KGraphViewer is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA
*/

#include "dotgrammar.h"
#include "DotGraphParsingHelper.h"
#include "dotdefaults.h"
#include "dotgraph.h"
#include "graphedge.h"
#include "graphnode.h"
#include "kgraphviewerlib_debug.h"

#include <iostream>

#include <QDebug>

#include <QFile>

#include <boost/spirit/include/classic_confix.hpp>
#include <boost/throw_exception.hpp>
namespace boost
{
void throw_exception(std::exception const &)
{
}
}

using namespace std;
using namespace boost;
using namespace boost::spirit::classic;

using namespace KGraphViewer;

#define KGV_MAX_ITEMS_TO_LOAD std::numeric_limits<size_t>::max()
#define BOOST_SPIRIT_DEBUG 1

DotGraphParsingHelper *phelper = nullptr;

std::string therenderop;
std::string thestr;

void anychar(char const c);

// keyword_p for C++
// (for basic usage instead of std_p)
const boost::spirit::classic::distinct_parser<> keyword_p("0-9a-zA-Z_");

template<typename ScannerT> DotGrammar::definition<ScannerT>::definition(DotGrammar const & /*self*/)
{
    graph = (!(keyword_p("strict")[&strict]) >> (keyword_p("graph")[&undigraph] | keyword_p("digraph")[&digraph]) >> !ID[&graphid] >> ch_p('{') >> !stmt_list >> ch_p('}'))[&finalactions];
    ID = ((((anychar_p - punct_p) | '_') >> *((anychar_p - punct_p) | '_')) | real_p | ('"' >> *((ch_p('\\') >> '"') | (anychar_p - '"')) >> '"') | (ch_p('<') >> *((anychar_p - '<' - '>') | tag) >> '>'));
    tag = ('<' >> *(anychar_p - '>') >> '>');
    stmt_list = stmt >> !(ch_p(';')) >> !(stmt_list);
    stmt = (attr_stmt | subgraph | edge_stmt | node_stmt | (ID >> '=' >> ID));

    attr_stmt = ((keyword_p("graph")[assign_a(phelper->attributed)] >> attr_list[&setattributedlist])[&setgraphattributes] | (keyword_p("node")[assign_a(phelper->attributed)] >> attr_list[&setattributedlist]) |
                 (keyword_p("edge")[assign_a(phelper->attributed)] >> attr_list[&setattributedlist]));

    attr_list = ch_p('[') >> !(a_list) >> ch_p(']');
    a_list = ((ID[&attrid] >> !('=' >> ID[&valid]))[&addattr] >> !(',' >> a_list));
    edge_stmt = ((node_id[&edgebound] | subgraph) >> edgeRHS >> !(attr_list[assign_a(phelper->attributed, "edge")]))[&pushAttrList][&setattributedlist][&createedges][&popAttrList];
    edgeRHS = edgeop[&checkedgeop] >> (node_id[&edgebound] | subgraph) >> !(edgeRHS);
    edgeop = str_p("->") | str_p("--");
    node_stmt = (node_id[&createnode] >> !(attr_list))[assign_a(phelper->attributed, "node")][&pushAttrList][&setattributedlist][&setnodeattributes][&popAttrList];
    node_id = (ID >> !(port));
    port = (ch_p(':') >> ID >> !(':' >> compass_pt)) | (':' >> compass_pt);
    subgraph = (!(keyword_p("subgraph") >> !(ID[&subgraphid])) >> ch_p('{')[&createsubgraph][&incrz][&pushAttrListC] >> stmt_list >> ch_p('}')[&decrz][&popAttrListC]) | (keyword_p("subgraph") >> ID[&subgraphid]);
    compass_pt = (keyword_p("n") | keyword_p("ne") | keyword_p("e") | keyword_p("se") | keyword_p("s") | keyword_p("sw") | keyword_p("w") | keyword_p("nw"));
}

void incrz(char const /*first*/)
{
    if (phelper) {
        phelper->z++;
        if (phelper->z > phelper->maxZ) {
            phelper->maxZ = phelper->z;
        }
    }
}

void anychar(char const c)
{
    Q_UNUSED(c);
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << c;
}

void decrz(char const /*first*/)
{
    if (phelper) {
        phelper->z--;
        phelper->gs = nullptr;
    }
}

void dump(char const *first, char const *last)
{
    std::string str(first, last);
    qCWarning(KGRAPHVIEWERLIB_LOG) << ">>>> " << QString::fromStdString(str) << " <<<<";
}

void strict(char const * /*first*/, char const * /*last*/)
{
    if (phelper)
        phelper->graph->strict(true);
}

void gotid(char const *first, char const *last)
{
    std::string id(first, last);
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Got ID  = '"<<QString::fromStdString(phelper->attrid)<<"'";
}

void undigraph(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Setting graph as undirected";
    if (phelper)
        phelper->graph->directed(false);
}

void digraph(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Setting graph as directed";
    if (phelper)
        phelper->graph->directed(true);
}

void graphid(char const *first, char const *last)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << QString::fromStdString(std::string(first,last));
    if (phelper)
        phelper->graph->setId(QString::fromStdString(std::string(first, last)));
}

void attrid(char const *first, char const *last)
{
    if (phelper) {
        std::string id(first, last);
        if (id.size() > 0 && id[0] == '"')
            id = id.substr(1);
        if (id.size() > 0 && id[id.size() - 1] == '"')
            id = id.substr(0, id.size() - 1);
        phelper->attrid = id;
        phelper->valid = "";
        //     qCDebug(KGRAPHVIEWERLIB_LOG) << "Got attr ID  = '"<<QString::fromStdString(phelper->attrid)<<"'";
    }
}

void subgraphid(char const *first, char const *last)
{
    std::string id(first, last);
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << QString::fromStdString(id);
    if (phelper) {
        if (id.size() > 0 && id[0] == '"')
            id = id.substr(1);
        if (id.size() > 0 && id[id.size() - 1] == '"')
            id = id.substr(0, id.size() - 1);
        phelper->subgraphid = id;
        //     qCDebug(KGRAPHVIEWERLIB_LOG) << "Got subgraph id = '"<<QString::fromStdString(phelper->subgraphid)<<"'";
    }
}

void valid(char const *first, char const *last)
{
    std::string id(first, last);
    if (phelper) {
        if (id.size() > 0 && id[0] == '"')
            id = id.substr(1);
        if (id.size() > 0 && id[id.size() - 1] == '"')
            id = id.substr(0, id.size() - 1);
        phelper->valid = id;
        //     qCDebug(KGRAPHVIEWERLIB_LOG) << "Got attr val = '"<<QString::fromStdString(id)<<"'";
    }
}

void addattr(char const * /*first*/, char const * /*last*/)
{
    if (phelper) {
        phelper->attributes.insert(std::make_pair(phelper->attrid, phelper->valid));
    }
}

void pushAttrListC(char const /*c*/)
{
    pushAttrList(nullptr, nullptr);
}

void pushAttrList(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Pushing attributes";
    if (phelper) {
        phelper->graphAttributesStack.push_back(phelper->graphAttributes);
        phelper->nodesAttributesStack.push_back(phelper->nodesAttributes);
        phelper->edgesAttributesStack.push_back(phelper->edgesAttributes);
    }
}

void popAttrListC(char const /*c*/)
{
    popAttrList(nullptr, nullptr);
}

void popAttrList(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Poping attributes";
    if (phelper) {
        phelper->graphAttributes = phelper->graphAttributesStack.back();
        phelper->graphAttributesStack.pop_back();
        phelper->nodesAttributes = phelper->nodesAttributesStack.back();
        phelper->nodesAttributesStack.pop_back();
        phelper->edgesAttributes = phelper->edgesAttributesStack.back();
        phelper->edgesAttributesStack.pop_back();
    }
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Poped";
}

void createnode(char const *first, char const *last)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << (void*)first << (void*)last << QString::fromStdString(std::string(first,last));
    if (phelper && first && last) {
        std::string id(first, last);
        if (id.size() > 0 && id[0] == '"')
            id = id.substr(1);
        if (id.size() > 0 && id[id.size() - 1] == '"')
            id = id.substr(0, id.size() - 1);
        phelper->createnode(id);
    }
}

void createsubgraph(char const /*c*/)
{
    if (phelper) {
        phelper->createsubgraph();
    }
}

void setgraphattributes(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "setgraphattributes with z = " << phelper->z;
    if (phelper) {
        if (phelper->z == 1) // main graph
        {
            phelper->setgraphattributes();
        } else {
            phelper->setsubgraphattributes();
        }
    }
}

void setnodeattributes(char const * /*first*/, char const * /*last*/)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "setnodeattributes with z = " << phelper->z;
    if (phelper) {
        phelper->setnodeattributes();
    }
}

void setattributedlist(char const * /*first*/, char const * /*last*/)
{
    if (phelper) {
        phelper->setattributedlist();
    }
}

void checkedgeop(char const *first, char const *last)
{
    std::string str(first, last);
    if (phelper) {
        if (((phelper->graph->directed()) && (str == "->")) || ((!phelper->graph->directed()) && (str == "--")))
            return;

        qCWarning(KGRAPHVIEWERLIB_LOG) << "Error !! uncoherent relation : directed = '" << phelper->graph->directed() << "' and op = '" << QString::fromStdString(str) << "'";
    }
}

void edgebound(char const *first, char const *last)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "edgebound: " << QString::fromStdString(std::string(first,last));
    if (phelper) {
        std::string id(first, last);
        if (id.size() > 0 && id[0] == '"')
            id = id.substr(1);
        if (id.size() > 0 && id[id.size() - 1] == '"')
            id = id.substr(0, id.size() - 1);
        phelper->edgebound(id);
    }
}

void createedges(char const * /*first*/, char const * /*last*/)
{
    if (phelper) {
        phelper->createedges();
    }
}

void finalactions(char const * /*first*/, char const * /*last*/)
{
    if (phelper) {
        phelper->finalactions();
    }
}

bool parse_point(char const *str, QPoint &p)
{
    int x, y;
    bool res;
    res = parse(str, (int_p[assign_a(x)] >> ',' >> int_p[assign_a(y)]), +space_p).full;
    if (!res)
        return false;
    p = QPoint(x, y);
    return true;
}

bool parse_numeric_color(char const *str, QColor &c)
{
    if (str == nullptr)
        return false;
    int r, g, b, a;
    bool res;
    uint_parser<unsigned, 16, 2, 2> hex2digits_p;
    res = parse(str, (ch_p('#') >> hex2digits_p[assign_a(r)] >> hex2digits_p[assign_a(g)] >> hex2digits_p[assign_a(b)] >> !hex2digits_p[assign_a(a)]), +space_p).full;
    if (res) {
        c.setRgb(r, g, b);
        return true;
    }

    double h, s, v;
    res = parse(str, (real_p[assign_a(h)] >> !ch_p(',') >> real_p[assign_a(s)] >> !ch_p(',') >> real_p[assign_a(v)]), +space_p).full;
    if (res) {
        c.setHsv(int(255 * h), int(255 * s), int(255 * v));
        return true;
    }
    return false;
}

bool parse_real(char const *str, double &d)
{
    return parse(str, (real_p[assign_a(d)]), +space_p).full;
}

bool parse_integers(char const *str, std::vector<int> &v)
{
    return parse(str, (int_p[push_back_a(v)] >> *(',' >> int_p[push_back_a(v)])), +space_p).full;
}

bool parse_spline(char const *str, QVector<QPair<float, float>> &points)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Parsing spline..." << QString::fromStdString(str);
    char e = 'n', s = 'n';
    int sx, sy, ex, ey;
    QPair<float, float> p;
    bool res;
    res = parse(str,
                (!(ch_p('e')[assign_a(e)] >> ',' >> int_p[assign_a(ex)] >> ',' >> int_p[assign_a(ey)]) >> !(ch_p('s')[assign_a(s)] >> ',' >> int_p[assign_a(sx)] >> ',' >> int_p[assign_a(sy)]) >>
                 ((int_p[assign_a(p.first)] >> ',' >> int_p[assign_a(p.second)]))[push_back_a(points, p)] >>
                 +((int_p[assign_a(p.first)] >> ',' >> int_p[assign_a(p.second)])[push_back_a(points, p)] >> (int_p[assign_a(p.first)] >> ',' >> int_p[assign_a(p.second)])[push_back_a(points, p)] >>
                   (int_p[assign_a(p.first)] >> ',' >> int_p[assign_a(p.second)])[push_back_a(points, p)])),
                +space_p)
              .full;
    if (!res)
        return false;
    if (s == 's') {
        //     qCDebug(KGRAPHVIEWERLIB_LOG) << "inserting the s point";
        points.insert(points.begin(), qMakePair(float(sx), float(sy)));
    }
    if (e == 'e') {
        //     points.insert(points.begin(), qMakePair(float(ex),float(ey)));
    }
    return true;
}

DotRenderOp renderop;

DotRenderOpVec *renderopvec = nullptr;

void init_op()
{
    renderop = DotRenderOp();
}

void valid_op(char const *first, char const *last)
{
    std::string s(first, last);
    renderop.renderop = QString::fromUtf8(therenderop.c_str());
    renderop.str = QString::fromUtf8(thestr.c_str());

    //   qCDebug(KGRAPHVIEWERLIB_LOG) << "Validating render operation '"<<QString::fromStdString(s)<<"': '"<<renderop.renderop<<"/"<<renderop.str<<"'";
    renderopvec->push_back(renderop);
    renderop.renderop = "";
    renderop.integers = QList<int>();
    renderop.str = "";
}

bool parse_renderop(const std::string &str, DotRenderOpVec &arenderopvec)
{
    //   qCDebug(KGRAPHVIEWERLIB_LOG) << QString::fromUtf8(str.c_str()) << str.size();
    if (str.empty()) {
        return false;
    }
    init_op();
    renderopvec = &arenderopvec;
    bool res;
    int c;
    res = parse(str.c_str(),
                (+(((ch_p('E') | ch_p('e'))[assign_a(therenderop)] >> +space_p >> repeat_p(4)[int_p[push_back_a(renderop.integers)] >> !((ch_p(',') | ch_p('.')) >> int_p) >> +space_p])[&valid_op] |
                   ((ch_p('P') | ch_p('p') | ch_p('L') | ch_p('B') | ch_p('b'))[assign_a(therenderop)] >> +space_p >> int_p[assign_a(c)][push_back_a(renderop.integers)] >> +space_p >>
                    repeat_p(boost::ref(c))[int_p[push_back_a(renderop.integers)] >> !((ch_p(',') | ch_p('.')) >> int_p) >> +space_p >> int_p[push_back_a(renderop.integers)] >> !((ch_p(',') | ch_p('.')) >> int_p) >> +space_p])[&valid_op]
                   // "T 1537 228 0 40 9 -#1 (== 0) T 1537 217 0 90 19 -MAIN:./main/main.pl "
                   // T x y j w n -b1b2...bn 	Text drawn using the baseline point (x,y). The text consists of the n bytes following '-'. The text should be left-aligned (centered, right-aligned) on the point if j is -1 (0, 1),
                   // respectively. The value w gives the width of the text as computed by the library. I x y w h n -b1b2...bn 	Externally-specified image drawn in the box with lower left corner (x,y) and upper right corner (x+w,y+h). The
                   // name of the image consists of the n bytes following '-'. This is usually a bitmap image. Note that the image size, even when converted from pixels to points, might be different from the required size (w,h). It is
                   // assumed the renderer will perform the necessary scaling. (1.2)
                   | ((ch_p('T') | ch_p('I'))[assign_a(therenderop)] >> +space_p >> repeat_p(4)[int_p[push_back_a(renderop.integers)] >> !((ch_p(',') | ch_p('.')) >> int_p) >> +space_p] >> int_p[assign_a(c)] >> +space_p >> '-' >>
                      (repeat_p(boost::ref(c))[anychar_p])[assign_a(thestr)] >> +space_p)[&valid_op]
                   // c 9 -#000000ff
                   | ((ch_p('C') | ch_p('c') | ch_p('S'))[assign_a(therenderop)] >> +space_p >> int_p[assign_a(c)] >> +space_p >> '-' >> (repeat_p(boost::ref(c))[anychar_p])[assign_a(thestr)] >> +space_p)[&valid_op]
                   // t 0
                   // t f 	Set font characteristics. The integer f is the OR of BOLD=1, ITALIC=2, UNDERLINE=4, SUPERSCRIPT=8, SUBSCRIPT=16, (1.5) STRIKE-THROUGH=32 (1.6), and OVERLINE=64 (1.7).
                   | ((ch_p('t'))[assign_a(therenderop)] >> +space_p >> int_p[assign_a(c)] >> +space_p)[&valid_op]
                   // F 14,000000 11 -Times-Roman
                   | (ch_p('F')[assign_a(therenderop)] >> +space_p >> int_p[push_back_a(renderop.integers)] >> !((ch_p(',') | ch_p('.')) >> int_p) >> +space_p >> int_p[assign_a(c)] >> +space_p >> '-' >>
                      (repeat_p(boost::ref(c))[anychar_p])[assign_a(thestr)] >> +space_p)[&valid_op])) >>
                    !end_p)
              .full;
    if (res == false) {
        qCWarning(KGRAPHVIEWERLIB_LOG) << "ERROR: parse_renderop failed on " << QString::fromStdString(str);
        qCWarning(KGRAPHVIEWERLIB_LOG) << "       Last renderop string is " << QString::fromStdString(str.c_str());
    }
    //   delete renderop; renderop = 0;
    return res;
}

bool parse(const std::string &str)
{
    DotGrammar g;
    return boost::spirit::classic::parse(str.c_str(), g >> end_p, (+boost::spirit::classic::space_p | boost::spirit::classic::comment_p("/*", "*/"))).full;
}
