
CREATE TABLE users (
       name  	   varchar(80),
       password	   varchar(80),
       salt	   bigint
);

CREATE TABLE music (
       title  	   varchar(256),
       artist  	   varchar(256),
       album  	   varchar(256),
       track	   int,
       time	   int,          -- in seconds
       path        varchar(512),
       modified	   timestamp(0)
);

GRANT ALL ON users, music TO "www-data";
