-- Build a deterministic database used by sqlite_query.sql.
-- Usage: sqlite3 bench.db < sqlite_create.sql
PRAGMA journal_mode = OFF;
PRAGMA synchronous = OFF;

DROP TABLE IF EXISTS t;
CREATE TABLE t (id INTEGER PRIMARY KEY, a INTEGER, b INTEGER, c REAL, s TEXT);

WITH RECURSIVE seq(i) AS (
  SELECT 1
  UNION ALL
  SELECT i + 1 FROM seq WHERE i < 300000
)
INSERT INTO t (id, a, b, c, s)
SELECT i,
       (i * 1103515245 + 12345) % 100003,
       (i * 2654435761) % 65537,
       (i % 1000) / 7.0,
       'row-' || (i % 97)
FROM seq;

CREATE INDEX idx_a ON t(a);
CREATE INDEX idx_b ON t(b);
ANALYZE;
