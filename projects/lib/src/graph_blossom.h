/*
    This file is part of Cute Chess.
    Copyright (C) 2020-2021 Cute Chess authors

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

// Blossom algorithm for dense non-directed graphs ( https://en.wikipedia.org/wiki/Blossom_algorithm )
//
// The algorithm can be used for efficiently checking whether a swiss tournament
// is pairable--i.e., whether there exists a pairing where two players won't
// meet again.

#ifndef GRAPH_BLOSSOM_H
#define GRAPH_BLOSSOM_H

#include <QDebug>
#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

// Note: define this before including this header to enable debug prints
#if !defined(GRAPH_BLOSSOM_DEBUG_PRINTF)
#define GRAPH_BLOSSOM_DEBUG_PRINTF(...) do { } while (false)
#endif

namespace graph_blossom
{

using Vertex = int;

struct Edge
{
    Vertex m_v0, m_v1; // v0 < v1

    Edge(Vertex v0, Vertex v1) :
        m_v0 { std::min(v0, v1) },
        m_v1 { std::max(v0, v1) }
    {
    }
};

using EdgeList = std::vector<Edge>;

class DenseGraph
{
private:
    using GraphType = std::vector<bool>;

    GraphType m_connections;
    const size_t m_vertices { };

public:
    DenseGraph(size_t numVertices) :
        m_vertices { numVertices }
    {
        m_connections.resize(numVertices * numVertices);
    }

    DenseGraph(const DenseGraph &o) :
        m_connections { o.m_connections },
        m_vertices { o.m_vertices }
    {
    }

    DenseGraph(DenseGraph &&o) :
        m_connections { std::move(o.m_connections) },
        m_vertices { std::move(o.m_vertices) }
    {
    }

    DenseGraph() = delete;
    DenseGraph &operator = (const DenseGraph &) & = delete;
    DenseGraph &operator = (DenseGraph &&) & = delete;

    bool containsEdge(Vertex v0, Vertex v1) const
    {
        if (v1 < v0)
            std::swap(v0, v1);

        return m_connections[v0 * m_vertices + v1];
    }

    void insertEdge(Vertex v0, Vertex v1)
    {
        if (v1 < v0)
            std::swap(v0, v1);

        m_connections[v0 * m_vertices + v1] = true;
    }

    void removeEdge(Vertex v0, Vertex v1)
    {
        if (v1 < v0)
            std::swap(v0, v1);

        m_connections[v0 * m_vertices + v1] = false;
    }

    size_t numVertices() const
    {
        return m_vertices;
    }

    size_t numEdges() const
    {
        size_t ret = 0;

        for (size_t i = 0; i < m_vertices; ++i)
            for (size_t j = i + 1; j < m_vertices; ++j)
                ret += size_t { m_connections[i * m_vertices + j] };

        return ret;
    }
};

class MaximumCardinalityMatcher
{
private:
    using Path = std::vector<Vertex>;

    // bidirectional: v0 --> v1 and v1 --> v0
    using MatchEdgeMap = std::map<Vertex, Vertex>;

    struct ForestNode
    {
        Vertex m_parent;      // -1 for no parent, i.e., this is root
        int m_distanceToRoot; // 0 for root

        ForestNode() : m_parent { -1 }, m_distanceToRoot { -1 }
        {
        }

        ForestNode(Vertex parent, int distanceToRoot) :
            m_parent { parent }, m_distanceToRoot { distanceToRoot }
        {
        }
    };

    static void addExposedVerticesAsForestRoots(
        const DenseGraph &graph, const MatchEdgeMap &matching,
        std::vector<ForestNode> &forestNodes, std::deque<Vertex> &unmarkedForestVertices)
    {
        const size_t numVertices = graph.numVertices();

        for (size_t v = 0; v < numVertices; ++v)
        {
            if (matching.find(v) == matching.end())
            {
                forestNodes[v] = ForestNode { -1, 0 };
                unmarkedForestVertices.push_back(v);
            }
        }
    }

    static void removeMatchedEdges(DenseGraph &unmarkedEdges, const MatchEdgeMap &matching)
    {
        for (const auto &e : matching)
            if (e.first < e.second)
                unmarkedEdges.removeEdge(e.first, e.second);
    }

    static Vertex getForestRoot(const std::vector<ForestNode> &forestNodes, Vertex x_id)
    {
        while (true)
        {
            const ForestNode &x_fn = forestNodes[x_id];
            if (x_fn.m_parent == -1)
                return x_id;

            x_id = x_fn.m_parent;
        }
    }

    static Vertex findClosestSharedParent(
        const std::vector<ForestNode> &forestNodes, Vertex x_id, Vertex y_id)
    {
        while (true)
        {
            if (x_id == y_id)
                return x_id; // found!

            const ForestNode &x_fn = forestNodes[x_id];
            const ForestNode &y_fn = forestNodes[y_id];

            if (x_fn.m_distanceToRoot >= y_fn.m_distanceToRoot)
                x_id = x_fn.m_parent;
            else
                y_id = y_fn.m_parent;
        }
    }

    // contract a graph and its matching
    // blossomNodes are the nodes that are renamed to blossomId
    static void contractGraph(DenseGraph &g, MatchEdgeMap &m,
                              const std::set<Vertex> &blossomNodes, Vertex blossomId)
    {
        // go through the graph, make the edges to blossom point to blossimId
        for (Vertex v0 = 0; v0 < static_cast<Vertex>(g.numVertices()); ++v0)
        {
            const bool v0InBlossom = blossomNodes.find(v0) != blossomNodes.end();

            for (Vertex v1 = v0 + 1; v1 < static_cast<Vertex>(g.numVertices()); ++v1)
            {
                if (!g.containsEdge(v0, v1))
                    continue;

                const bool v1InBlossom = blossomNodes.find(v1) != blossomNodes.end();

                if (v0InBlossom || v1InBlossom)
                {
                    g.removeEdge(v0, v1);
                    if (!v1InBlossom)
                        g.insertEdge(blossomId, v1);
                    if (!v0InBlossom)
                        g.insertEdge(v0, blossomId);
                }
            }
        }

        // next step: update matchings
        for (Vertex v : blossomNodes)
        {
            if (v == blossomId)
                continue; // keep the root matches

            m.erase(v); // delete everything else
        }
    }

    static Path liftPath(const Path &contractedPath, Vertex blossomId, Vertex v_id, Vertex w_id,
                         const DenseGraph &graph,
                         const std::vector<ForestNode> &forestNodes)
    {
        Path liftedPath;
        GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d contractedPath size=%zu\n", __FILE__, __LINE__, contractedPath.size());
        Q_ASSERT((contractedPath.size() % 2) == 0);

        GRAPH_BLOSSOM_DEBUG_PRINTF("Contracted path:");
        for (size_t i = 0; i < contractedPath.size(); ++i)
        {
            GRAPH_BLOSSOM_DEBUG_PRINTF(" %d", contractedPath.at(i));
        }
        GRAPH_BLOSSOM_DEBUG_PRINTF("\n");

        for (size_t i = 0; i < contractedPath.size(); ++i)
        {
            const Vertex x_id = contractedPath.at(i);

            if (x_id != blossomId)
                liftedPath.push_back(x_id); // not a contracted vertex
            else
            {
                // unroll the blossom
                Path blossomPath;
                const int b_dist = forestNodes.at(blossomId).m_distanceToRoot;
                const int v_dist = forestNodes.at(v_id).m_distanceToRoot;
                const int w_dist = forestNodes.at(w_id).m_distanceToRoot;

                blossomPath.resize(v_dist + w_dist - 2 * b_dist + 1);
                blossomPath[0] = blossomId; // root

                int d = v_dist - b_dist;
                for (Vertex y_id = v_id; y_id != blossomId; y_id = forestNodes[y_id].m_parent)
                    blossomPath[d--] = y_id;

                d = v_dist - b_dist + 1;
                for (Vertex y_id = w_id; y_id != blossomId; y_id = forestNodes[y_id].m_parent)
                    blossomPath[d++] = y_id;

                GRAPH_BLOSSOM_DEBUG_PRINTF("Blossom path:");
                for (size_t z = 0; z < blossomPath.size(); ++z)
                    GRAPH_BLOSSOM_DEBUG_PRINTF(" %d", blossomPath.at(z));
                GRAPH_BLOSSOM_DEBUG_PRINTF("\n");

                // blossom path constraints
                const Vertex prevVertex { i > 0 ? contractedPath.at(i - 1) : -1 };
                const Vertex nextVertex { i + 1 < contractedPath.size() ? contractedPath.at(i + 1) : -1 };
                GRAPH_BLOSSOM_DEBUG_PRINTF("prev=%d next=%d\n", prevVertex, nextVertex);
                size_t fromIndex = blossomPath.size();
                size_t toIndex = blossomPath.size();
                size_t maxPathLen = 0; // in vertices

                // find longest path through blossom: TODO check K init
                for (size_t k = 0; k < blossomPath.size(); ++k)
                {
                    // first check if the new path would be larger than the largest found so far
                    const size_t pathLen { 1 + (k % 2 == 0 ? k : blossomPath.size() - k) };
                    Q_ASSERT(pathLen % 2 == 1);
                    if (pathLen > maxPathLen)
                    {
                        // ok, new path would be bigger, but does it satisfy constraints?
                        if (prevVertex != -1 && nextVertex != -1)
                        {
                            GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d %d-%d=%d %d-%d=%d %d-%d=%d %d-%d=%d\n", __FILE__, __LINE__,
                                   prevVertex, blossomPath[0], graph.containsEdge(Edge { prevVertex, blossomPath[0] }),
                                   prevVertex, blossomPath[k], graph.containsEdge(Edge { prevVertex, blossomPath[k] }),
                                   blossomPath[0], nextVertex, graph.containsEdge(Edge { nextVertex, blossomPath[0] }),
                                   blossomPath[k], nextVertex, graph.containsEdge(Edge { nextVertex, blossomPath[k] }));

                            // blossom within path
                            if ((i % 2) == 0)
                            {
                                GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d\n", __FILE__, __LINE__);
                                // root must connect to prev vertex
                                const bool prevConnected = graph.containsEdge(prevVertex, blossomPath[0]);
                                const bool nextConnected = graph.containsEdge(nextVertex, blossomPath[k]);

                                if (!nextConnected || !prevConnected)
                                    continue;

                                fromIndex = 0;
                                toIndex = k;
                            }
                            else
                            {
                                GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d\n", __FILE__, __LINE__);
                                // k must connect to prev vertex
                                if (!graph.containsEdge(prevVertex, blossomPath[k]))
                                    continue;
                                // root must connect to next vertex
                                if (!graph.containsEdge(nextVertex, blossomPath[0]))
                                    continue;

                                fromIndex = k;
                                toIndex = 0;
                            }
                        }
                        else if (prevVertex == -1)
                        {
                            if (graph.containsEdge(blossomPath[k], nextVertex))
                            {
                                GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d %d-%d\n", __FILE__, __LINE__, nextVertex, blossomPath[k]);
                                fromIndex = 0;
                                toIndex = k;
                            }
                            else
                                continue;
                        }
                        else
                        {
                            Q_ASSERT(nextVertex == -1);
                            if (graph.containsEdge(blossomPath[k], prevVertex))
                            {
                                GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d %d-%d\n", __FILE__, __LINE__, prevVertex, blossomPath[k]);
                                fromIndex = k;
                                toIndex = 0;
                            }
                            else
                                continue;
                        }
                        maxPathLen = pathLen;
                    }
                }

                GRAPH_BLOSSOM_DEBUG_PRINTF("extracted path: fromix=%zu toix=%zu len=%zu\n",
                       fromIndex, toIndex, maxPathLen);

                // we have the path end points, extract it!
                // note: root is always in the extracted path
                if (fromIndex == 0)
                {
                    liftedPath.push_back(blossomPath[0]);
                    if ((toIndex % 2) == 0)
                    {
                        for (size_t j = 1; j <= toIndex; ++j)
                            liftedPath.push_back(blossomPath[j]);
                    }
                    else
                    {
                        for (size_t j = blossomPath.size() - 1; j >= toIndex; --j)
                            liftedPath.push_back(blossomPath[j]);
                    }
                }
                else
                {
                    Q_ASSERT(toIndex == 0);
                    if ((fromIndex % 2) == 0)
                    {
                        for (size_t j = fromIndex; j > 0; --j)
                            liftedPath.push_back(blossomPath[j]);
                    }
                    else
                    {
                        for (size_t j = fromIndex; j < blossomPath.size(); ++j)
                            liftedPath.push_back(blossomPath[j]);
                    }

                    liftedPath.push_back(blossomPath[0]);
                }
            }
        }

        GRAPH_BLOSSOM_DEBUG_PRINTF("%s:%d liftedPath size=%zu\n", __FILE__, __LINE__, liftedPath.size());

        if (liftedPath.size() > 0)
        {
            // verify that lifted path is proper
            GRAPH_BLOSSOM_DEBUG_PRINTF("lifted path: %d", liftedPath[0]);
            for (size_t i = 1; i < liftedPath.size(); ++i)
            {
                GRAPH_BLOSSOM_DEBUG_PRINTF(" %d", liftedPath[i]);
                Q_ASSERT(graph.containsEdge(liftedPath[i - 1], liftedPath[i]));
            }
            GRAPH_BLOSSOM_DEBUG_PRINTF("\n");
        }

        return std::move(liftedPath);
    }

    static Path findAugmentingPath(const DenseGraph &graph, const MatchEdgeMap &matching)
    {
        std::vector<ForestNode> forestNodes; // indexed by vertex id
        std::deque<Vertex> unmarkedForestVertices; // distance must be even
        DenseGraph unmarkedEdges { graph };

        forestNodes.resize(graph.numVertices());

        addExposedVerticesAsForestRoots(graph, matching, forestNodes, unmarkedForestVertices);
        removeMatchedEdges(unmarkedEdges, matching);

        while (!unmarkedForestVertices.empty())
        {
            const Vertex v_id = unmarkedForestVertices.front();
            unmarkedForestVertices.pop_front();
            const ForestNode &v_fn = forestNodes[v_id];

            // go through unmarked edges
            for (Vertex w_id = 0; w_id < static_cast<Vertex>(graph.numVertices()); ++w_id)
            {
                if (unmarkedEdges.containsEdge(v_id, w_id))
                {
                    const ForestNode &w_fn = forestNodes[w_id];
                    if (w_fn.m_distanceToRoot < 0)
                    {
                        // w is not in F
                        const Vertex x_id = matching.at(w_id);
                        forestNodes[w_id].m_parent = v_id;
                        forestNodes[w_id].m_distanceToRoot = v_fn.m_distanceToRoot + 1;
                        forestNodes[x_id].m_parent = w_id;
                        forestNodes[x_id].m_distanceToRoot = v_fn.m_distanceToRoot + 2;
                        unmarkedForestVertices.push_back(x_id);
                    }
                    else
                    {
                        if (w_fn.m_distanceToRoot % 2 == 0)
                        {
                            const Vertex rootOfV_id = getForestRoot(forestNodes, v_id);
                            const Vertex rootOfW_id = getForestRoot(forestNodes, w_id);

                            if (rootOfV_id != rootOfW_id)
                            {
                                Path p;
                                p.resize(v_fn.m_distanceToRoot + 1 + w_fn.m_distanceToRoot + 1);

                                // construct root(v) --> v
                                Vertex x_id = v_id;
                                for (int i = 0; i <= v_fn.m_distanceToRoot; ++i)
                                {
                                    p[v_fn.m_distanceToRoot - i] = x_id;
                                    x_id = forestNodes[x_id].m_parent;
                                }

                                // construct w --> root(w)
                                x_id = w_id;
                                for (int i = 0; i <= w_fn.m_distanceToRoot; ++i)
                                {
                                    p[v_fn.m_distanceToRoot + 1 + i] = x_id;
                                    x_id = forestNodes[x_id].m_parent;
                                }

                                return std::move(p);
                            }
                            else
                            {
                                // find the closest shared parent node
                                Vertex p_id = findClosestSharedParent(forestNodes, v_id, w_id);

                                // blossom detected: v -> p -> w -> v
                                // note that v or w may equal to p

                                std::set<Vertex> blossomNodes;

                                blossomNodes.insert(p_id);
                                for (Vertex x_id = v_id; x_id != p_id; x_id = forestNodes[x_id].m_parent)
                                    blossomNodes.insert(x_id);
                                for (Vertex x_id = w_id; x_id != p_id; x_id = forestNodes[x_id].m_parent)
                                    blossomNodes.insert(x_id);

                                DenseGraph contractedGraph { graph };
                                MatchEdgeMap contractedMatching { matching };
                                contractGraph(contractedGraph, contractedMatching, blossomNodes, p_id);

                                Path path { findAugmentingPath(contractedGraph, contractedMatching) };

                                Path liftedPath { liftPath(path, p_id, v_id, w_id, graph, forestNodes) };

                                return std::move(liftedPath);
                            }
                        }
                    }

                    unmarkedEdges.removeEdge(v_id, w_id);
                }
            }
        }

        return Path { }; // we didn't find anything
    }

public:
    static EdgeList findMaximumMatching(const DenseGraph &graph)
    {
        MatchEdgeMap matching;

        // initial matching: add everything that can be added trivially
        for (size_t i = 0; i < graph.numVertices(); ++i)
        {
            if (matching.find(i) != matching.end())
                continue;

            for (size_t j = i + 1; j < graph.numVertices(); ++j)
            {
                if (matching.find(j) != matching.end())
                    continue;

                if (graph.containsEdge(i, j))
                {
                    matching[i] = j;
                    matching[j] = i;
                    break;
                }
            }
        }

        while (true)
        {
            Path p { findAugmentingPath(graph, matching) };

            if (p.empty())
                break;

            // make sure the augmenting path is sane
            Q_ASSERT(p.size() % 2 == 0); // odd number of edges = even number of vertices
            Q_ASSERT(matching.find(p.front()) == matching.end()); // must be exposed vertex
            Q_ASSERT(matching.find(p.back()) == matching.end()); // must be exposed vertex

            // sanity for augmenting inner segments (1) -- augments matching
            for (size_t i = 1; i < p.size() - 1; i += 2)
            {
                Vertex v0 = p[i];
                Vertex v1 = p[i + 1];

                Q_ASSERT(matching.at(v0) == v1);
                Q_ASSERT(v0 == matching.at(v1));
            }

            // sanity for augmenting path (2) --  edges found in graph
            for (size_t i = 0; i < p.size() - 1; ++i)
                Q_ASSERT(graph.containsEdge(p[i], p[i + 1]));

            // augment matching based on path
            bool insertMode = true;

            for (size_t i = 0; i < p.size() - 1; ++i)
            {
                if (insertMode)
                {
                    matching[p[i]] = p[i + 1];
                    matching[p[i + 1]] = p[i];
                }

                insertMode = !insertMode;
            }
        }

        EdgeList ret;
        ret.reserve(graph.numVertices() / 2);

        for (const auto &p : matching)
            if (p.first < p.second) // we don't want to add edges twice
                ret.push_back(Edge(p.first, p.second));

        return std::move(ret);
    }
};

}

#endif
