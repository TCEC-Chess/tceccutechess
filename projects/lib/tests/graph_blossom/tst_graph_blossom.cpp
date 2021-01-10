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

// uncomment for a lot of debug prints
// #define GRAPH_BLOSSOM_DEBUG_PRINTF(...) printf(__VA_ARGS__)

#include <graph_blossom.h>
#include <QtTest/QtTest>

#include <cstdlib>
#include <random>
#include <vector>

namespace
{
static bool operator == (const graph_blossom::Edge &e1, const graph_blossom::Edge &e2)
{
    return e1.m_v0 == e2.m_v0 && e1.m_v1 == e2.m_v1;
}

void printGraph(const graph_blossom::DenseGraph &g)
{
    for (size_t i = 0; i < g.numVertices(); ++i)
    {
        GRAPH_BLOSSOM_DEBUG_PRINTF("Vertex %2zu connections:", i);
        for (size_t j = 0; j < g.numVertices(); ++j)
            if (g.containsEdge(graph_blossom::Vertex(i), graph_blossom::Vertex(j)))
                GRAPH_BLOSSOM_DEBUG_PRINTF(" %zu", j);

        GRAPH_BLOSSOM_DEBUG_PRINTF("\n");
    }
}

}

class tst_GraphBlossom: public QObject
{
    Q_OBJECT

private slots:
    // trivial pairing test: adds N disjoint vertex pairs to the graph and
    // expects that those are found as matches
    void trivialPairing();

    // modification of the trivial pairing test: adds N disjoint vertex
    // pairs. every unconnected vertex is connected to every paired vertex. This
    // does not add new matches
    void pairingStars();

    // hand crafted cases
    void testCase1();
    void testCase2();
    void testCase3();

    void randomGraphs();

private:
    std::minstd_rand rnd;

    static bool checkMatch(const graph_blossom::DenseGraph &g, const graph_blossom::EdgeList &match);
};

bool tst_GraphBlossom::checkMatch(const graph_blossom::DenseGraph &g, const graph_blossom::EdgeList &match)
{
    GRAPH_BLOSSOM_DEBUG_PRINTF("match:");
    for (size_t i = 0; i < match.size(); ++i)
    {
        GRAPH_BLOSSOM_DEBUG_PRINTF(" %d-%d", match[i].m_v0, match[i].m_v1);
    }
    GRAPH_BLOSSOM_DEBUG_PRINTF("\n");

    // first: ensure that the match is a subset of the graph
    for (const graph_blossom::Edge &e : match)
        if (!g.containsEdge(e.m_v0, e.m_v1))
            return false;

    // second: check that the match edges are disjoint
    std::vector<bool> matchVertices;
    matchVertices.resize(g.numVertices());

    for (const graph_blossom::Edge &e : match)
    {
        if (matchVertices[e.m_v0])
            return false;
        matchVertices[e.m_v0] = true;

        if (matchVertices[e.m_v1])
            return false;
        matchVertices[e.m_v1] = true;
    }

    // checks passed
    return true;
}

void tst_GraphBlossom::trivialPairing()
{
    constexpr size_t numVertices { 20 };
    constexpr size_t iterationsPerSize { 30 };

    std::vector<graph_blossom::Vertex> vertexPermutation;
    vertexPermutation.resize(numVertices);
    for (size_t i = 0; i < numVertices; ++i)
        vertexPermutation[i] = static_cast<graph_blossom::Vertex>(i);

    for (size_t matches = 0; matches <= (numVertices / 2); ++matches)
        for (size_t j = 0; j < iterationsPerSize; ++j)
            for (size_t k = 0; k < matches; ++k)
            {
                std::shuffle(vertexPermutation.begin(), vertexPermutation.end(), rnd);

                // create graph with a number of matches
                graph_blossom::DenseGraph g(numVertices);

                for (size_t i = 0; i < matches; ++i)
                    g.insertEdge(vertexPermutation[i * 2], vertexPermutation[i * 2 + 1]);

                graph_blossom::EdgeList match = graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g);

                QCOMPARE(match.size(), matches);
                QVERIFY(checkMatch(g, match));

                // check that matches are expected
                for (size_t i = 0; i < matches; ++i)
                {
                    const graph_blossom::Edge expected { vertexPermutation[i * 2], vertexPermutation[i * 2 + 1] };
                    bool found = false;

                    for (const graph_blossom::Edge &e : match)
                    {
                        if (expected == e)
                        {
                            found = true;
                            break;
                        }
                    }
                    QVERIFY(found);
                }
            }
}

void tst_GraphBlossom::pairingStars()
{
    constexpr size_t numVertices { 20 };
    constexpr size_t iterationsPerSize { 30 };

    std::vector<graph_blossom::Vertex> vertexPermutation;
    vertexPermutation.resize(numVertices);
    for (size_t i = 0; i < numVertices; ++i)
        vertexPermutation[i] = static_cast<graph_blossom::Vertex>(i);

    for (size_t matches = 0; matches <= (numVertices / 2); ++matches)
        for (size_t j = 0; j < iterationsPerSize; ++j)
            for (size_t k = 0; k < matches; ++k)
            {
                std::shuffle(vertexPermutation.begin(), vertexPermutation.end(), rnd);

                // create graph with a number of matches
                graph_blossom::DenseGraph g(numVertices);

                for (size_t i = 0; i < matches; ++i)
                {
                    g.insertEdge(vertexPermutation[i * 2], vertexPermutation[i * 2 + 1]);

                    // connect paired vertices to every unpaired vertex
                    for (size_t m = matches * 2; m < numVertices; ++m)
                    {
                        g.insertEdge(vertexPermutation[i * 2], vertexPermutation[m]);
                        g.insertEdge(vertexPermutation[i * 2 + 1], vertexPermutation[m]);
                    }
                }

                printGraph(g);

                const graph_blossom::EdgeList match
                { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

                QCOMPARE(match.size(), std::min(numVertices / 2, matches * 2));
                QVERIFY(checkMatch(g, match));
            }
}

void tst_GraphBlossom::testCase1()
{
    graph_blossom::DenseGraph g(10);

    // cycle a
    g.insertEdge(0, 1);
    g.insertEdge(1, 2);
    g.insertEdge(2, 3);
    g.insertEdge(3, 4);
    g.insertEdge(4, 0);

    // cycle b
    g.insertEdge(5, 6);
    g.insertEdge(6, 7);
    g.insertEdge(7, 8);
    g.insertEdge(8, 9);
    g.insertEdge(9, 5);

    printGraph(g);

    {
        const graph_blossom::EdgeList match
        { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

        QVERIFY(checkMatch(g, match));
        QCOMPARE(match.size(), size_t { 4 });
    }

    g.insertEdge(1, 9);
    {
        const graph_blossom::EdgeList match
        { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

        QVERIFY(checkMatch(g, match));
        QCOMPARE(match.size(), size_t { 5 });
    }
}

void tst_GraphBlossom::testCase2()
{
    graph_blossom::DenseGraph g(10);

    g.insertEdge(0, 1);
    g.insertEdge(1, 2);
    g.insertEdge(2, 3);
    g.insertEdge(3, 4);

    g.insertEdge(5, 6);
    g.insertEdge(6, 7);
    g.insertEdge(7, 8);
    g.insertEdge(8, 9);

    g.insertEdge(1, 6);
    g.insertEdge(1, 7);
    g.insertEdge(1, 8);
    g.insertEdge(2, 7);
    g.insertEdge(2, 8);
    g.insertEdge(3, 6);
    g.insertEdge(3, 7);
    g.insertEdge(3, 6);

    printGraph(g);

    {
        const graph_blossom::EdgeList match
        { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

        QVERIFY(checkMatch(g, match));
        QCOMPARE(match.size(), size_t { 5 });
    }
}

void tst_GraphBlossom::testCase3()
{
    graph_blossom::DenseGraph g(10);

    g.insertEdge(0, 1);
    g.insertEdge(0, 2);
    g.insertEdge(1, 3);
    g.insertEdge(2, 3);

    g.insertEdge(1, 4);
    g.insertEdge(4, 5);
    g.insertEdge(3, 7);
    g.insertEdge(5, 7);

    g.insertEdge(5, 6);


    printGraph(g);

    {
        const graph_blossom::EdgeList match
        { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

        QVERIFY(checkMatch(g, match));
        QCOMPARE(match.size(), size_t { 4 });
    }
}

void tst_GraphBlossom::randomGraphs()
{
    graph_blossom::DenseGraph g(40);
    std::uniform_int_distribution<> distrib(0, 100);

    for (size_t density = 1; density <= 99; density += 3)
    {
        printf("density %zu%%: ", density);
        for (size_t iter = 0; iter < 100; ++iter)
        {
            printf(" %zu", iter);
            for (size_t i = 0; i < 40; ++i)
                for (size_t j = i + 1; j < 40; ++j)
                {
                    if (size_t(distrib(rnd)) <= density)
                        g.insertEdge(i, j);
                    else
                        g.removeEdge(i, j);
                }

//            if (density == 1 && iter == 6)
            {
                printGraph(g);
                const graph_blossom::EdgeList match
                { graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(g) };

                QVERIFY(checkMatch(g, match));
                printf(":%zu", match.size());
            }
        }
        printf("\n");
    }

}

QTEST_MAIN(tst_GraphBlossom)
#include "tst_graph_blossom.moc"
