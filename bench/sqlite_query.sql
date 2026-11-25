-- Query mix executed by the measured phase.  Kept deterministic so every run
-- executes exactly the same VDBE program.
PRAGMA cache_size = -65536;
PRAGMA temp_store = MEMORY;

SELECT count(*), sum(a), avg(c) FROM t WHERE a < 50000;
SELECT b, count(*) FROM t GROUP BY b ORDER BY count(*) DESC LIMIT 20;
SELECT count(*) FROM t x JOIN t y ON x.a = y.a WHERE x.id < 20000;
SELECT sum(x.c * y.c) FROM t x JOIN t y ON x.b = y.b WHERE y.id % 7 = 0;
SELECT s, sum(a) FROM t GROUP BY s HAVING sum(a) > 1000000 ORDER BY 2 DESC;
SELECT id, a, b FROM t WHERE b BETWEEN 1000 AND 1200 ORDER BY c DESC LIMIT 50;
SELECT count(*) FROM (SELECT a, count(*) AS n FROM t GROUP BY a HAVING n > 1);
SELECT max(c), min(c), sum(c * c) FROM t WHERE id % 3 = 0;
SELECT count(*) FROM t WHERE s LIKE 'row-1%' AND a % 2 = 0;
SELECT sum(a) OVER (ORDER BY id ROWS BETWEEN 10 PRECEDING AND CURRENT ROW)
  FROM t WHERE id <= 5000;
