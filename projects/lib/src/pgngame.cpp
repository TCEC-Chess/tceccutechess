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

#include "pgngame.h"
#include <QStringList>
#include <QFile>
#include <QMetaObject>
#include <QDateTime>
#include "board/boardfactory.h"
#include "econode.h"
#include "pgnstream.h"

namespace {

void writeTag(QTextStream& out, const QString& tag, const QString& value)
{
	if (!value.isEmpty())
		out << "[" << tag << " \"" << value << "\"]\n";
	else
		out << "[" << tag << " \"?\"]\n";
}

} // anonymous namespace

PgnStream& operator>>(PgnStream& in, PgnGame& game)
{
	game.read(in);
	return in;
}

QTextStream& operator<<(QTextStream& out, PgnGame& game)
{
	game.write(out);
	return out;
}


PgnGame::PgnGame()
	: m_startingSide(Chess::Side::White),
	  m_tagReceiver(nullptr),
	  m_key(0)
{
}

bool PgnGame::isNull() const
{
	return (m_tags.isEmpty() && m_moves.isEmpty());
}

void PgnGame::clear()
{
	m_startingSide = Chess::Side();
	m_tags.clear();
	m_moves.clear();

    resetCursor();
}

/**
 * This will make sure the next write is a full PGN write
 */
void PgnGame::resetCursor() {
    m_tag_changed = 1;
    m_last_move = 0;
    m_last_result.clear();
}

QList< QPair<QString, QString> > PgnGame::tags() const
{
	QList< QPair<QString, QString> > list;

	// The seven tag roster
	const QStringList roster = (QStringList() << "Event" << "Site" << "Date"
		<< "Round" << "White" << "Black" << "Result");
	for (const QString& tag : roster)
	{
		QString value = m_tags.value(tag);
		if (value.isEmpty())
			value = "?";
		list.append(qMakePair(tag, value));
	}

	QMap<QString, QString>::const_iterator it;
	for (it = m_tags.constBegin(); it != m_tags.constEnd(); ++it)
	{
		if (!roster.contains(it.key()) && !it.value().isEmpty())
			list.append(qMakePair(it.key(), it.value()));
	}

	return list;
}

const QVector<PgnGame::MoveData>& PgnGame::moves() const
{
	return m_moves;
}

void PgnGame::addMove(const MoveData& data, quint64 key, bool addEco)
{
	m_moves.append(data);
	m_key = key;

	if (addEco && isStandard())
	{
		const EcoNode* eco = EcoNode::find(key);
		if (eco)
		{
			setTag("ECO", eco->ecoCode());
			setTag("Opening", eco->opening());
			setTag("Variation", eco->variation());
		}
	}
}

void PgnGame::setMove(int ply, const MoveData& data)
{
	m_moves[ply] = data;
}

Chess::Board* PgnGame::createBoard() const
{
	Chess::Board* board = Chess::BoardFactory::create(variant());
	if (board == nullptr)
		return nullptr;

	bool ok = true;

	QString fen(startingFenString());
	if (!fen.isEmpty())
		ok = board->setFenString(fen);
	else
	{
		board->reset();
		ok = !board->isRandomVariant();
	}
	if (!ok)
	{
		delete board;
		return nullptr;
	}

	return board;
}

bool PgnGame::parseMove(PgnStream& in, bool addEco)
{
	if (m_tags.isEmpty())
	{
		qWarning("No tags found");
		return false;
	}

	Chess::Board* board(in.board());

	// If the FEN string wasn't already set by the FEN tag,
	// set the board when we get the first move
	if (m_moves.isEmpty())
	{
		QString tmp(m_tags.value("Variant").toLower());
		if (tmp == "chess" || tmp == "normal")
			tmp = QString("standard");

		if (!tmp.isEmpty() && !in.setVariant(tmp))
		{
			qWarning("Unknown variant: %s", qUtf8Printable(tmp));
			return false;
		}
		board = in.board();
		if (tmp.isEmpty() && board->variant() != "standard")
			setTag("Variant", board->variant());

		tmp = m_tags.value("FEN");
		if (tmp.isEmpty())
		{
			if (board->isRandomVariant())
			{
				qWarning("Missing FEN tag");
				return false;
			}
			tmp = board->defaultFenString();
		}

		if (!board->setFenString(tmp))
		{
			qWarning("Invalid FEN string: %s", qUtf8Printable(tmp));
			return false;
		}
		m_startingSide = board->startingSide();
	}

	const QString str(in.tokenString());
	Chess::Move move(board->moveFromString(str));
	if (move.isNull())
	{
		qWarning("Illegal move: %s", qUtf8Printable(str));
		return false;
	}

	MoveData md = { board->key(), board->genericMove(move),
			str, QString() };
	board->makeMove(move);
	addMove(md, board->key(), addEco);

	return true;
}

bool PgnGame::read(PgnStream& in, int maxMoves, bool addEco)
{
	clear();
	if (!in.nextGame())
		return false;

	while (in.status() == PgnStream::Ok)
	{
		bool stop = false;

		switch (in.readNext())
		{
		case PgnStream::PgnTag:
			setTag(in.tagName(), in.tagValue());
			break;
		case PgnStream::PgnMove:
			stop = !parseMove(in, addEco) || m_moves.size() >= maxMoves;
			break;
		case PgnStream::PgnComment:
			if (!m_moves.isEmpty())
				m_moves.last().comment.append(in.tokenString());
			break;
		case PgnStream::PgnResult:
			{
				const QString str(in.tokenString());
				QString result = m_tags.value("Result");

				if (!result.isEmpty() && str != result)
					qWarning("%s", qUtf8Printable(QString("Line %1: The termination "
						 "marker is different from the result tag").arg(in.lineNumber())));
				setTag("Result", str);
			}
			stop = true;
			break;
		case PgnStream::PgnNag:
			{
				bool ok;
				int nag = in.tokenString().toInt(&ok);
				if (!ok || nag < 0 || nag > 255)
					qWarning("Invalid NAG: %s", in.tokenString().constData());
			}
			break;
		case PgnStream::NoToken:
			stop = true;
			break;
		default:
			break;
		}

		if (stop)
			break;
	}
	if (m_tags.isEmpty())
		return false;

	setTag("PlyCount", QString::number(m_moves.size()));

	return true;
}

bool PgnGame::write(QTextStream& out, PgnMode mode)
{
    if (m_tags.isEmpty())
        return false;

    // 1) if tags have changed => full rewrite
    if (m_tag_changed) {
        const QList< QPair<QString, QString> > tags = this->tags();
        int maxTags = (mode == Verbose) ? tags.size() : 7;
        for (int i = 0; i < maxTags; i++)
            writeTag(out, tags.at(i).first, tags.at(i).second);

        if (mode == Minimal && m_tags.contains("FEN"))
        {
            writeTag(out, "FEN", m_tags["FEN"]);
            writeTag(out, "SetUp", m_tags["SetUp"]);
        }

        if (mode == Minimal && m_tags.contains("Variant")
            &&  variant() != "standard")
        {
            writeTag(out, "Variant", m_tags["Variant"]);
        }
        m_tag_changed = 0;
    }

    // 2) skip saved moves
	int movenum = 0;
	int side = m_startingSide;

    if (!m_last_move && !m_initialComment.isEmpty())
		out << "\n" << "{" << m_initialComment << "}";

    // skip moves
    for (auto i = 0; i < m_last_move; i ++) {
        if (i == 0 && side == Chess::Side::Black)
            movenum ++;
        else if (side == Chess::Side::White)
            movenum ++;

        side = !side;
    }

    // 3) save from the last move, not from 0
    QString str;
    int lineLength = 0;

    for (auto i = m_last_move; i < m_moves.size(); i++)
	{
		const MoveData& data = m_moves.at(i);

		str.clear();
		if (i == 0 && side == Chess::Side::Black)
			str = QString::number(++movenum) + "... ";
		else if (side == Chess::Side::White)
			str = QString::number(++movenum) + ". ";

		str += data.moveString;
		if (mode == Verbose && !data.comment.isEmpty())
			str += QString(" {%1}").arg(data.comment);

		// Limit the lines to 80 characters
		if (lineLength == 0 || lineLength + str.size() >= 80)
		{
			out << "\n" << str;
			lineLength = str.size();
		}
		else
		{
			out << " " << str;
			lineLength += str.size() + 1;
		}

		side = !side;
	}
    m_last_move = m_moves.size();

    // 4) remember the last result
    m_last_result = m_tags.value("Result");
    if (lineLength + m_last_result.size() >= 80)
        out << "\n" << m_last_result << "\n\n";
    else
        out << " " << m_last_result << "\n\n";

    out.flush();
	return (out.status() == QTextStream::Ok);
}

/**
 * @param reset_flag
 * 		&1: reset the cursor to 0 => causes a full PGN write
 * 		&2: when cursor reset, empty the file (not wanted when appending files)
 */
bool PgnGame::write(const QString& filename, int reset_flag, PgnMode mode)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
		return false;

    if (reset_flag & 1)
        resetCursor();

    if (m_tag_changed) {
		// good for single PGN writes
        if (reset_flag & 2)
            file.resize(0);
    }
    // remove the "\n*\n\n" at the end of the file
    else if (m_last_result == "*") {
        auto size = file.size();
        file.resize((size > 4)? size - 4: 0);
        m_last_result.clear();
    }

	QTextStream out(&file);
	return write(out, mode);
}

bool PgnGame::isStandard() const
{
	return variant() == "standard" && !m_tags.contains("FEN");
}

QString PgnGame::tagValue(const QString& tag) const
{
	return m_tags.value(tag);
}

QString PgnGame::event() const
{
	return m_tags.value("Event");
}

QString PgnGame::site() const
{
	return m_tags.value("Site");
}

QDate PgnGame::date() const
{
	return QDate::fromString(m_tags.value("Date"), "yyyy.MM.dd");
}

int PgnGame::round() const
{
	return m_tags.value("Round").toInt();
}

QString PgnGame::playerName(Chess::Side side) const
{
	if (side == Chess::Side::White)
		return m_tags.value("White");
	else if (side == Chess::Side::Black)
		return m_tags.value("Black");

	return QString();
}

Chess::Result PgnGame::result() const
{
	return Chess::Result(m_tags.value("Result"));
}

QString PgnGame::variant() const
{
	if (m_tags.contains("Variant"))
	{
		QString variant(m_tags.value("Variant").toLower());
		if ("chess" != variant && "normal" != variant)
			return variant;
	}
	return "standard";
}

Chess::Side PgnGame::startingSide() const
{
	return m_startingSide;
}

QString PgnGame::startingFenString() const
{
	return m_tags.value("FEN");
}

quint64 PgnGame::key() const
{
	return m_key;
}

void PgnGame::setTag(const QString& tag, const QString& value)
{
    // PGN optimization: tag changed => full write
    auto changed = 0;
    auto prev = m_tags[tag];

    if (value.isEmpty()) {
        if (prev.size())
            changed ++;
		m_tags.remove(tag);
    }
    else {
        if (value != prev)
            changed ++;
        m_tags[tag] = value;
    }
    if (!changed)
        return;

    resetCursor();

	if (m_tagReceiver)
		QMetaObject::invokeMethod(m_tagReceiver, "setTag",
					  Qt::QueuedConnection,
					  Q_ARG(QString, tag),
					  Q_ARG(QString, value));
}

void PgnGame::setEvent(const QString& event)
{
	setTag("Event", event);
}

void PgnGame::setSite(const QString& site)
{
	setTag("Site", site);
}

void PgnGame::setDate(const QDate& date)
{
	setTag("Date", date.toString("yyyy.MM.dd"));
}

void PgnGame::setRound(int round, int game)
{
	QString value(QString::number(round));
	if (game > 0)
		value += '.' + QString::number(game);
	setTag("Round", value);
}

void PgnGame::setPlayerName(Chess::Side side, const QString& name)
{
	if (side == Chess::Side::White)
		setTag("White", name);
	else if (side == Chess::Side::Black)
		setTag("Black", name);
}

void PgnGame::setPlayerRating(Chess::Side side, const int rating)
{
	if (side == Chess::Side::White && rating) // remove "&& rating" if "-" is desired
		setTag("WhiteElo", rating ? QString::number(rating) : "-");
	else if (side == Chess::Side::Black && rating)
		setTag("BlackElo", rating ? QString::number(rating) : "-");
}

void PgnGame::setResult(const Chess::Result& result)
{
	setTag("Result", result.toShortString());

	switch (result.type())
	{
	case Chess::Result::Adjudication:
		setTag("Termination", "adjudication");
		break;
	case Chess::Result::Timeout:
		setTag("Termination", "time forfeit");
		break;
	case Chess::Result::Disconnection:
		setTag("Termination", "abandoned");
		break;
	case Chess::Result::StalledConnection:
		setTag("Termination", "stalled connection");
		break;
	case Chess::Result::IllegalMove:
		setTag("Termination", "illegal move");
		break;
	case Chess::Result::NoResult:
		setTag("Termination", "unterminated");
		break;
	default:
		setTag("Termination", QString());
		break;
	}
}

void PgnGame::setVariant(const QString& variant)
{
	if (variant == "standard")
		setTag("Variant", QString());
	else
		setTag("Variant", variant);
}

void PgnGame::setStartingSide(Chess::Side side)
{
	m_startingSide = side;
}

void PgnGame::setStartingFenString(Chess::Side side, const QString& fen)
{
	m_startingSide = side;
	if (fen.isEmpty())
	{
		setTag("FEN", QString());
		setTag("SetUp", QString());
	}
	else
	{
		setTag("FEN", fen);
		setTag("SetUp", "1");
	}
}

void PgnGame::setResultDescription(const QString& description)
{
	if (description.isEmpty())
		return;
	if (m_moves.isEmpty())
	{
		m_initialComment = description;
		return;
	}

	QString& comment = m_moves.last().comment;
	if (!comment.isEmpty())
	{
		if (comment[comment.size() - 1] != ',')
			comment += ',';
		comment += ' ';
	}

	comment += description;
}

void PgnGame::setTagReceiver(QObject* receiver)
{
	m_tagReceiver = receiver;
}

QString PgnGame::timeStamp(const QDateTime& dateTime)
{
	return dateTime.toString("yyyy-MM-ddThh:mm:ss.zzz t");
}

void PgnGame::setGameStartTime(const QDateTime& dateTime)
{
	m_gameStartTime = dateTime;
	setTag("GameStartTime", timeStamp(dateTime));
}

void PgnGame::setGameEndTime(const QDateTime& dateTime)
{
	setTag("GameEndTime", timeStamp(dateTime));

	int d = m_gameStartTime.secsTo(dateTime);
	m_gameDuration = QTime(d / 3600, d % 3600 / 60, d % 60);
	setTag("GameDuration", m_gameDuration.toString("hh:mm:ss"));
}

const QTime& PgnGame::gameDuration() const
{
	return m_gameDuration;
}

QString PgnGame::initialComment() const
{
	return m_initialComment;
}
