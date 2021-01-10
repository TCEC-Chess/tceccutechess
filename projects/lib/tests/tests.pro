TEMPLATE = subdirs
SUBDIRS = chessboard tb sprt mersenne tournamentplayer tournamentpair polyglotbook graph_blossom
win32 {
    SUBDIRS += pipereader
}
