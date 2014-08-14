/*
 * =====================================================================================
 *
 *       Filename:  consensus.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/06/2014 02:13:09 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../include/consensus/consensus.h"

typedef struct consensus_component_t{
    con_role my_role;
    // use up call to transfer message to other replicas
    up_call uc;
    user_cb ucb;
}consensus_component;



