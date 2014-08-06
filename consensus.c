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

#include "common-header.h"
#include "consensus.h"
#include "leader.h"
#include "learner.h"

typedef struct consensus_component_t{
    con_role my_role;
    // use up call to transfer message to other replicas
    net_up_call net_cb;
    leader* p_leader;
    learner* p_learner;
}consensus_component;



