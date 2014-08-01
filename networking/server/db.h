#ifndef DB_H
#define DB_H

#include <sys/types.h>

/*
 Database structure:

 CREATE DATABASE cce ;

 USE cce ;

 CREATE TABLE IF NOT EXISTS `clients` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `address` varchar(256) COLLATE utf8_bin NOT NULL,
  `port` varchar(6) COLLATE utf8_bin NOT NULL,
  `date` timestamp NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;

CREATE TABLE IF NOT EXISTS `programs` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `client_id` int(11) NOT NULL,
  `start_date` date NOT NULL,
  `end_date` date NOT NULL,
  `name` date NOT NULL,
  `bin` varchar(1024) COLLATE utf8_bin NOT NULL,
  `txt` varchar(1024) COLLATE utf8_bin NOT NULL,
  `srt` varchar(1024) COLLATE utf8_bin NOT NULL,
  `xds` varchar(1024) COLLATE utf8_bin NOT NULL,
  PRIMARY KEY (`id`),
  FOREIGN KEY (`client_id`) REFERENCES clients(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;

*/

#define QUERY_LEN 2048

int init_db();

int db_add_cli(const char *host, const char *serv, id_t *new_id);
int db_get_last_id(id_t *new_id);

int lock_db();
int unlock_db();

#endif /* end of include guard: DB_H */

