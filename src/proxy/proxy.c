/*
 * =====================================================================================
 *
 *       Filename:  proxy.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/14/2014 11:19:25 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "../include/proxy/proxy.h"
#include "../include/config-comp/config-proxy.h"



#define MY_HASH_SET(value,hash_map,ret) { if(value==NULL){ret=1;}else{ \\
 HASH_ADD(hh,hash_map,key,sizeof(hk_t),value); }} 

#define MY_HASH_GET(key,hash_map,ret){ if(value==NULL){ret=1;}else{ \\
 HASH_ADD(hh,hash_map,key,sizeof(hk_t),value); }} 


static void* shared_mem=NULL; 
static rec_no_t cur_id=0;
static rec_no_t highest_id=0;




//helper function
static hk_t gen_key(nid_t,nc_t,sec_t);
static socket_pair* hash_get(hk_t key,socket_pair* hash_map){
    socket_pair* ret = NULL;
    HASH_FIND(hh,hash_map,&key,sizeof(hk_t),ret);
    return ret;

}

static int hash_set(socket_pair* value,socket_pair* hash_map){
    // error
    if(value==NULL){
        return 1;
    }else{
        HASH_ADD(hh,hash_map,key,sizeof(hk_t),value);
    }
    return 0;
}
// consensus callback
static void update_state(int,void*);

static void usage();


//implementation

static hk_t gen_key(nid_t node_id,nc_t node_count,sec_t time){
    hk_t key = time;
    key |= ((hk_t)node_id<<52);
    key |= ((hk_t)node_count<<36);
    return key;
}

static void update_state(int data_size,void* data){
    ENTER_FUNC
    paxos_log("the newly delivered request is %lu.\n",*(hk_t*)data);
    void* dest = ((flag_t*)shared_mem)+1; 
    dest = ((rec_no_t*)dest)+1; 
    memcpy(dest,data,data_size);
    LEAVE_FUNC
    return;
}


//public interface

proxy_node* proxy_init(int argc,char** argv){
    ENTER_FUNC
    char* start_mode= NULL;
    char* config_path = NULL;
    int node_id = -1;
    int c;

    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        goto exit_error;
    }

    memset(proxy,0,sizeof(proxy_node));

    while((c = getopt (argc,argv,"c:n:m:")) != -1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 'c':
                config_path = optarg;
                break;
            case 'm':
                start_mode= optarg;
                if(*start_mode!='s' && *start_mode!='r'){
                    paxos_log("Unknown Start Mode\n");
                    usage();
                    goto exit_error;
                }
                break;
            case '?':
                if(optopt == 'n'){
                    paxos_log("Option -n requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'm'){
                    paxos_log("Option -m requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'c'){
                    paxos_log("Option -c requires an argument\n");
                    usage();
                    goto exit_error;
                }
                break;
            default:
                paxos_log("Unknown Option %d \n",c);
                goto exit_error;
        }
    }

    if(argc<7 || optind>argc){
        usage();
        goto exit_error;
    }

    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto exit_error;
    }

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    proxy->base = base;
    proxy->node_id = node_id;

    if(proxy_read_config(proxy,config_path)){
        goto exit_error;
    }
    // ensure the value is NULL at first
    proxy->hash_map=NULL;
    
#if 0
    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&proxy->sys_addr.p_addr.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current proxy's ip address is %s:%d\n",ipv4_address,ntohs(proxy->sys_addr.p_addr.sin_port));
    inet_ntop(AF_INET,&proxy->sys_addr.c_addr.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current consensus's ip address is %s:%d\n",ipv4_address,ntohs(proxy->sys_addr.c_addr.sin_port));
    inet_ntop(AF_INET,&proxy->sys_addr.s_addr.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current server's ip address is %s:%d\n",ipv4_address,ntohs(proxy->sys_addr.s_addr.sin_port));
    debug_log("current proxy's db name is %s\n",proxy->db_name);
#endif

    proxy->con_node = system_initialize(node_id,start_mode,config_path,update_state);

    if(NULL==proxy->con_node){
        paxos_log("cannot initialize node\n");
        goto exit_error;
    }

//    proxy->listener =
//        evconnlistener_new_bind(base,NULL,
//                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
//                (struct sockaddr*)&proxy->sys_addr.p_addr,sizeof(proxy->sys_addr.p_addr));
//
//    if(!proxy->listener){
//        paxos_log("cannot set up the listener\n");
//        goto exit_error;
//    }

	return proxy;

exit_error:
    if(NULL!=proxy){
        if(NULL!=proxy->con_node){
            // to do
        }
        free(proxy);
    }
    LEAVE_FUNC
    return NULL;
}


void proxy_run(proxy_node* proxy){
    return;
}
void proxy_exit(proxy_node* proxy){
    return;
}


static void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -m [sr] Start Mode seed|recovery\n");
    paxos_log("        -c path path to configuration file\n");
}

