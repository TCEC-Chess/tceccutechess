/*
    This file is part of Cute Chess.

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

#include "chessgame.h"
#include <QThread>
#include <QTimer>
#include <QtMath>
#include <QRegularExpression>
#include "board/board.h"
#include "board/westernboard.h"
#include "chessplayer.h"
#include "openingbook.h"
#include "chessengine.h"
#include "engineoption.h"

#include <jsonserializer.h>
#include <QFileInfo>


QString ChessGame::evalString(const MoveEvaluation& eval, const Chess::Move& move)
{
	if (eval.isEmpty())
		return QString();

#if 1
	QString str;
	if (eval.isBookEval())
		str = "book";
	else {
		// score
		QString sScore;
		int depth = eval.depth();
		if (depth > 0)
		{
			int score = eval.score();
			int absScore = qAbs(score);

			// Detect mate-in-n scores
			if (absScore > 9900
			&&  (absScore = 1000 - (absScore % 1000)) < 100)
			{
				if (score < 0)
					sScore = "-";
				sScore += "M" + QString::number(absScore);
			}
			else
				sScore = QString::number(double(score) / 100.0, 'f', 2);

		} else
			sScore = "0.00";

		str = "d=";
		if (depth <= 0)
			depth = 1;
		str += QString::number(depth);

		// selective depth
		str += ", sd=";
		if (eval.selectiveDepth() > depth)
			str += QString::number(eval.selectiveDepth());
		else
			str += QString::number(depth);

		// pv parser / san converter
		QString sanPv = m_board->sanStringForPv(eval.pv(), Chess::Board::StandardAlgebraic);
		if (sanPv.isEmpty())
		{
			bool lanCheck = true;
			QRegularExpression re;

			sanPv = eval.pv();
			if (sanPv.contains('.'))
			{
				QString probPv(sanPv);
				re.setPattern("\\d+\\.\\h+");	// Remove numbering
				probPv.remove(re);
				re.setPattern("\\.\\.\\.\\h+");	// Remove ellipses
				probPv.remove(re);
				sanPv = m_board->sanStringForPv(probPv, Chess::Board::StandardAlgebraic);
				lanCheck = sanPv.isEmpty();
				if (lanCheck)
					sanPv = probPv;
			}
			re.setPattern("-|x");
			if (lanCheck && sanPv.contains(re))				// LAN notation
			{
				re.setPattern("([NBRQK]?)([a-h][1-8])(-|x)([a-h][1-8])([NBRQ]?)");
				sanPv = sanPv.replace(re, "\\2\\4\\5");
				sanPv = m_board->sanStringForPv(sanPv, Chess::Board::StandardAlgebraic);
			}
		}
		// ponder move 'pd' algebraic move
	#if 0
		QStringList sanList = sanPv.split(' ');
		if (sanList.length() > 1) {
			str+= ", pd=" + sanList[1];
		}
	#else
		if (!eval.ponderMove().isEmpty())
			str+= ", pd=" + eval.ponderMove();
	#endif

	#if 0
		// move time 'mt' "hh:mm:ss"
		int t = eval.time(); // milliseconds
		str += ", mt=";
		if (t == 0)
			str += "00:00:00";
		else {
			int total = qFloor(t / 1000.);
			int hours = qFloor(total / 3600.) % 24; // should be ok, right?
			int minutes = (total / 60) % 60;
			int seconds = total % 60;
			str +=	QString::number(hours).rightJustified(2, '0') + ":" +
					QString::number(minutes).rightJustified(2, '0') + ":" +
					QString::number(seconds).rightJustified(2, '0');
		}

		// time left 'tl' "hh:mm:ss"
		ChessPlayer *player = m_player[m_board->sideToMove()];
		Q_ASSERT(player != 0);

		int tl = player->timeControl()->timeLeft(); // milliseconds
		str += ", tl=";
		if (tl == 0)
			str += "00:00:00";
		else {
			int total = qFloor(tl / 1000.);
			int hours = qFloor(total / 3600.) % 24; // should be ok, right?
			int minutes = (total / 60) % 60;
			int seconds = total % 60;
			str +=	QString::number(hours).rightJustified(2, '0') + ":" +
					QString::number(minutes).rightJustified(2, '0') + ":" +
					QString::number(seconds).rightJustified(2, '0');
		}

		// speed 's' "%d kN/s"
		int nps = eval.nps();
		str += ", s=" + QString::number(qFloor(nps / 1000)) + " kN/s";
	#else
		// move time 'mt'
		str += ", mt=" + QString::number(eval.time());

		// time left 'tl'
		ChessPlayer *player = m_player[m_board->sideToMove()];
		Q_ASSERT(player != 0);
		str += ", tl=" + QString::number(player->timeControl()->timeLeft());

		// speed 's'
		str += ", s=" + QString::number(eval.nps());
	#endif

		// nodes 'n' "%d"
		str += ", n=" + QString::number(eval.nodeCount());

		// pv 'pv' algebraic string
		str += ", pv=" + sanPv;

		// tbhits 'tb'
		str += ", tb=";
		if (eval.tbHits() == MoveEvaluation::NULL_TBHITS)
			str += "null";
		else
			str += QString::number(eval.tbHits());

		// hash usage
		str += ", h=" + QString::number(eval.hashUsage() / 10.0, 'f', 1);

		// ponderhit rate
		str += ", ph=" + QString::number(eval.ponderhitRate() / 10.0, 'f', 1);

		// eval from white's perspective 'wv'
		Chess::Side side = m_board->sideToMove();
		str += ", wv=";
		if (side == Chess::Side::Black && sScore != "0.00") {
			if (sScore[0] == '-')
				str += sScore.right(sScore.length() - 1);
			else
				str += "-" + sScore;
		} else {
			str += sScore;
		}
	}

	m_board->makeMove(move);

	// 50-move clock 'R50'
	str += ", R50=" + QString::number((100 - m_board->reversibleMoveCount()) / 2);

	// draw rule clock 'Rd'
	str += ", Rd=" + QString::number(m_adjudicator.drawClock(m_board, eval));

	// resign rule clock 'Rr'
	str += ", Rr=" + QString::number(m_adjudicator.resignClock(m_board, eval));

	// material balance
	str += statusString(move, false);

	m_board->undoMove();

#else
	if (eval.isBookEval())
		return "book";

	QString str = eval.scoreText();
	if (eval.depth() > 0)
		str += "/" + QString::number(eval.depth()) + " ";

	int t = eval.time();
	if (t == 0)
		return str + "0s";

	int precision = 0;
	if (t < 100)
		precision = 3;
	else if (t < 1000)
		precision = 2;
	else if (t < 10000)
		precision = 1;
	str += QString::number(double(t / 1000.0), 'f', precision) + 's';
#endif
	return str;
}

QString ChessGame::statusString(const Chess::Move& move, bool doMove)
{
	if (doMove)
		m_board->makeMove(move);

	// Material balance 'mb'
	QMap<QString, int> pMap;
	for (int file = 0; file < m_board->height(); file++)
		for (int rank = 0; rank < m_board->width(); rank++)
		{
			const Chess::Square sq = Chess::Square(file, rank);
			const Chess::Piece piece = m_board->pieceAt(sq);
			if (!piece.isValid()) continue;
			const QString sym(m_board->pieceSymbol(piece).toUpper());
			if (!pMap.contains(sym))
				pMap[sym] = 0;
			if (piece.side() == Chess::Side::White)
				++pMap[sym];
			else
				--pMap[sym];
		}

	QString str(", mb=");
	for(const char* istr : {"P", "N", "B", "R", "Q"})
	{
		const int v(pMap[istr]);
		if (v >= 0)
			str += '+';
		str += QString::number(v);
	}

	if (doMove)
		m_board->undoMove();

	str += ',';
	return str;
}

ChessGame::ChessGame(Chess::Board* board, PgnGame* pgn, QObject* parent)
	: QObject(parent),
	  m_board(board),
	  m_startDelay(0),
	  m_finished(false),
	  m_gameInProgress(false),
	  m_paused(false),
	  m_pgnInitialized(false),
	  m_bookOwnership(false),
	  m_boardShouldBeFlipped(false),
	  m_pgn(pgn)
{
	Q_ASSERT(pgn != nullptr);

	for (int i = 0; i < 2; i++)
	{
		m_player[i] = nullptr;
		m_book[i] = nullptr;
		m_bookDepth[i] = 0;
	}
}

ChessGame::~ChessGame()
{
	delete m_board;
	if (m_bookOwnership)
	{
		bool same = (m_book[0] == m_book[1]);
		delete m_book[0];
		if (!same)
			delete m_book[1];
	}
}

QString ChessGame::errorString() const
{
	return m_error;
}

ChessPlayer* ChessGame::player(Chess::Side side) const
{
	Q_ASSERT(!side.isNull());
	return m_player[side];
}

bool ChessGame::isFinished() const
{
	return m_finished;
}

bool ChessGame::boardShouldBeFlipped() const
{
	return m_boardShouldBeFlipped;
}

void ChessGame::setBoardShouldBeFlipped(bool flip)
{
	m_boardShouldBeFlipped = flip;
}

PgnGame* ChessGame::pgn() const
{
	return m_pgn;
}

Chess::Board* ChessGame::board() const
{
	return m_board;
}

QString ChessGame::startingFen() const
{
	return m_startingFen;
}

const QVector<Chess::Move>& ChessGame::moves() const
{
	return m_moves;
}

const QMap<int,int>& ChessGame::scores() const
{
	return m_scores;
}

Chess::Result ChessGame::result() const
{
	return m_result;
}

ChessPlayer* ChessGame::playerToMove() const
{
	if (m_board->sideToMove().isNull())
		return nullptr;
	return m_player[m_board->sideToMove()];
}

ChessPlayer* ChessGame::playerToWait() const
{
	if (m_board->sideToMove().isNull())
		return nullptr;
	return m_player[m_board->sideToMove().opposite()];
}

void ChessGame::stop(bool emitMoveChanged)
{
	if (m_finished)
		return;

	m_finished = true;
	emit humanEnabled(false);
	if (!m_gameInProgress)
	{
		m_result = Chess::Result();
		finish();
		return;
	}
	
	QDateTime gameEndTime = QDateTime::currentDateTimeUtc();

	initializePgn();
	m_gameInProgress = false;
	const QVector<PgnGame::MoveData>& moves(m_pgn->moves());
	int plies = moves.size();

	m_pgn->setTag("PlyCount", QString::number(plies));

	m_pgn->setGameEndTime(gameEndTime);

	m_pgn->setResult(m_result);
	m_pgn->setResultDescription(m_result.description());
	m_pgn->setTag("TerminationDetails", m_result.shortDescription());

	if (emitMoveChanged && plies > 1)
	{
		const PgnGame::MoveData& md(moves.at(plies - 1));
		emit moveChanged(plies - 1, md.move, md.moveString, md.comment);
	}

	m_player[Chess::Side::White]->endGame(m_result);
	m_player[Chess::Side::Black]->endGame(m_result);

	connect(this, SIGNAL(playersReady()), this, SLOT(finish()), Qt::QueuedConnection);
	syncPlayers();
}

void ChessGame::finish()
{
	disconnect(this, SIGNAL(playersReady()), this, SLOT(finish()));
	for (int i = 0; i < 2; i++)
	{
		if (m_player[i] != nullptr)
			m_player[i]->disconnect(this);
	}

	emit finished(this, m_result);
}

void ChessGame::kill()
{
	for (int i = 0; i < 2; i++)
	{
		if (m_player[i] != nullptr)
			m_player[i]->kill();
	}

	stop();
}

void ChessGame::addPgnMove(const Chess::Move& move, const QString& comment)
{
	PgnGame::MoveData md;
	md.key = m_board->key();
	md.move = m_board->genericMove(move);
	md.moveString = m_board->moveString(move, Chess::Board::StandardAlgebraic);
	md.comment = comment;

	m_board->makeMove(move);
	m_pgn->addMove(md, m_board->key());
	m_board->undoMove();
}

void ChessGame::emitLastMove()
{
	emit pgnMove();

	int ply = m_moves.size() - 1;
	if (m_scores.contains(ply))
	{
		int score = m_scores[ply];
		if (score != MoveEvaluation::NULL_SCORE)
			emit scoreChanged(ply, score);
	}

	const auto& md = m_pgn->moves().last();
	emit moveMade(md.move, md.moveString, md.comment);
}

void ChessGame::onMoveMade(const Chess::Move& move)
{
	ChessPlayer* sender = qobject_cast<ChessPlayer*>(QObject::sender());
	Q_ASSERT(sender != nullptr);

	Q_ASSERT(m_gameInProgress);
	Q_ASSERT(m_board->isLegalMove(move));
	if (sender != playerToMove())
	{
		qWarning("%s tried to make a move on the opponent's turn",
			 qUtf8Printable(sender->name()));
		return;
	}

	m_scores[m_moves.size()] = sender->evaluation().score();
	m_moves.append(move);
	addPgnMove(move, evalString(sender->evaluation(), move));

	// Get the result before sending the move to the opponent
	m_board->makeMove(move);
	m_result = m_board->result();
	if (m_result.isNone())
	{
		if (m_board->reversibleMoveCount() == 0)
			m_adjudicator.resetDrawMoveCount();

		m_adjudicator.addEval(m_board, sender->evaluation());
		m_result = m_adjudicator.result();
	}
	m_board->undoMove();

	ChessPlayer* player = playerToWait();
	player->makeMove(move);
	m_board->makeMove(move);

	if (m_result.isNone())
	{
		emitLastMove();
		startTurn();
	}
	else
	{
		stop(false);
		emitLastMove();
	}

	updateLiveFiles();
}

void ChessGame::startTurn()
{
	if (m_paused)
		return;

	Chess::Side side(m_board->sideToMove());
	Q_ASSERT(!side.isNull());

	emit humanEnabled(m_player[side]->isHuman());

	Chess::Move move(bookMove(side));
	if (move.isNull())
	{
		m_player[side]->go();
		m_player[side.opposite()]->startPondering();
	}
	else
	{
		m_player[side.opposite()]->clearPonderState();
		m_player[side]->makeBookMove(move);
	}
}

void ChessGame::onAdjudication(const Chess::Result& result)
{
	if (m_finished || result.type() != Chess::Result::Adjudication)
		return;

	m_result = result;

	stop();
}

void ChessGame::onResignation(const Chess::Result& result)
{
	if (m_finished || result.type() != Chess::Result::Resignation)
		return;

	m_result = result;

	stop();
}

void ChessGame::onResultClaim(const Chess::Result& result)
{
	if (m_finished)
		return;

	ChessPlayer* sender = qobject_cast<ChessPlayer*>(QObject::sender());
	Q_ASSERT(sender != nullptr);

	if (result.type() == Chess::Result::Disconnection)
	{
		// The engine may not be properly started so we have to
		// figure out the player's side this way
		Chess::Side side(Chess::Side::White);
		if (m_player[side] != sender)
			side = Chess::Side::Black;
		m_result = Chess::Result(result.type(), side.opposite());
	}
	else if (!m_gameInProgress && result.winner().isNull())
	{
		qWarning("Unexpected result claim from %s: %s",
			 qUtf8Printable(sender->name()),
			 qUtf8Printable(result.toVerboseString()));
	}
	else if (sender->areClaimsValidated() && result.loser() != sender->side())
	{
		qWarning("%s forfeits by invalid result claim: %s",
			 qUtf8Printable(sender->name()),
			 qUtf8Printable(result.toVerboseString()));
		m_result = Chess::Result(Chess::Result::Adjudication,
					 sender->side().opposite(),
					 "Invalid result claim");
	}
	else
		m_result = result;

	stop();
}

Chess::Move ChessGame::bookMove(Chess::Side side)
{
	Q_ASSERT(!side.isNull());

	if (m_book[side] == nullptr
	||  m_moves.size() >= m_bookDepth[side] * 2)
		return Chess::Move();

	Chess::GenericMove bookMove = m_book[side]->move(m_board->key());
	Chess::Move move = m_board->moveFromGenericMove(bookMove);
	if (move.isNull())
		return Chess::Move();

	if (!m_board->isLegalMove(move))
	{
		qWarning("Illegal opening book move for %s: %s",
			 qUtf8Printable(side.toString()),
			 qUtf8Printable(m_board->moveString(move, Chess::Board::LongAlgebraic)));
		return Chess::Move();
	}

	if (m_board->isRepetition(move))
		return Chess::Move();

	return move;
}

void ChessGame::setError(const QString& message)
{
	m_error = message;
}

void ChessGame::setPlayer(Chess::Side side, ChessPlayer* player)
{
	Q_ASSERT(!side.isNull());
	Q_ASSERT(player != nullptr);
	m_player[side] = player;
}

void ChessGame::setStartingFen(const QString& fen)
{
	Q_ASSERT(!m_gameInProgress);
	m_startingFen = fen;
}

void ChessGame::setTimeControl(const TimeControl& timeControl, Chess::Side side)
{
	if (side != Chess::Side::White)
		m_timeControl[Chess::Side::Black] = timeControl;
	if (side != Chess::Side::Black)
		m_timeControl[Chess::Side::White] = timeControl;
}

void ChessGame::setMoves(const QVector<Chess::Move>& moves)
{
	Q_ASSERT(!m_gameInProgress);
	m_scores.clear();
	m_moves = moves;
}

bool ChessGame::setMoves(const PgnGame& pgn)
{
	setStartingFen(pgn.startingFenString());
	if (!resetBoard())
		return false;
	m_scores.clear();
	m_moves.clear();

	for (const PgnGame::MoveData& md : pgn.moves())
	{
		Chess::Move move(m_board->moveFromGenericMove(md.move));
		if (!m_board->isLegalMove(move))
			return false;

		m_board->makeMove(move);
		if (!m_board->result().isNone())
			return true;

		m_moves.append(move);
	}

	return true;
}

void ChessGame::setOpeningBook(const OpeningBook* book,
			       Chess::Side side,
			       int depth)
{
	Q_ASSERT(!m_gameInProgress);

	if (side.isNull())
	{
		setOpeningBook(book, Chess::Side::White, depth);
		setOpeningBook(book, Chess::Side::Black, depth);
	}
	else
	{
		m_book[side] = book;
		m_bookDepth[side] = depth;
	}
}

void ChessGame::setAdjudicator(const GameAdjudicator& adjudicator)
{
	m_adjudicator = adjudicator;
}

void ChessGame::generateOpening()
{
	if (m_book[Chess::Side::White] == nullptr || m_book[Chess::Side::Black] == nullptr)
		return;
	if (!resetBoard())
		return;

	// First play moves that are already in the opening
	// TODO: use qAsConst() from Qt 5.7
	for (const Chess::Move& move : qAsConst(m_moves))
	{
		Q_ASSERT(m_board->isLegalMove(move));

		m_board->makeMove(move);
		if (!m_board->result().isNone())
			return;
	}

	// Then play the opening book moves
	for (;;)
	{
		Chess::Move move = bookMove(m_board->sideToMove());
		if (move.isNull())
			break;

		m_board->makeMove(move);
		if (!m_board->result().isNone())
			break;

		m_moves.append(move);
	}
}

void ChessGame::emitStartFailed()
{
	emit startFailed(this);
}

void ChessGame::setStartDelay(int time)
{
	Q_ASSERT(time >= 0);
	m_startDelay = time;
}

void ChessGame::setBookOwnership(bool enabled)
{
	m_bookOwnership = enabled;
}

void ChessGame::setLiveOutput(const QString &livePgnOut, PgnGame::PgnMode livePgnOutMode,
			      bool pgnFormat, bool jsonFormat)
{
	m_livePgnOut = livePgnOut;
	m_livePgnOutMode = livePgnOutMode;
	m_pgnFormat = pgnFormat;
	m_jsonFormat = jsonFormat;
}

void ChessGame::pauseThread()
{
	m_pauseSem.release();
	m_resumeSem.acquire();
}

void ChessGame::lockThread()
{
	if (QThread::currentThread() == thread())
		return;

	QMetaObject::invokeMethod(this, "pauseThread", Qt::QueuedConnection);
	m_pauseSem.acquire();
}

void ChessGame::unlockThread()
{
	if (QThread::currentThread() == thread())
		return;

	m_resumeSem.release();
}

bool ChessGame::resetBoard()
{
	QString fen(m_startingFen);
	if (fen.isEmpty())
	{
		fen = m_board->defaultFenString();
		if (m_board->isRandomVariant())
			m_startingFen = fen;
	}

	if (!m_board->setFenString(fen))
	{
		qWarning("Invalid FEN string: %s", qUtf8Printable(fen));
		m_board->reset();
		if (m_board->isRandomVariant())
			m_startingFen = m_board->fenString();
		else
			m_startingFen.clear();
		return false;
	}
	else if (!m_startingFen.isEmpty())
		m_startingFen = m_board->fenString();

	return true;
}

void ChessGame::onPlayerReady()
{
	ChessPlayer* sender = qobject_cast<ChessPlayer*>(QObject::sender());
	Q_ASSERT(sender != nullptr);

	disconnect(sender, SIGNAL(ready()),
		   this, SLOT(onPlayerReady()));
	disconnect(sender, SIGNAL(disconnected()),
		   this, SLOT(onPlayerReady()));

	for (int i = 0; i < 2; i++)
	{
		if (!m_player[i]->isReady()
		&&  m_player[i]->state() != ChessPlayer::Disconnected)
			return;
	}

	emit playersReady();
}

void ChessGame::syncPlayers()
{
	bool ready = true;

	for (int i = 0; i < 2; i++)
	{
		ChessPlayer* player = m_player[i];
		Q_ASSERT(player != nullptr);

		if (!player->isReady()
		&&  player->state() != ChessPlayer::Disconnected)
		{
			ready = false;
			connect(player, SIGNAL(ready()),
				this, SLOT(onPlayerReady()));
			connect(player, SIGNAL(disconnected()),
				this, SLOT(onPlayerReady()));
		}
	}
	if (ready)
		emit playersReady();
}

void ChessGame::start()
{
	if (m_startDelay > 0)
	{
		QTimer::singleShot(m_startDelay, this, SLOT(start()));
		m_startDelay = 0;
		return;
	}

	for (int i = 0; i < 2; i++)
	{
		connect(m_player[i], SIGNAL(resultClaim(Chess::Result)),
			this, SLOT(onResultClaim(Chess::Result)));
	}

	// Start the game in the correct thread
	connect(this, SIGNAL(playersReady()), this, SLOT(startGame()));
	QMetaObject::invokeMethod(this, "syncPlayers", Qt::QueuedConnection);


	m_result = Chess::Result();
	emit humanEnabled(false);
	resetBoard();
	initializePgn();
	emit initialized(this);
	emit fenChanged(m_board->startingFenString());
}

void ChessGame::pause()
{
	m_paused = true;
}

void ChessGame::resume()
{
	if (!m_paused)
		return;
	m_paused = false;

	QMetaObject::invokeMethod(this, "startTurn", Qt::QueuedConnection);
}

void ChessGame::initializePgn()
{
	if (m_pgnInitialized)
		return;
	m_pgnInitialized = true;

	m_pgn->setVariant(m_board->variant());
	m_pgn->setStartingFenString(m_board->startingSide(), m_startingFen);
	m_pgn->setDate(QDate::currentDate());
	m_pgn->setPlayerName(Chess::Side::White, m_player[Chess::Side::White]->name());
	m_pgn->setPlayerName(Chess::Side::Black, m_player[Chess::Side::Black]->name());
	m_pgn->setPlayerRating(Chess::Side::White, m_player[Chess::Side::White]->rating());
	m_pgn->setPlayerRating(Chess::Side::Black, m_player[Chess::Side::Black]->rating());
	m_pgn->setResult(m_result);

	if (m_timeControl[Chess::Side::White] == m_timeControl[Chess::Side::Black])
		m_pgn->setTag("TimeControl", m_timeControl[0].toString());
	else
	{
		m_pgn->setTag("WhiteTimeControl", m_timeControl[Chess::Side::White].toString());
		m_pgn->setTag("BlackTimeControl", m_timeControl[Chess::Side::Black].toString());
	}

	// this is a hack, but it works
	QString engineOptions;
	if (!m_player[Chess::Side::White]->isHuman()) {
		ChessEngine *engine = dynamic_cast<ChessEngine *>(m_player[Chess::Side::White]);
		if (engine) {
			engineOptions += QString("WhiteEngineOptions: %1").arg(engine->configurationString());
		}
	}

	if (!m_player[Chess::Side::Black]->isHuman()) {
		ChessEngine *engine = dynamic_cast<ChessEngine *>(m_player[Chess::Side::Black]);
		if (engine) {
			if (!engineOptions.isEmpty()) engineOptions += ", ";
			engineOptions += QString("BlackEngineOptions: %1").arg(engine->configurationString());
		}
	}
//	m_pgn->setGameComment(engineOptions);
	m_pgn->setResultDescription(engineOptions);
}

void ChessGame::startGame()
{
	disconnect(this, SIGNAL(playersReady()), this, SLOT(startGame()));
	if (m_finished)
		return;

	m_gameInProgress = true;
	for (int i = 0; i < 2; i++)
	{
		ChessPlayer* player = m_player[i];
		Q_ASSERT(player != nullptr);
		Q_ASSERT(player->isReady());

		if (player->state() == ChessPlayer::Disconnected)
		{
			setError(tr("Could not initialize player %1: %2")
			         .arg(player->name()).arg(player->errorString()));
			m_result = Chess::Result(Chess::Result::ResultError);
			stop();
			emitStartFailed();
			return;
		}
		if (!player->supportsVariant(m_board->variant()))
		{
			qWarning("%s doesn't support variant %s",
				 qUtf8Printable(player->name()),
				 qUtf8Printable(m_board->variant()));
			m_result = Chess::Result(Chess::Result::ResultError);
			stop();
			return;
		}
	}

	m_pgn->setPlayerName(Chess::Side::White, m_player[Chess::Side::White]->name());
	m_pgn->setPlayerName(Chess::Side::Black, m_player[Chess::Side::Black]->name());

	emit started(this);
	QDateTime gameStartTime = QDateTime::currentDateTimeUtc();
	m_pgn->setGameStartTime(gameStartTime);

	for (int i = 0; i < 2; i++)
	{
		Chess::Side side = Chess::Side::Type(i);

		Q_ASSERT(m_timeControl[side].isValid());
		m_player[side]->setTimeControl(m_timeControl[side]);
		m_player[side]->newGame(side, m_player[side.opposite()], m_board);
	}

	// Play the forced opening moves first
	for (int i = 0; i < m_moves.size(); i++)
	{
		Chess::Move move(m_moves.at(i));
		Q_ASSERT(m_board->isLegalMove(move));
		
		addPgnMove(move, "book" + statusString(move, true));

		playerToMove()->makeBookMove(move);
		playerToWait()->makeMove(move);
		m_board->makeMove(move);
		
		emitLastMove();

		if (!m_board->result().isNone())
		{
			qWarning("Every move was played from the book");
			m_result = m_board->result();
			stop();
			return;
		}
	}

	updateLiveFiles();

	for (int i = 0; i < 2; i++)
	{
		connect(m_player[i], SIGNAL(moveMade(Chess::Move)),
			this, SLOT(onMoveMade(Chess::Move)));
		if (m_player[i]->isHuman())
			connect(m_player[i], SIGNAL(wokeUp()),
				this, SLOT(resume()));
	}
	
	startTurn();
}

void ChessGame::updateLiveFiles() const
{
	if (m_livePgnOut.isEmpty()) return;

	const PgnGame* const pgn = m_pgn;

	if (m_pgnFormat)
	{
		const QString fileName(m_livePgnOut + ".pgn");
		QFile::resize(fileName, 0);
		pgn->write(fileName, m_livePgnOutMode);
	}

	if (m_jsonFormat)
	{
		Chess::Board* board = m_board->copy();
		board->setFenString(board->startingFenString());

		QVariantMap pMap;

		// Parse and assemble engine options
		QStringList engines = pgn->initialComment().split(',', QString::SkipEmptyParts);
		for (QString& engine : engines)
		{
			engine = engine.trimmed();
			const int ePos = engine.indexOf(':');
			if (ePos > 0)
			{
				QVariantList oList;
				QStringList options = engine.mid(ePos + 1).trimmed().split(';', QString::SkipEmptyParts);
				for (QString& option : options)
				{
					option = option.trimmed();
					QVariantMap oMap;
					const int oPos = option.indexOf('=');
					if(oPos > 0)
					{
						oMap["Name"] = option.left(oPos).trimmed();
						oMap["Value"] = option.mid(oPos + 1).trimmed();
					} else
						oMap["Name"] = option;
					oList << oMap;
				}
				pMap[engine.left(ePos).trimmed()] = oList;
			}
		}

		// Assemble tags
		const QList< QPair<QString, QString> >& tags = pgn->tags();
		QVariantMap hMap;
		for(const QPair<QString, QString>& tagPair : tags)
			hMap[tagPair.first] = tagPair.second;
		pMap["Headers"] = hMap;

		// Parse and assemble move stats
		const QVector<PgnGame::MoveData>& moves = pgn->moves();
		QVariantList mList;
		for (const PgnGame::MoveData& move : moves)
		{
			QVariantMap mMap;
			QVariantMap aMap;

			mMap["m"] = move.moveString;

			QString sq(static_cast<char>(move.move.sourceSquare().file() + 'a'));
			sq += static_cast<char>(move.move.sourceSquare().rank() + '1');
			mMap["from"] = sq;

			sq = static_cast<char>(move.move.targetSquare().file() + 'a');
			sq += static_cast<char>(move.move.targetSquare().rank() + '1');
			mMap["to"] = sq;

			mMap["book"] = false;

			QStringList stats = move.comment.split(',', QString::SkipEmptyParts);
			for(QString& stat : stats)
			{
				stat = stat.trimmed();
				if (stat == "book") {
					mMap["book"] = true;
				} else {
					const int pos = stat.indexOf('=');
					if (pos > 0)
					{
						const QString name(stat.left(pos).trimmed());
						const QString value(stat.mid(pos + 1).trimmed());
						if (name == "pv")
						{
							QVariantMap pvMap;
							QVariantList pvList;

							pvMap["San"] = value;

							int pvmCnt = 0;
							QStringList pvMoves = value.split(' ', QString::SkipEmptyParts);
							for (const QString& pvMoveStr : pvMoves)
							{
								QVariantMap pvMove;

								const Chess::Move& pvbm(board->moveFromString(pvMoveStr));
								if (pvbm.isNull())
									break;
								const Chess::GenericMove& gm(board->genericMove(pvbm));

								board->makeMove(pvbm);
								++pvmCnt;

								pvMove["m"] = pvMoveStr;
								pvMove["fen"] = board->fenString();

								sq = static_cast<char>(gm.sourceSquare().file() + 'a');
								sq += static_cast<char>(gm.sourceSquare().rank() + '1');
								pvMove["from"] = sq;

								sq = static_cast<char>(gm.targetSquare().file() + 'a');
								sq += static_cast<char>(gm.targetSquare().rank() + '1');
								pvMove["to"] = sq;

								pvList << pvMove;
							}
							for(; pvmCnt > 0; --pvmCnt)
								board->undoMove();

							pvMap["Moves"] = pvList;
							mMap["pv"] = pvMap;
						}
						else if (name == "mb")
						{
							QVariantMap mbMap;
							int idx = 0;
							for (const char* mstr : {"p", "n", "b", "r", "q"})
							{
								mbMap[mstr] = value.mid(idx, 2).toInt();
								idx += 2;
							}
							mMap["material"] = mbMap;
						}
						else if (name == "R50")
							aMap["FiftyMoves"] = value.toInt();
						else if (name == "Rd")
							aMap["Draw"] = value.toInt();
						else if (name == "Rr")
							aMap["ResignOrWin"] = value.toInt();
						else
							mMap[name] = value;
					}
					else	// real comment
						mMap["rem"] = stat;
				}
			}
			if (!aMap.empty())
				mMap["adjudication"] = aMap;

			board->makeMove(board->moveFromGenericMove(move.move));

			mMap["fen"] = board->fenString();

			mList << mMap;
		}
		pMap["Moves"] = mList;

		delete board;

		const QString tempName(m_livePgnOut + "_temp.json");
		const QString finalName(m_livePgnOut + ".json");
		if (QFile::exists(tempName))
			QFile::remove(tempName);
		QFile output(tempName);
		if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qWarning("cannot open live JSON output file: %s", qUtf8Printable(tempName));
		} else {
			QTextStream out(&output);
			JsonSerializer serializer(pMap);
			serializer.serialize(out);
			output.close();
			if (QFile::exists(finalName))
				QFile::remove(finalName);
			if (!QFile::rename(tempName, finalName))
				qWarning("cannot rename live JSON output file: %s to %s", qUtf8Printable(tempName), qUtf8Printable(finalName));
		}
	}
}
