/*
 * =====================================================================================
 *
 *       Filename:  config.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/29/2014 03:25:24 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H
#include "common-header.h"
#include "node.h"


extern int max_waiting_connections;
#define MAX_ACCEPT_CONNECTIONS 500

int read_configuration_file(node* cur_node,const char* config_file);




#endif
