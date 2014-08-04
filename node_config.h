/*
 * =====================================================================================
 *
 *       Filename:  node_config.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/04/2014 04:30:28 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H
#include <sys/time.h>
struct timeval reconnect_timeval = {2,0};
struct timeval ping_timeval = {10,0};
int heart_beat_threshold = 4;
#endif
