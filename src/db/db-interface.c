/*
 * =====================================================================================
 *
 *       Filename:  db-interface.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/18/2014 02:14:41 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <db.h>
#include "../include/db/db-interface.h"
#include "../include/util/debug.h"

const char* db_dir="./.db";

struct db_t{
    DB* bdb_ptr;
};


void mk_path(char* dest,const char* prefix,const char* db_name){
    memcpy(dest,prefix,strlen(prefix));
    dest[strlen(prefix)] = '/';
    memcpy(dest+strlen(prefix)+1,db_name,strlen(db_name));
    dest[strlen(prefix)+strlen(db_name)+1] = '\0';
    return;
}

db* initialize_db(const char* db_name,uint32_t flag){
    db* db_ptr=NULL;
    DB* b_db;
    int ret;
    char* full_path = NULL;
    if((ret=mkdir(db_dir,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
        if(errno!=EEXIST){
            paxos_log("dir creation failed\n");
            goto db_init_return;
        }
    }
    full_path = (char*)malloc(strlen(db_dir) + strlen(db_name)+2);
    mk_path(full_path,db_dir,db_name);
    /* Initialize the DB handle */
    if((ret = db_create(&b_db, NULL,flag))!=0){
        paxos_log("%s:\n",db_strerror(ret));
        goto db_init_return;
    }

    debug_log("The Path To The Database Will Be %s.\n",full_path);
    if((ret = b_db->open(b_db,NULL,full_path,NULL,DB_BTREE,DB_CREATE,0))!=0){
        b_db->err(b_db,ret,"%s","test.db");
        goto db_init_return;
    }
    db_ptr = (db*)(malloc(sizeof(db)));
    db_ptr->bdb_ptr = b_db;

db_init_return:
    if(full_path!=NULL){
        free(full_path);
    }
    return db_ptr;
}

void close_db(db* db_p,uint32_t mode){
    if(db_p!=NULL){
        if(db_p->bdb_ptr!=NULL){
            db_p->bdb_ptr->close(db_p->bdb_ptr,mode);
            db_p->bdb_ptr=NULL;
        }
        free(db_p);
        db_p = NULL;
    }
    return;
}

int store_record(db* db_p,uint64_t record_no,uint32_t data_size,void* data){
    int ret = 1;
    if((NULL==db_p)||(NULL==db_p->bdb_ptr)){
        goto db_store_return;
    }
    DB* b_db = db_p->bdb_ptr;
    DBT key,db_data;
    memset(&key,0,sizeof(key));
    memset(&db_data,0,sizeof(db_data));
    key.data = &record_no;
    key.size = sizeof(record_no);
    db_data.data = data;
    db_data.size = data_size;
    if ((ret=b_db->put(b_db,NULL,&key,&db_data,0))==0){
        debug_log("db : %ld record stored. \n",record_no);
        b_db->sync(b_db,0);
    }
    else{
        b_db->err(b_db,ret,"DB->Put");
    }
db_store_return:
    return ret;
}


void* retrieve_record(db* db_p,uint64_t record_no){
    void* ret_p=NULL;
    int ret=1;
    if(NULL==db_p || NULL==db_p->bdb_ptr){
        goto db_retrieve_return;
    }
    DB* b_db = db_p->bdb_ptr;
    DBT key,db_data;
    memset(&key,0,sizeof(key));
    memset(&db_data,0,sizeof(db_data));
    key.data = &record_no;
    key.size = sizeof(record_no);
    if((ret=b_db->get(b_db,NULL,&key,&db_data,0))==0){
        debug_log("db : get record %ld from database.\n",record_no);
    }else{
        b_db->err(b_db,ret,"DB->Get");
        goto db_retrieve_return;
    }
    if(!db_data.size){
        goto db_retrieve_return;
    }
    ret_p = db_data.data;
db_retrieve_return:
    return ret_p;
}
