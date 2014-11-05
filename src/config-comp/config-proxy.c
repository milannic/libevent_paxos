/*
 * =====================================================================================
 *
 *       Filename:  config-proxy.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/16/2014 04:44:59 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */


#include "../include/util/common-header.h"
#include "../include/proxy/proxy.h"
#include <libconfig.h>


int proxy_read_config(struct proxy_node_t* cur_node,const char* config_path){
    ENTER_FUNC
    config_t config_file;
    config_init(&config_file);

    if(!config_read_file(&config_file,config_path)){
        goto goto_config_error;
    }
    
    uint32_t group_size;
    if(!config_lookup_int(&config_file,"group_size",(int*)&group_size)){
        goto goto_config_error;
    }

    if(group_size<=cur_node->node_id){
        err_log("Proxy : Invalid Node Id\n");
        goto goto_config_error;
    }

// parse consensus address
    config_setting_t *consensus_config;
    consensus_config = config_lookup(&config_file,"consensus_config");

    if(NULL==consensus_config){
        err_log("cannot find nodes settings \n");
        goto goto_config_error;
    }    

    debug_log("the length is %d\n",
            config_setting_length(consensus_config));

    if(NULL==consensus_config){
        err_log("cannot find node address section \n");
        goto goto_config_error;
    }
    config_setting_t *con_ele = config_setting_get_elem(consensus_config,cur_node->node_id);
    if(NULL==con_ele){
        err_log("cannot find current node's address\n");
        goto goto_config_error;
    }
    const char* peer_ipaddr=NULL;
    int peer_port=-1;

    if(!config_setting_lookup_string(con_ele,"ip_address",&peer_ipaddr)){
        goto goto_config_error;
    }
    if(!config_setting_lookup_int(con_ele,"port",&peer_port)){
        goto goto_config_error;
    }
    cur_node->sys_addr.c_addr.sin_port = htons(peer_port);
    cur_node->sys_addr.c_addr.sin_family = AF_INET;
    inet_pton(AF_INET,peer_ipaddr,&cur_node->sys_addr.c_addr.sin_addr);
    cur_node->sys_addr.c_sock_len = sizeof(cur_node->sys_addr.c_addr);

// parse server address
    config_setting_t *server_config;
    server_config = config_lookup(&config_file,"server_config");

    if(NULL==server_config){
        err_log("cannot find nodes settings \n");
        goto goto_config_error;
    }    

    debug_log("the length is %d\n",
            config_setting_length(server_config));

    if(NULL==server_config){
        err_log("cannot find node address section \n");
        goto goto_config_error;
    }
    config_setting_t *serv_ele = config_setting_get_elem(server_config,cur_node->node_id);
    if(NULL==con_ele){
        err_log("cannot find current node's address\n");
        goto goto_config_error;
    }

    peer_ipaddr=NULL;
    peer_port=-1;
    if(!config_setting_lookup_string(serv_ele,"ip_address",&peer_ipaddr)){
        goto goto_config_error;
    }
    if(!config_setting_lookup_int(serv_ele,"port",&peer_port)){
        goto goto_config_error;
    }

    cur_node->sys_addr.s_addr.sin_port = htons(peer_port);
    cur_node->sys_addr.s_addr.sin_family = AF_INET;
    inet_pton(AF_INET,peer_ipaddr,&cur_node->sys_addr.s_addr.sin_addr);

    cur_node->sys_addr.s_sock_len = sizeof(cur_node->sys_addr.s_addr);
// parse proxy address
    config_setting_t *proxy_config;
    proxy_config = config_lookup(&config_file,"proxy_config");


    if(NULL==proxy_config){
        err_log("cannot find nodes settings \n");
        goto goto_config_error;
    }    

    debug_log("the length is %d\n",
            config_setting_length(proxy_config));

    if(NULL==proxy_config){
        err_log("cannot find node address section \n");
        goto goto_config_error;
    }
    config_setting_t *pro_ele = config_setting_get_elem(proxy_config,cur_node->node_id);
    if(NULL==con_ele){
        err_log("cannot find current node's address\n");
        goto goto_config_error;
    }

    peer_ipaddr=NULL;
    peer_port=-1;

    if(!config_setting_lookup_string(pro_ele,"ip_address",&peer_ipaddr)){
        goto goto_config_error;
    }
    if(!config_setting_lookup_int(pro_ele,"port",&peer_port)){
        goto goto_config_error;
    }

    debug_log("test %d.\n",cur_node->node_id);
    debug_log("test %d.\n",peer_port);

    cur_node->sys_addr.p_addr.sin_port = htons(peer_port);
    cur_node->sys_addr.p_addr.sin_family = AF_INET;
    inet_pton(AF_INET,peer_ipaddr,&cur_node->sys_addr.p_addr.sin_addr);
    cur_node->sys_addr.p_sock_len = sizeof(cur_node->sys_addr.p_addr);

    debug_log("test %lu.\n",cur_node->sys_addr.p_sock_len);

    const char* db_name;
    if(!config_setting_lookup_string(pro_ele,"db_name",&db_name)){
        goto goto_config_error;
    }
    size_t db_name_len = strlen(db_name);
    cur_node->db_name = (char*)malloc(sizeof(char)*(db_name_len+1));
    if(cur_node->db_name==NULL){
        goto goto_config_error;
    }
    if(NULL==strncpy(cur_node->db_name,db_name,db_name_len)){
        free(cur_node->db_name);
        goto goto_config_error;
    }
    cur_node->db_name[db_name_len] = '\0';

    config_destroy(&config_file);
    LEAVE_FUNC
    return 0;

goto_config_error:
    err_log("%s:%d - %s\n", config_error_file(&config_file),
            config_error_line(&config_file), config_error_text(&config_file));
    config_destroy(&config_file);
    ERR_LEAVE_FUNC
    return -1;
}
