// sala - a component of the depthmapX - spatial network analysis platform
// Copyright (C) 2000-2010, University College London, Alasdair Turner
// Copyright (C) 2011-2012, Tasos Varoudis
// Copyright (C) 2017-2018, Petros Koutsolampros

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "salalib/vgamodules/vgaangularshortestpath.h"

#include "genlib/stringutils.h"

bool VGAAngularShortestPath::run(Communicator *, const Options &, PointMap &map, bool) {

    auto &attributes = map.getAttributeTable();
    auto &selection_set = map.getSelSet();

    int linked_col = attributes.insertColumn("Angular Shortest Path Linked");
    int order_col = attributes.insertColumn("Angular Shortest Path Order");

    for (int i = 0; i < attributes.getRowCount(); i++) {
        PixelRef pix = attributes.getRowKey(i);
        map.getPoint(pix).m_misc = 0;
        map.getPoint(pix).m_dist = 0.0f;
        map.getPoint(pix).m_cumangle = -1.0f;
    }

    // in order to calculate Penn angle, the MetricPair becomes a metric triple...
    std::set<AngularTriple> search_list; // contains root point

    if (selection_set.size() != 2) {
        throw depthmapX::RuntimeException("Two nodes must be selected");
    }
    PixelRef pixelFrom = *selection_set.begin();
    PixelRef pixelTo = *std::next(selection_set.begin());

    search_list.insert(AngularTriple(0.0f, pixelFrom, NoPixel));
    map.getPoint(pixelFrom).m_cumangle = 0.0f;

    // note that m_misc is used in a different manner to analyseGraph / PointDepth
    // here it marks the node as used in calculation only
    std::map<PixelRef, PixelRef> parents;
    while (search_list.size()) {
        std::set<AngularTriple>::iterator it = search_list.begin();
        AngularTriple here = *it;
        search_list.erase(it);
        Point &p = map.getPoint(here.pixel);
        std::set<AngularTriple> newPixels;
        std::set<AngularTriple> mergePixels;
        // nb, the filled check is necessary as diagonals seem to be stored with 'gaps' left in
        if (p.filled() && p.m_misc != ~0) {
            p.getNode().extractAngular(newPixels, &map, here);
            p.m_misc = ~0;
            if (!p.getMergePixel().empty()) {
                Point &p2 = map.getPoint(p.getMergePixel());
                if (p2.m_misc != ~0) {
                    auto newTripleIter = newPixels.insert(AngularTriple(here.angle, p.getMergePixel(), NoPixel));
                    p2.m_cumangle = p.m_cumangle;
                    p2.getNode().extractAngular(mergePixels, &map, *newTripleIter.first);
                    for (auto &pixel : mergePixels) {
                        parents[pixel.pixel] = p.getMergePixel();
                    }
                    p2.m_misc = ~0;
                }
            }
        }
        for (auto &pixel : newPixels) {
            parents[pixel.pixel] = here.pixel;
        }
        newPixels.insert(mergePixels.begin(), mergePixels.end());
        for (auto &pixel : newPixels) {
            if (pixel.pixel == pixelTo) {
                int counter = 0;
                int row = attributes.getRowid(pixel.pixel);
                attributes.setValue(row, order_col, counter);
                counter++;
                int lastPixelRow = row;
                auto currParent = parents.find(pixel.pixel);
                row = attributes.getRowid(currParent->second);
                attributes.setValue(row, order_col, counter);

                if (!p.getMergePixel().empty() && p.getMergePixel() == currParent->first) {
                    attributes.setValue(row, linked_col, 1);
                    attributes.setValue(lastPixelRow, linked_col, 1);
                } else {
                    // apparently we can't just have 1 number in the whole column
                    attributes.setValue(row, linked_col, 0);
                }

                lastPixelRow = row;
                currParent = parents.find(currParent->second);
                counter++;
                if (currParent->first != here.pixel) {
                    row = attributes.getRowid(here.pixel);
                    attributes.setValue(row, order_col, counter);

                    if (!p.getMergePixel().empty() && p.getMergePixel() == currParent->first) {
                        attributes.setValue(row, linked_col, 1);
                        attributes.setValue(lastPixelRow, linked_col, 1);
                    } else {
                        // apparently we can't just have 1 number in the whole column
                        attributes.setValue(row, linked_col, 0);
                    }

                    lastPixelRow = row;
                    currParent = parents.find(here.pixel);
                    counter++;
                }
                while (currParent != parents.end()) {
                    Point &p = map.getPoint(currParent->second);
                    int row = attributes.getRowid(currParent->second);
                    attributes.setValue(row, order_col, counter);

                    if (!p.getMergePixel().empty() && p.getMergePixel() == currParent->first) {
                        attributes.setValue(row, linked_col, 1);
                        attributes.setValue(lastPixelRow, linked_col, 1);
                    } else {
                        // apparently we can't just have 1 number in the whole column
                        attributes.setValue(row, linked_col, 0);
                    }

                    lastPixelRow = row;
                    currParent = parents.find(currParent->second);
                    counter++;
                }

                map.overrideDisplayedAttribute(-2);
                map.setDisplayedAttribute(order_col);

                return true;
            }
        }
        search_list.insert(newPixels.begin(), newPixels.end());
    }

    return false;
}
