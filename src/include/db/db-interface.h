/*
 * =====================================================================================
 *
 *       Filename:  db_interface.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/13/2014 03:20:44 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef DB_INTERFACE_H
#define DB_INTERFACE_H
#include <stdint.h>

typedef struct db_t db;

db* initialize_db(const char* db_name,uint32_t flag);

void close_db(db*,uint32_t);

int store_record(db*,uint64_t,uint32_t,void*);

// the caller is responsible to release the memory

void* retrieve_record(db*,uint64_t);

#endif
