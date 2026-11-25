PRAGMA cache_size = -262144;
SELECT sum(a*b + (c > 0.5)*3 + (a & 7)) FROM t WHERE (a & 7) = 3;
SELECT count(*) FROM t WHERE (b % 13) = 5 AND a > 1000;
SELECT sum(((a | b) - (a & b)) & 1023) FROM t WHERE c < 500;
SELECT max(a + b), min(a - b) FROM t WHERE (a & 3) = 1;
SELECT sum(a) FROM t WHERE s = "row-7";
SELECT sum(((a * 3) | (b >> 2))) FROM t;
