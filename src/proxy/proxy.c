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
#include "../include/replica-sys/message.h"


static void* shared_mem=NULL; 
static rec_no_t cur_id=0;
static rec_no_t highest_id=0;
static pthread_t con_subt=9999999;
static struct timeval reconnect_timeval = {2,0};

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    char is_closed;
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))


//helper function
static hk_t gen_key(nid_t,nc_t,sec_t);

// consensus callback
static void update_state(int,void*);
static void usage();

static void proxy_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg);
static void consensus_on_event(struct bufferevent* bev,short ev,void* arg);
static void consensus_on_read(struct bufferevent*,void*);
static void connect_consensus(proxy_node*);
static void reconnect_on_timeout(int fd,short what,void* arg);


//socket pair callback function
static void server_side_on_read(struct bufferevent* bev,void* arg);
static void server_side_on_err(struct bufferevent* bev,short what,void* arg);
static void client_side_on_read(struct bufferevent* bev,void* arg);
static void client_side_on_err(struct bufferevent* bev,short what,void* arg);

static req_sub_msg* build_req_sub_msg(hk_t s_key,int type,size_t data_size,void* data);

//log component

//implementation

static hk_t gen_key(nid_t node_id,nc_t node_count,sec_t time){
    hk_t key = time;
    key |= ((hk_t)node_id<<52);
    key |= ((hk_t)node_count<<36);
    return key;
}

//static void update_state(int data_size,void* data){
//    ENTER_FUNC
//    paxos_log("the newly delivered request is %lu.\n",*(hk_t*)data);
//    void* dest = ((flag_t*)shared_mem)+1; 
//    dest = ((rec_no_t*)dest)+1; 
//    memcpy(dest,data,data_size);
//    LEAVE_FUNC
//    return;
//}

//fake update state, we take out the data directly without re-
//visit the database
static void update_state(int data_size,void* data){
    ENTER_FUNC
    proxy_msg_header* header = data;
    switch(header->action){
        case CONNECT:
        case SEND:
        case CLOSE:
            break;
    }
    LEAVE_FUNC
    return;
}


static void* t_consensus(void *arg){
    ENTER_FUNC
    struct node_t* my_node = arg;
    system_run(my_node);
    system_exit(my_node);
    LEAVE_FUNC
    return NULL;
}

//public interface
//


void consensus_on_event(struct bufferevent* bev,short ev,void* arg){
    proxy_node* proxy = arg;
    if(ev&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Consensus.\n");
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s.\n",evutil_socket_error_to_string(err));
        bufferevent_free(bev);
        proxy->con_conn = NULL;
        event_add(proxy->re_con,&reconnect_timeval);
    }

}
//void consensus_on_read(struct bufferevent* bev,void*);
void connect_consensus(proxy_node* proxy){
    proxy->con_conn = bufferevent_socket_new(proxy->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(proxy->con_conn,NULL,NULL,consensus_on_event,proxy);
    bufferevent_enable(proxy->con_conn,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(proxy->con_conn,(struct sockaddr*)&proxy->sys_addr.c_addr,proxy->sys_addr.c_sock_len);
}

void reconnect_on_timeout(int fd,short what,void* arg){
    // this function should also test whether it should re-set 
    // a thread to run the consensus module but currently 
    // ignore this
    connect_consensus(arg);
}


static void server_side_on_read(struct bufferevent* bev,void* arg){
    return;
}
static void server_side_on_err(struct bufferevent* bev,short what,void* arg){
    return;
}
static void client_side_on_read(struct bufferevent* bev,void* arg){
    return;
}
static void client_side_on_err(struct bufferevent* bev,short what,void* arg){
    return;
}


static req_sub_msg* build_req_sub_msg(hk_t s_key,int type,size_t data_size,void* data){
    req_sub_msg* msg = NULL;
    switch(type){
        case CONNECT:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+PROXY_CONNECT_MSG_SIZE);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = PROXY_CONNECT_MSG_SIZE;
            proxy_connect_msg* co_msg = (void*)msg->data;
            co_msg->header.action = CONNECT;
            gettimeofday(&co_msg->header.created_time,NULL);
            co_msg->header.connection_id = s_key;
            break;
        case SEND:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+sizeof(proxy_send_msg)+data_size);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = sizeof(proxy_send_msg)+data_size;
            proxy_send_msg* send_msg = (void*)msg->data;
            send_msg->header.action = SEND;
            gettimeofday(&send_msg->header.created_time,NULL);
            send_msg->header.connection_id = s_key;
            send_msg->data_size = data_size;
            memcpy(send_msg->data,data,data_size);
            break;
        case CLOSE:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+PROXY_CONNECT_MSG_SIZE);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = PROXY_CONNECT_MSG_SIZE;
            proxy_close_msg* cl_msg = (void*)msg->data;
            cl_msg->header.action = CONNECT;
            gettimeofday(&cl_msg->header.created_time,NULL);
            cl_msg->header.connection_id = s_key;
            break;
        default:
            goto build_req_sub_msg_err_exit;
    }
    return msg;
build_req_sub_msg_err_exit:
    if(NULL!=msg){
        free(msg);
    }
    return NULL;
}

static void proxy_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg){
    req_sub_msg* req_msg = NULL;
    proxy_node* proxy = arg;
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(proxy->con_conn==NULL){
        paxos_log("We have lost the connection to consensus component now.\n");
        close(fd);
    }else{
        socket_pair* new_conn = malloc(sizeof(socket_pair));
        struct timeval cur;
        gettimeofday(&cur,NULL);
        new_conn->key = gen_key(proxy->node_id,proxy->pair_count++,
                cur.tv_sec);
        new_conn->p_c = bufferevent_socket_new(proxy->base,fd,BEV_OPT_CLOSE_ON_FREE);
        new_conn->proxy = proxy;
        bufferevent_setcb(new_conn->p_c,server_side_on_read,NULL,server_side_on_err,new_conn);
        bufferevent_enable(new_conn->p_c,EV_READ|EV_PERSIST|EV_WRITE);
        MY_HASH_SET(new_conn,proxy->hash_map);
        // connect operation should be consistent among all the proxies.
        req_msg = build_req_sub_msg(new_conn->key,CONNECT,0,NULL); 
        bufferevent_write(proxy->con_conn,req_msg,REQ_SUB_SIZE(req_msg));
    }
    if(req_msg!=NULL){
        free(req_msg);
    }
    return;
}

static void proxy_singnal_handler(int sig){
    ENTER_FUNC
    switch(sig){
        case SIGTERM:
            paxos_log("the sig value is %d .\n",sig);
            paxos_log("the sub value is %lu .\n",con_subt);
            if(con_subt!=9999999){
                pthread_kill(con_subt,SIGQUIT);
                paxos_log("wating consensus comp to quit.\n");
                pthread_join(con_subt,NULL);
            }
            LEAVE_FUNC
            exit(sig);
    }
}

proxy_node* proxy_init(int node_id,const char* start_mode,const char* config_path,
        const char* log_path,int fake_mode){
    ENTER_FUNC
    signal(SIGINT,proxy_singnal_handler);
    signal(SIGQUIT,proxy_singnal_handler);
    signal(SIGTERM,proxy_singnal_handler);
    signal(SIGHUP,proxy_singnal_handler);

    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        goto exit_error;
    }

    memset(proxy,0,sizeof(proxy_node));
    
    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto exit_error;
    }

    
    if(log_path!=NULL){
        proxy->log_file = fopen(log_path,"w");
    }
    proxy->fake = fake_mode;
    proxy->base = base;
    proxy->node_id = node_id;

    if(proxy_read_config(proxy,config_path)){
        goto exit_error;
    }
    // ensure the value is NULL at first
    proxy->hash_map=NULL;
    
    proxy->con_node = system_initialize(node_id,start_mode,config_path,update_state);

    if(NULL==proxy->con_node){
        paxos_log("cannot initialize node\n");
        goto exit_error;
    }
    pthread_create(&con_subt,NULL,t_consensus,proxy->con_node);
    proxy->listener =
        evconnlistener_new_bind(proxy->base,proxy_on_accept,
                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                -1,(struct sockaddr*)&proxy->sys_addr.p_addr,
                sizeof(proxy->sys_addr.p_sock_len));
    if(!proxy->listener){
        paxos_log("cannot set up the listener\n");
        goto exit_error;
    }
    LEAVE_FUNC
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
    ENTER_FUNC
    proxy->re_con = evtimer_new(proxy->base,
            reconnect_on_timeout,proxy);
    connect_consensus(proxy);
    event_base_dispatch(proxy->base);
    proxy_exit(proxy);
    LEAVE_FUNC
    return;
}

void proxy_exit(proxy_node* proxy){
    ENTER_FUNC
    LEAVE_FUNC
    return;
}

