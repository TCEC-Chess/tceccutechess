/*
    This file is part of Cute Chess.
    Copyright (C) 2008-2018 Cute Chess authors

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

#include "knockouttournament.h"
#include <QStringList>
#include <QtMath>
#include "playerbuilder.h"
#include "mersenne.h"
#include "board/boardfactory.h"
#include <chessgame.h>

bool m_should_we_stop_global = false;

KnockoutTournament::KnockoutTournament(GameManager* gameManager,
					   EngineManager* engineManager,
				       QObject *parent)
	: Tournament(gameManager, engineManager, parent)
{
}

QString KnockoutTournament::type() const
{
	return "knockout";
}

bool KnockoutTournament::canSetRoundMultiplier() const
{
	return false;
}

int KnockoutTournament::playerSeed(int rank, int bracketSize)
{
	if (rank <= 1)
		return 0;

	// If our rank is even we need to put the player into the right bracket
	// so we add half the bracket size to his position and make a recursive
	// call with half the rank and half the bracket size.
	if (rank % 2 == 0)
		return bracketSize / 2 + playerSeed(rank / 2, bracketSize / 2);

	// If the rank is uneven, we put the player in the left bracket.
	// Since rank is uneven we need to add + 1 so that it stays uneven.
	return playerSeed(rank / 2 + 1, bracketSize / 2);
}

QList<int> KnockoutTournament::firstRoundPlayers() const
{
	QList<int> players;
	int n(playerCount());
	int seedCount = qMin(this->seedCount(), n);

	for (int i = 0; i < seedCount; i++)
		players << i;

	QList<int> unseeded;
	for (int i = seedCount; i < n; i++)
	{
		unseeded << i;
		players << i;
	}

/*
	while (!unseeded.isEmpty())
	{
		int i = Mersenne::random() % unseeded.size();
	}
*/
	return players;
}

void KnockoutTournament::initializePairing()
{
	int x = 1;
	while (x < playerCount())
		x *= 2;
	QVector<int> all = QVector<int>(x, -1);

	QList<int> players(firstRoundPlayers());
	int byeCount = x - players.size();
	for (int i = 0; i < byeCount; i++)
		players << -1;

	int byes = 0;
	for (int i = 0; i < players.size(); i++)
	{
		int index = playerSeed(i + 1, x);
		int player(players.at(i));

		// Pair BYEs with top-seeded players
		if (player == -1)
		{
			int byeIndex = playerSeed(++byes, x) + 1;
			all[index] = all.at(byeIndex);
			all[byeIndex] = player;
		}
		else
			all[i] = player;
	}

	QList<TournamentPair*> pairs;
	int j = 0;
	for (int i = 0; i < x; i += 2)
	{
		pairs.append(pair(all.at(j), all.at(x -j - 1)));
		j++;
	}

	m_rounds.clear();
	m_rounds << pairs;
}

int KnockoutTournament::gamesPerCycle() const
{
	int x = qNextPowerOfTwo(playerCount() - 1);
	int round = x / 2;
	int total = round - (x - playerCount());
	while (round >= 2)
	{
		round /= 2;
		total += round;
	}

	return total;
}

void KnockoutTournament::addScore(int player, int score)
{
	for (TournamentPair* pair : m_rounds.last())
	{
		if (score > 0)
		{
			if (pair->firstPlayer() == player)
			{
				pair->addFirstScore(score);
				break;
			}
			if (pair->secondPlayer() == player)
			{
				pair->addSecondScore(score);
				break;
			}
		}
	}

	Tournament::addScore(player, score);
}

QList<int> KnockoutTournament::lastRoundWinners() const
{
	QList<int> winners;
	const auto last = m_rounds.last();
	for (const TournamentPair* pair : last)
		winners << pair->leader();

	return winners;
}

bool KnockoutTournament::areAllGamesFinished() const
{
	const QList<TournamentPair*>& lastRound(m_rounds.last());

	qWarning () << "lastRound.size:" << lastRound.size() << " , m_should_we_stop_global" << m_should_we_stop_global;
	const auto last = m_rounds.last();
	for (TournamentPair* pair : last)
	{
		if (needMoreGames(pair))
			return false;
	}

	return true;
}

void KnockoutTournament::setTC(TournamentPlayer white, TournamentPlayer black, ChessGame * game, const TournamentPair* pair)
{
	TimeControl wTimeControl = white.timeControl();
	TimeControl bTimeControl = black.timeControl();

	if (!pair)
	{
		game->setTimeControl(wTimeControl, Chess::Side::White);
		game->setTimeControl(bTimeControl, Chess::Side::Black);
		return;
	}

	int firstScore  = pair->firstScore() + white.builder()->resumescore();
	int secondScore = pair->secondScore() + black.builder()->resumescore();

	qWarning() << "ARUN: Holy:" << white.builder()->resumescore();

	game->setTimeControl(wTimeControl, Chess::Side::White);
	game->setTimeControl(bTimeControl, Chess::Side::Black);
}

bool KnockoutTournament::shouldWeStopTour() const
{
	QString path = "failed.txt";

	if (!fileExists(path))
	{
		return areAllGamesFinished();
	}
	else
	{
		qWarning () << " \n *************************************************************** \n" << 
						"         We stopped before game#::" <<
						"         Look at failed.txt" <<
						" \n *************************************************************** \n";
		return true;
	}
}

bool KnockoutTournament::shouldWeStop(int iWhite, int iBlack, const TournamentPair* pair) const
{
	int firstScore  = pair->firstScore() + Tournament::playerAt(iWhite).builder()->resumescore();
	int secondScore = pair->secondScore() + Tournament::playerAt(iBlack).builder()->resumescore();
	int leadScore = qMax(firstScore, secondScore);
	int pointsInProgress = pair->gamesInProgress() * 2;

	if (leadScore <= gamesPerEncounter())
	{
		m_should_we_stop_global = false;
	}
	else if (firstScore == secondScore)
	{
		m_should_we_stop_global = false;	
	}

	if (!m_should_we_stop_global)
	{
		if (((Tournament::playerAt(iWhite).builder()->strikes() + Tournament::playerAt(iWhite).crashes()) 
				>= Tournament::strikes() ||
			 (Tournament::playerAt(iBlack).builder()->strikes() + Tournament::playerAt(iWhite).crashes()) 
				>= Tournament::strikes()))
		{
			m_should_we_stop_global = true;
			return true;
		}
		else
		{
			return false;
		}
	}
	m_should_we_stop_global = true;
	return true;
}

bool KnockoutTournament::procceedNextGame() const
{
}

bool KnockoutTournament::resetBook(const TournamentPair* pair) const
{
	// Second player is a BYE
	if (!pair->isValid())
		return false;

	const int iWhite = pair->firstPlayer();
	const int iBlack = pair->secondPlayer();
	int firstScore  = pair->firstScore() + Tournament::playerAt(iWhite).builder()->resumescore();
	int secondScore = pair->secondScore() + Tournament::playerAt(iBlack).builder()->resumescore();

	if ((firstScore == secondScore) && (!firstScore))
	{
		return true;
	}
	return false;
}

bool KnockoutTournament::needMoreGames(const TournamentPair* pair) const
{
	// Second player is a BYE
	if (!pair->isValid())
		return false;

	// Assign the leading player all points from ongoing games.
	// If the leader still doesn't have enough points to win the
	// encounter, return true
	const int iWhite = pair->firstPlayer();
	const int iBlack = pair->secondPlayer();

	int firstScore  = pair->firstScore() + Tournament::playerAt(iWhite).builder()->resumescore();
	int secondScore = pair->secondScore() + Tournament::playerAt(iBlack).builder()->resumescore();
	int leadScore = qMax(firstScore, secondScore);
	int pointsInProgress = pair->gamesInProgress() * 2;
	//leadScore += pointsInProgress;

	if (shouldWeStop(iWhite, iBlack, pair))
	{
		return false;
	}

	if (leadScore <= gamesPerEncounter())
		return true;

	// Make sure the score diff is big enough
	int minDiff = 1;

	if ((firstScore + secondScore) % 4 == 0)
		minDiff = 2;
	else
		minDiff = 3;

	int maxDiff = qAbs(firstScore - secondScore);
	qWarning () << "firstScore:" << firstScore << " ,secondScore:" << secondScore << " ,minDiff:" << minDiff;
	if (maxDiff >= minDiff)
	{
		return false;
	}

	if (minDiff == 1 || maxDiff == 0)
	{
		return true;
	}

	// If the encounter was extended, make sure there's an even
	// number of games if gamesPerEncounter() is even
	qWarning () << "need more games: true";
	return true;
}

TournamentPair* KnockoutTournament::nextPair(int gameNumber)
{
	Q_UNUSED(gameNumber);

	const auto last = m_rounds.last();
	for (TournamentPair* pair : last)
	{
		if (needMoreGames(pair))
			return pair;
	}

	QList<int> winners(lastRoundWinners());
	if (winners.size() <= 1 || gamesInProgress())
		return nullptr;

	QList<TournamentPair*> nextRound;
	for (int i = 0; i < winners.size(); i += 2)
	{
		nextRound << pair(winners.at(i), winners.at(i + 1));
	}
	m_rounds << nextRound;
	if (currentRound() == 1)
	{
		return nullptr;
	}
	setCurrentRound(currentRound() + 1);

	for (TournamentPair* pair : qAsConst(nextRound))
	{
		if (pair->isValid())
			return pair;
	}

	Q_UNREACHABLE();
	return nullptr;
}

QString KnockoutTournament::results() const
{
	QStringList lines;

	const auto first = m_rounds.first();
	for (const TournamentPair* pair : first)
	{
		int player1 = pair->firstPlayer();
		int player2 = pair->secondPlayer();
		if (!pair->hasOriginalOrder())
			std::swap(player1, player2);

		lines << playerAt(player1).name() << "";
		if (!pair->isValid())
			lines << "bye";
		else
			lines << playerAt(player2).name();
		lines << "";
	}
	lines.removeLast();

	for (int round = 0; round < currentRound(); round++)
	{
		int x = 0;
		const auto nthRound = m_rounds.at(round);
		for (const TournamentPair* pair : nthRound)
		{
			QString winner;
			if (needMoreGames(pair) || pair->gamesInProgress())
				winner = "...";
			else
			{
				int whoWon = pair->leader();
				if (whoWon < 0)
				{
					const int iWhite = pair->firstPlayer();
					const int iBlack = pair->secondPlayer();

					if (Tournament::playerAt(iWhite).builder()->strikes() > 
						Tournament::playerAt(iBlack).builder()->strikes())
					{
						whoWon = iBlack;
					}
					else
					{
						whoWon = iWhite;
					}
					winner = playerAt(whoWon).name();
				}
			}
			int r = round + 1;
			int lineNum = ((2 << (r - 1)) - 1) + (x * (2 << r));
			QString text = QString("%1 Winner %2")
				       .arg(QString(r * 2, '\t'), winner);
			if (pair->scoreSum())
			{
				int score1 = pair->firstScore();
				int score2 = pair->secondScore();
				if (!pair->hasOriginalOrder())
					std::swap(score1, score2);

				text += QString(" (%1-%2)")
					.arg(double(score1) / 2)
					.arg(double(score2) / 2);
			}
			lines[lineNum] += text;
			x++;
		}
	}

	return lines.join('\n');
}

int KnockoutTournament::gamesPerRound() const
{
	// TODO: Implement for TCEC
	return 0;
}

QList< QPair<QString, QString> > KnockoutTournament::getPairings()
{
	// TODO: Implement knockout for TCEC
	QList< QPair<QString, QString> > pList;
	return pList;
}
