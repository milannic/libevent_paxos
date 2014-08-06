/*
 * =====================================================================================
 *
 *       Filename:  commnon-structure.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/04/2014 04:10:18 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic, milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef COMMON_STRUCTURE_H
#define COMMON_STRUCTURE_H
#include <stdint.h>

typedef struct view_t{
    uint32_t view_id;
    uint32_t leader_id;
}view;

typedef struct view_stamp_t{
    view view_id;
    uint32_t req_id;
}view_stamp;

uint64_t vstol(view_stamp* vs);

#endif 
