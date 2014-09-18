/*
 * =====================================================================================
 *
 *       Filename:  rsm-interface.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/16/2014 03:45:46 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef RSM_INTERFACE_H
#define RSM_INTERFACE_H

struct proxy_node_t;

struct proxy_node_t* proxy_init(int argc,char** argv);
void proxy_run(struct proxy_node_t* proxy);
void proxy_exit(struct proxy_node_t* proxy);

#endif
