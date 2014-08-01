#ifndef DB_H
#define DB_H

#include <sys/types.h>

/*
 Database structure:

 CREATE DATABASE cce ;

 USE cce ;

 CREATE TABLE `clients` (
   `id` int(11) NOT NULL AUTO_INCREMENT,
   `address` varchar(256) COLLATE utf8_bin NOT NULL,
   `port` varchar(6) COLLATE utf8_bin NOT NULL,
   `date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
   PRIMARY KEY (`id`)
 ) ENGINE=InnoDB  DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;

 CREATE TABLE `programs` (
   `id` int(11) NOT NULL AUTO_INCREMENT,
   `client_id` int(11) NOT NULL,
   `start_date` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
   `end_date` timestamp NULL DEFAULT NULL,
   `name` varchar(300) COLLATE utf8_bin DEFAULT NULL,
   PRIMARY KEY (`id`),
   FOREIGN KEY (`client_id`) REFERENCES clients(id)
 ) ENGINE=InnoDB DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;

 CREATE TABLE `active_clients` (
   `id` int(11) NOT NULL,
   `program_id` int(11) DEFAULT NULL,
   FOREIGN KEY (`id`) REFERENCES clients(id),
   FOREIGN KEY (`program_id`) REFERENCES programs(id)
 ) ENGINE=InnoDB DEFAULT CHARSET=latin1;
*/

#define QUERY_LEN 2048

int init_db();

int lock_db();
int unlock_db();

int db_add_cli(const char *host, const char *serv, id_t *new_id);
int db_get_last_id(id_t *new_id);

int db_add_active_cli(id_t id);
int db_remove_active_cli(id_t id);

int db_add_program(id_t cli_id, id_t *pgrm_id, time_t start, char *name);

int db_get_last_pgrm_id(id_t *pgrm_id);

#endif /* end of include guard: DB_H */
