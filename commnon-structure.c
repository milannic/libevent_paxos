/*
 * =====================================================================================
 *
 *       Filename:  commnon-structure.c
 *
 *    Description:  j
 *
 *        Version:  1.0
 *        Created:  08/06/2014 04:54:08 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "commnon-structure.h"

uint64_t vstol(view_stamp* vs){
    uint64_t result = vs->req_id;
    result += (vs->view_id.view_id)<<31;
    return result;
}
