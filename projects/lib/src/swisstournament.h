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


#ifndef SWISSTOURNAMENT_H
#define SWISSTOURNAMENT_H

#include "tournament.h"

/*!
 * \brief Round-robin type chess tournament.
 *
 * In a Round-robin tournament each player meets all
 * other contestants in turn.
 */
class LIB_EXPORT SwissTournament : public Tournament
{
	Q_OBJECT

	public:

		/*! Creates a new TCEC Swiss tournament. */
		explicit SwissTournament(GameManager* gameManager,
                                         EngineManager* engineManager,
                                         QObject *parent);
		// Inherited from Tournament
		virtual QString type() const;
		virtual QList< QPair<QString, QString> > getPairings();

		virtual void addResumeGameResult(int gameNumber, const QString &result) override;

	protected:
		// Inherited from Tournament
		virtual void initializePairing();
		virtual int gamesPerCycle() const;
                virtual int gamesPerRound() const;
		virtual TournamentPair* nextPair(int gameNumber);

	private:
                struct PlayerStats
                {
                    int whiteGameDiff;    // white game increases; black game decreases
                    bool byeReceived;
                };

                struct PairingData
                {
                    int playerIndex;
                    int score;
                    bool paired;

                    // order: score (DESC); playerIndex (ASC)
                    bool operator < (const PairingData &other) const
                    {
                        if (score > other.score)
                            return true;

                        if (score < other.score)
                            return false;

                        if (playerIndex < other.playerIndex)
                            return true;

                        return false;
                    }
                };

                class EncountersTable
                {
                private:
                    std::vector<bool> m_encounters;
                    const int m_numPlayers;

                public:
                    EncountersTable(int numPlayers);

                    void clear();
                    void addEncounter(int player1, int player2);
                    bool hasMet(int player1, int player2) const;
                };

                // Prerecorded results from a resumed tournament
                QList<QString> m_preRecordedResults;

                // additional player stats needed for pairing
                QVector<PlayerStats> m_playerStats;

                // Round schedule (white vs black)
                // colors alternated between encounters
                //
                // size: gamesPerCycle()
                // colors alternated between encounters
                QVector<QPair<int, int> > m_pairings;

                // encounter history
                // one encounter per round; on double rounds, the second
                // encounter is with reverse colors
                QVector<QPair<int, int> > m_encounterHistory;

                // number of rounds ignored when building the encounters set
                int m_ignoreRoundsForEncounters;

                // try adding a new pairing and check whether the round pairing
                // can still be completed
                //
                // playerIndex1=-1 and playerIndex2=-1 can be used to determine
                // whether the full round can be paired
                bool tryPairing(const QVector<PairingData> &pairingData, int playerIndex1, int playerIndex2,
                                const EncountersTable &encounters) const;

                // rebuild the encounters lookup set. Encounter history of
                // previous rounds is used (except the rounds that are ignored)
                void rebuildEncountersSet(EncountersTable &encounters) const;

                // STEP 1: generate pairing order
                void generatePairingOrder(QVector<PairingData> &pairingData) const;

                // STEP 2: assign BYE and add scores (BYE=win) if necessary
                void assignByeIfNecessary(QVector<PairingData> &pairingData);

                // STEP 4: perform pairing with colors
                void assignPairs(QVector<PairingData> &pairingData, EncountersTable &encounters);

                // STEP 5: determine color
                bool determineColorIsFirstWhite(int firstPlayer, const PlayerStats &firstStats,
                                                int secondPlayer, const PlayerStats &secondStats) const;

                // generate pairings for the current round
                void generateRoundPairings();

                // determine pair for game (first = white). If the schedule is
                // not generated, then 0,0 will be returned.
                //
                // game number starts from 0
                QPair<int, int> getPairForGame(int gameNumber) const;
};

#endif // SWISSTOURNAMENT_H
