import chess
import chess.polyglot

board = chess.Board("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1")
h = chess.polyglot.zobrist_hash(board)
print(f"Hash: {h}")
