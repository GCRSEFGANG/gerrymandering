/*=======================================
 graph.cpp:                     k-vernooy
 last modified:               Sat, Apr 11
 
 Definitions for graph theory related
 algorithm implementations such as
 searches and component counts.
========================================*/

#include <iostream>
#include <algorithm>
// #include "../include/graph.hpp"
#include "../include/shape.hpp"

using namespace Geometry;
using namespace std;


int Graph::get_num_components() {
    /*
        @desc: get number of components of a graph
        @params: none
        @return: `int` number of components
    */

    vector<bool> visited(vertices.size(), false);
    int x = 0;

    for (int i = 0; i < vertices.size(); i++) {
        if (visited[i] == false) {
            dfs_recursor((vertices.begin() + i).key(), visited); 
            x++;
        }
    }

    return x;
}


void Graph::dfs_recursor(int v, std::vector<bool>& visited) { 
    /*
        @desc: recur seach for each adjacent node to index v
        
        @params: 
            `int` v: index of node
            `std::vector<bool>&` visited: array of which
                nodes have been visited

        @return: void
    */

    visited[v] = true; 
    Node node = vertices[v];

    for (int i = 0; i < node.edges.size(); i++) {
        int t_id = node.edges[i][1];

        if (!visited[t_id]) { 
            dfs_recursor(t_id, visited);
        }
    }
} 


void Graph::add_edge(Edge edge) {
    /*
        @desc: Adds an edge to a graph object
        @params: `Geometry::Edge` edge: edge to be added
        @return: `void`
    */

    Edge hl, lh;

    if (edge[0] < edge[1]) {
        hl[0] = edge[0];
        hl[1] = edge[1];

        lh[0] = edge[1];
        lh[1] = edge[0];
    }
    else {
        hl[0] = edge[1];
        hl[1] = edge[0];

        lh[0] = edge[0];
        lh[1] = edge[1];
    }


    if (!(std::find(edges.begin(), edges.end(), hl) != edges.end())) {
        edges.push_back(hl);
    }

    if (!(std::find(vertices[hl[0]].edges.begin(),
            vertices[hl[0]].edges.end(), hl)
        != vertices[hl[0]].edges.end())) {

        vertices[hl[0]].edges.push_back(hl);
    }


    if (!(std::find(vertices[hl[1]].edges.begin(),
            vertices[hl[1]].edges.end(), lh) 
        != vertices[hl[1]].edges.end())) {

        vertices[hl[1]].edges.push_back(lh);
    }
}


std::vector<Edge> Graph::remove_edges_to(int id) {
    /*
        @desc: removes edges to a node id
        @params: `int` id: id to remove edges of
        @return: `std::vector<Geometry::Edge>` edges that have been removed
    */

    // 5 => {5, 1}, {5, 6}, {5, 2};

    vector<Edge> edges = vertices[id].edges;
    vertices[id].edges.clear();

    for (Edge edge : edges) {
        Edge remove = {edge[1], edge[0]};

        vertices[edge[1]].edges.erase(
            std::remove(vertices[edge[1]].edges.begin(),
                vertices[edge[1]].edges.end(),
                remove
            ),
            vertices[edge[1]].edges.end()
        );
    }

    return edges;
}


// void remove_edge(Edge edge);