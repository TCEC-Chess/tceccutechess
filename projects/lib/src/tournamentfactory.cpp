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

#include "tournamentfactory.h"
#include "roundrobintournament.h"
#include "gauntlettournament.h"
#include "knockouttournament.h"
#include "pyramidtournament.h"
#include "swisstournament.h"

Tournament* TournamentFactory::create(const QString& type,
				      GameManager* gameManager,
					  EngineManager* engineManager,
				      QObject* parent)
{
	if (type == "round-robin")
		return new RoundRobinTournament(gameManager, engineManager, parent);
	if (type == "gauntlet")
		return new GauntletTournament(gameManager, engineManager, parent);
	if (type == "knockout")
		return new KnockoutTournament(gameManager, engineManager, parent);
	if (type == "pyramid")
		return new PyramidTournament(gameManager, engineManager, parent);
	if (type == "swiss-tcec")
            return new SwissTournament(gameManager, engineManager, parent);

	return nullptr;
}
