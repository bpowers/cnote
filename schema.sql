DROP TABLE IF EXISTS users, music;

CREATE TABLE users (
       name  	   varchar(80) PRIMARY KEY,
       password	   varchar(80) NOT NULL,
       salt	   bigint
);

CREATE SEQUENCE music_index_seq MINVALUE 0;
CREATE TABLE music (
       index 	   serial PRIMARY KEY,
       title  	   varchar(256) NOT NULL,
       artist  	   varchar(256) NOT NULL,
       album  	   varchar(256) NOT NULL,
       track	   int,
       time	   int,                          -- in seconds
       path        varchar(512) UNIQUE NOT NULL,
       hash        char(64) NOT NULL,
       modified	   timestamp(0)
);

GRANT ALL ON users, music TO "www-data";
