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


#include "swisstournament.h"
#include <algorithm>
#include <QDebug>

#include "graph_blossom.h"

SwissTournament::SwissTournament(GameManager* gameManager,
                                 EngineManager* engineManager,
                                 QObject *parent)
    : Tournament(gameManager, engineManager, parent)
{
}

QString SwissTournament::type() const
{
    return "swiss-tcec";
}

QPair<int, int> SwissTournament::getPairForGame(int gameNumber) const
{
    QPair<int, int> thePair { };
    int pairNum = -1;
    int encounterNum = -1;

    int round = gameNumber / gamesPerRound();
    int gameInRound = gameNumber % gamesPerRound();

    if (bergerSchedule())
    {
        // first play: 2-1, 4-3, ...; then play 1-2, 3-4, ...
        pairNum = gameInRound % gamesPerCycle();
        encounterNum = gameInRound / gamesPerCycle();
    }
    else
    {
        pairNum = gameInRound / gamesPerEncounter();
        encounterNum = gameInRound % gamesPerEncounter();
    }

    thePair = m_encounterHistory[round * gamesPerCycle() + pairNum];

    // swap colors on second encounter
    if (encounterNum % 2 == 1)
        thePair = qMakePair(thePair.second, thePair.first);

    return thePair;
}



QList< QPair<QString, QString> > SwissTournament::getPairings()
{
    QList< QPair<QString, QString> > pList;
    const int numGames = roundMultiplier() * gamesPerEncounter() * gamesPerCycle();

    for (int g = 0; g < numGames; ++g)
    {
        const auto pair = getPairForGame(g);
        if (pair != qMakePair(0,0))
        {
            pList.append(qMakePair(playerAt(pair.first).builder()->name(),
                                   playerAt(pair.second).builder()->name()));
        }
        else
        {
            pList.append(qMakePair(QString(""), QString("")));
        }
    }

    return pList;
}

void SwissTournament::addResumeGameResult(int gameNumber, const QString &result)
{
    qWarning() << "Adding resumed game result: " << gameNumber << result;
    while (m_preRecordedResults.size() <= gameNumber)
        m_preRecordedResults.append(QString());

    m_preRecordedResults[gameNumber] = result;
}

void SwissTournament::initializePairing()
{
    m_playerStats.clear();
    m_playerStats.resize(playerCount());

    m_pairings.clear();
    m_encounterHistory.clear();
    m_encounterHistory.resize(gamesPerCycle() * roundMultiplier());
    m_ignoreRoundsForEncounters = 0;
}

int SwissTournament::gamesPerCycle() const
{
    return playerCount() / 2;
}

int SwissTournament::gamesPerRound() const
{
    return gamesPerCycle() * gamesPerEncounter();
}

SwissTournament::EncountersTable::EncountersTable(int numPlayers) : m_numPlayers(numPlayers)
{
    m_encounters.resize(numPlayers * numPlayers);
}

void SwissTournament::EncountersTable::clear()
{
    std::fill(m_encounters.begin(), m_encounters.end(), false);
}

void SwissTournament::EncountersTable::addEncounter(int player1, int player2)
{
    if (player1 > player2)
        std::swap(player1, player2);

    m_encounters[player2 * m_numPlayers + player1] = true;
}

bool SwissTournament::EncountersTable::hasMet(int player1, int player2) const
{
    if (player1 > player2)
        std::swap(player1, player2);

    return m_encounters[player2 * m_numPlayers + player1];
}

bool SwissTournament::tryPairing(const QVector<PairingData> &pairingData, int playerIndex1, int playerIndex2,
                                 const EncountersTable &encounters) const
{
    std::vector<bool> paired;

    paired.resize(pairingData.size());

    for (const PairingData &pd : pairingData)
        paired[pd.playerIndex] = pd.paired;

    if (playerIndex1 >= 0)
        paired[playerIndex1] = true;

    if (playerIndex2 >= 0)
        paired[playerIndex2] = true;

    graph_blossom::DenseGraph pairingGraph(playerCount());
    size_t numUnpaired = 0;

    // build graph for allowed pairings
    for (size_t i = 0; i < paired.size(); ++i)
    {
        if (!paired[i])
        {
            ++numUnpaired;

            for (size_t j = i + 1; j < paired.size(); ++j)
            {
                if (!paired[j] && !encounters.hasMet(i, j))
                    pairingGraph.insertEdge(i, j);
            }
        }
    }
    const graph_blossom::EdgeList matching {
        graph_blossom::MaximumCardinalityMatcher::findMaximumMatching(pairingGraph) };

    const size_t numMatched = 2 * matching.size(); // 2 players per match

    return numMatched == numUnpaired;
}

void SwissTournament::rebuildEncountersSet(EncountersTable &encounters) const
{
    encounters.clear();

    for (int r0 = m_ignoreRoundsForEncounters; r0 < currentRound() - 1; ++r0)
    {
        // note: r0 is the zero-based round counter

        for (int g = 0; g < gamesPerCycle(); ++g)
        {
            const QPair<int, int> pair = m_encounterHistory[r0 * gamesPerCycle() + g];

            encounters.addEncounter(pair.first, pair.second);
        }
    }
}

void SwissTournament::generatePairingOrder(QVector<PairingData> &pairingData) const
{
    for (int i = 0; i < playerCount(); ++i)
    {
        auto &entry = pairingData[i];

        entry.playerIndex = i;
        entry.score = playerAt(i).score();
        entry.paired = false;
    }

    qSort(pairingData);
}

void SwissTournament::assignByeIfNecessary(QVector<PairingData> &pairingData)
{
    // BYE needed only for odd number of players
    if ((playerCount() % 2) == 0)
        return;

    bool allByes = true;

    // first check whether everyone already has a BYE
    for (int i = 0; allByes && i < playerCount(); ++i)
    {
        if (!m_playerStats[i].byeReceived)
            allByes = false;
    }

    // everyone has a BYE, reset the BYEs
    if (allByes)
    {
        qWarning() << "- Reset BYEs";
        for (int i = 0; allByes && i < playerCount(); ++i)
            m_playerStats[i].byeReceived = false;
    }

    // assign BYE to the lowest ranked player without a BYE
    for (int i = playerCount() - 1; i >= 0; --i)
    {
        auto &entry = pairingData[i];
        PlayerStats &stats = m_playerStats[entry.playerIndex];

        if (stats.byeReceived)
            continue;

        // BYE
        stats.byeReceived = true;
        entry.paired = true;

        for (int j = 0; j < gamesPerEncounter(); ++j)
            addScore(entry.playerIndex, 2); // BYE games are wins

        qWarning() << "- Added BYE for player" << entry.playerIndex;
        break;
    }
}

bool SwissTournament::determineColorIsFirstWhite(int firstPlayer, const PlayerStats &firstStats,
                                                 int secondPlayer, const PlayerStats &secondStats) const
{
    if ((gamesPerEncounter() % 2) == 0)
        return false; // double rounds: first is always black on first encounter

    // first we balance the white/black games
    if (firstStats.whiteGameDiff < secondStats.whiteGameDiff)
        return true;

    if (firstStats.whiteGameDiff > secondStats.whiteGameDiff)
        return false;

    // Higher-scored player always gets black -- but this can only be the first player
    const int firstScore = playerAt(firstPlayer).score();
    const int secondScore = playerAt(secondPlayer).score();
    Q_ASSERT(firstScore >= secondScore);
    if (firstScore > secondScore)
        return false;

    // even score, even white game diff. Use prefixed pattern
    switch ((currentRound() - 1) % 4)
    {
    case 0: return false;
    case 1: return true;
    case 2: return true;
    case 3: return false;

    default:
        Q_UNREACHABLE();
        return false;
    }
}

void SwissTournament::assignPairs(QVector<PairingData> &pairingData, EncountersTable &encounters)
{
    m_pairings.clear();
    m_pairings.resize(playerCount() / 2);

    // do pairing
    int pairNo = 0;

    for (int i = 0; i < playerCount() / 2; ++i)
    {
        // first unpaired player
        int firstUnpaired = -1;

        // first unpaired
        for (int j = 0; j < playerCount(); ++j)
        {
            auto &entry = pairingData[j];

            if (!entry.paired)
            {
                entry.paired = true;
                firstUnpaired = entry.playerIndex;
                break;
            }
        }

        // find match
        for (int j = 0; j < playerCount(); ++j)
        {
            auto &entry = pairingData[j];

            if (entry.paired)
                continue;

            const int secondUnpaired = entry.playerIndex;

            // do we have an encounter?
            if (encounters.hasMet(firstUnpaired, secondUnpaired))
            {
                continue;
            }

            if (!tryPairing(pairingData,
                            std::min(firstUnpaired, secondUnpaired),
                            std::max(firstUnpaired, secondUnpaired),
                            encounters))
            {
                continue;
            }

            // new encounter
            entry.paired = true;
            encounters.addEncounter(firstUnpaired, secondUnpaired);

            // STEP 5: Determine color
            // the player who has more black games gets
            // white. If equal number, then higher ranked gets black.

            PlayerStats &firstStats = m_playerStats[firstUnpaired];
            PlayerStats &secondStats = m_playerStats[secondUnpaired];

            const bool isFirstWhite =
                determineColorIsFirstWhite(firstUnpaired, firstStats,
                                           secondUnpaired, secondStats);

            QPair<int, int> newPair;
            if (isFirstWhite)
            {
                newPair = qMakePair(firstUnpaired, secondUnpaired);
                if (gamesPerEncounter() % 2)
                {
                    firstStats.whiteGameDiff++;
                    secondStats.whiteGameDiff--;
                }
            }
            else
            {
                newPair = qMakePair(secondUnpaired, firstUnpaired);
                if (gamesPerEncounter() % 2)
                {
                    firstStats.whiteGameDiff--;
                    secondStats.whiteGameDiff++;
                }
            }

            // add pairs in reverse order
            ++pairNo;
            m_pairings[m_pairings.size() - pairNo] = newPair;
            qInfo() << "Added PAIR"
                    << playerAt(newPair.first).builder()->name()
                    << "-"
                    << playerAt(newPair.second).builder()->name();

            break;
        }
    }
}

// See https://wiki.chessdom.org/TCEC_Swiss_Tournament_System
void SwissTournament::generateRoundPairings()
{
    QVector<PairingData> pairingData;
    pairingData.resize(playerCount());

    qWarning() << "Generate pairings for round" << currentRound();

    // STEP 1: Generate pairing order
    generatePairingOrder(pairingData);

    // Print out the pairing order
    for (int i = 0; i < playerCount(); ++i)
    {
        const PairingData &entry = pairingData[i];
        const PlayerStats &stats = m_playerStats[entry.playerIndex];

        qInfo() << playerAt(entry.playerIndex).builder()->name()
                << "  SCORE:" << entry.score
                << "  SEED:" << entry.playerIndex
                << "  White game diff:" << stats.whiteGameDiff
                << "  Received BYE:" << stats.byeReceived;
    }

    // STEP 2: assign BYE
    assignByeIfNecessary(pairingData);

    // STEP 3: Determine whether there exists a viable pairing, if not, ignore rounds
    EncountersTable encounters(playerCount());
    while (true)
    {
        rebuildEncountersSet(encounters);

        // another helpful print
        for (int i = 0; i < playerCount(); ++i)
        {
            QString met;
            for (int j = 0; j < playerCount(); ++j)
                met += encounters.hasMet(i,j) ? "x" : " ";

            qInfo() << "EncounterMatrix:" << met;
        }

        // pairable?
        if (tryPairing(pairingData, -1, -1, encounters))
            break;

        ++m_ignoreRoundsForEncounters;
        qWarning() << "Pairing not possible, ignoring round" << m_ignoreRoundsForEncounters
                   << "in pairing history";
        Q_ASSERT(m_ignoreRoundsForEncounters < currentRound());
    }

    // STEP 4 & 5: Perform pairing and assign color
    assignPairs(pairingData, encounters);

    // Finally, record the encounter history
    for (int i = 0; i < gamesPerCycle(); ++i)
        m_encounterHistory[(currentRound() - 1)* gamesPerCycle() + i] = m_pairings[i];
}

TournamentPair* SwissTournament::nextPair(int gameNumber)
{
    qWarning() << "Requesting next pair: game number" << gameNumber;

    if (gameNumber >= finalGameCount())
        return nullptr;

    const int gameInRound = gameNumber % gamesPerRound();

    // note: round 1 pairings are generated on initializePairing()
    if (gameInRound == 0)
    {
        setCurrentRound(1 + (gameNumber / gamesPerRound()));
        generateRoundPairings();
    }

    const QPair<int, int> thePair = getPairForGame(gameNumber);

    // make sure we actually get the correct colors
    TournamentPair *const tpair = pair(thePair.first, thePair.second);
    if (tpair->firstPlayer() != thePair.first)
        tpair->swapPlayers();

    Q_ASSERT(tpair->firstPlayer() == thePair.first);
    Q_ASSERT(tpair->secondPlayer() == thePair.second);

    // handle prerecorded result
    if (m_preRecordedResults.size() > gameNumber)
    {
        const QString &result = m_preRecordedResults[gameNumber];

        qWarning() << "Using prerecorded result" << result << "for pairing";

        if (result == "1-0")
        {
            addScore(thePair.first, 2);
        }
        else if (result == "0-1")
        {
            addScore(thePair.second, 2);
        }
        else if (result == "1/2-1/2")
        {
            addScore(thePair.first, 1);
            addScore(thePair.second, 1);
        }
        else
        {
            qWarning() << "Resume result" << result << "not understood.";

            if (gameNumber != m_preRecordedResults.size() - 1)
            {
                qFatal(
                    "This was not the last game and pairings will break. Not continuing.\n"
                    "Please fix the events JSON file and try again.\n");
            }
        }
    }

    return tpair;
}
