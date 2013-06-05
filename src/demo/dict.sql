su mysql
mysql -u root -p
use mysql
SELECT user, host, password FROM user;

GRANT ALL ON *.* TO FastDCS@localhost IDENTIFIED BY 'fastdcs';
GRANT ALL ON *.* TO FastDCS@'%' IDENTIFIED BY 'fastdcs';

show FastDCS;

CREATE DATABASE FastDCS;

CREATE TABLE dict_task (
	task_id VARCHAR(32) NOT NULL PRIMARY KEY,
	task_status INTEGER default 0
);

CREATE TABLE dict_word (
	word_id VARCHAR(32) NOT NULL PRIMARY KEY,
	word VARCHAR(128) NOT NULL,
	count INTEGER default 0 
);

SELECT word, word_id, count FROM dict_word ORDER BY count desc limit 5;

SELECT count(1), task_status FROM dict_task GROUP BY task_status;

UPDATE dict_task SET task_status = 1 WHERE task_status != 1;

TRUNCATE TABLE dict_word;
