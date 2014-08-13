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

struct db_t;
typedef struct db_t* dbptr;

typedef struct db_record_t{
    uint64_t record_no;
    uint32_t data_size;
    char data[0];
}__attribute__((packed))db_record;

typedef db_record* db_record_ptr;

void* store_record(db_record_ptr,void*);
db_record_ptr retrive_record(uint64_t);

#endif
