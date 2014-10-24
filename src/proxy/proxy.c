/*
 * =====================================================================================
 *
 *       Filename:  proxy.c
 * *    Description:  
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
static struct timeval reconnect_timeval = {2,0};
static void* hack_arg=NULL;

//helper function
static hk_t gen_key(nid_t,nc_t,sec_t);

// consensus callback
static void update_state(int,void*,void*);
static void fake_update_state(int,void*,void*);
static void usage();

static void proxy_do_action(int fd,short whaDB_t,void* arg);
static void do_action_to_server(int data_size,void* data,void* arg);
static void do_action_connect(int data_size,void* data,void* arg);
static void do_action_send(int data_size,void* data,void* arg);
static void do_action_close(int data_size,void* data,void* arg);

static void proxy_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg);
static void consensus_on_event(struct bufferevent* bev,short ev,void* arg);
static void consensus_on_read(struct bufferevent*,void*);
static void connect_consensus(proxy_node*);
static void reconnect_on_timeout(int fd,short what,void* arg);


//socket pair callback function
//
//
static unsigned global_req_count=0;
static void server_side_on_read(struct bufferevent* bev,void* arg);
static void server_side_on_err(struct bufferevent* bev,short what,void* arg);
static void client_process_data(socket_pair*,struct bufferevent*,size_t);
static void client_side_on_read(struct bufferevent* bev,void* arg);
static void client_side_on_err(struct bufferevent* bev,short what,void* arg);

static req_sub_msg* build_req_sub_msg(hk_t s_key,counter_t counter,int type,size_t data_size,void* data);

//log component

//implementation

static hk_t gen_key(nid_t node_id,nc_t node_count,sec_t time){
    hk_t key = time;
    key |= ((hk_t)node_id<<52);
    key |= ((hk_t)node_count<<36);
    return key;
}

static void cross_view(proxy_node* proxy){
    ENTER_FUNC
    proxy->cur_rec = proxy->cur_rec>>32;
    proxy->cur_rec++;
    proxy->cur_rec = proxy->cur_rec<<32;
    proxy->cur_rec++;
    LEAVE_FUNC
};

static void proxy_do_action(int fd,short what,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    request_record* data = NULL;
    size_t data_size=0;
    debug_log("in do action,the current rec is %lu.\n",proxy->cur_rec);
    db_key_type cur_higest;
    pthread_mutex_lock(&proxy->lock);
    cur_higest = proxy->highest_rec;
    pthread_mutex_unlock(&proxy->lock);
    debug_log("in do action,the highest rec is %lu.\n",cur_higest);
    while(proxy->cur_rec<=cur_higest){
        data = NULL;
        data_size = 0;
        retrieve_record(proxy->db_ptr,sizeof(db_key_type),&proxy->cur_rec,&data_size,(void**)&data);
        if(NULL==data){
            cross_view(proxy);
        }else{
            do_action_to_server(data->data_size,data->data,proxy);
            proxy->cur_rec++;
        }
        debug_log("in do action,the current rec is %lu.\n",proxy->cur_rec);
    }
    evtimer_add(proxy->do_action,&proxy->action_period);
    LEAVE_FUNC
}

static void do_action_to_server(int data_size,void* data,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    proxy_msg_header* header = data;
    FILE* output = proxy->log_file;
    if(output==NULL){
        output = stdout;
    }
    struct timeval endtime;
    gettimeofday(&endtime,NULL);
    fprintf(output,"\n%lu.%lu,%lu.%lu,%lu.%lu,%lu.%lu\n",header->received_time.tv_sec,
            header->received_time.tv_usec,header->created_time.tv_sec,
                    header->created_time.tv_usec,endtime.tv_sec,endtime.tv_usec,
                  endtime.tv_sec,endtime.tv_usec);
    switch(header->action){
        case P_CONNECT:
            fprintf(output,"%lu,connects.\n",
                    header->connection_id);
            do_action_connect(data_size,data,arg);
            break;
        case P_SEND:
            fprintf(output,"%lu,sends data:%s.\n",
                    header->connection_id,((proxy_send_msg*)header)->data);
            do_action_send(data_size,data,arg);
            break;
        case P_CLOSE:
            fprintf(output,"%lu,closes.\n",
                    header->connection_id);
            do_action_close(data_size,data,arg);
            break;
        default:
            break;
    }
    LEAVE_FUNC
    return;
}
// when we have seen a connect method;
static void do_action_connect(int data_size,void* data,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    proxy_msg_header* header = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&header->connection_id,proxy->hash_map,ret);
    if(NULL==ret){
        ret = malloc(sizeof(socket_pair));
        memset(ret,0,sizeof(socket_pair));
        ret->key = header->connection_id;
        ret->counter = 0;
        ret->proxy = proxy;
        MY_HASH_SET(ret,proxy->hash_map);
    }
    if(ret->p_s==NULL){
        ret->p_s = bufferevent_socket_new(proxy->base,-1,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(ret->p_s,server_side_on_read,NULL,server_side_on_err,ret);
        bufferevent_enable(ret->p_s,EV_READ|EV_PERSIST|EV_WRITE);
        bufferevent_socket_connect(ret->p_s,(struct sockaddr*)&proxy->sys_addr.s_addr,proxy->sys_addr.s_sock_len);
    }else{
        debug_log("why there is an existing connection?\n");
    }
    LEAVE_FUNC
    return;
}
// when we have seen a send method,and since we have seen the same request
// sequence on all the machine, then this time, we must have already set up the
// connection with the server
static void do_action_send(int data_size,void* data,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    proxy_send_msg* msg = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&msg->header.connection_id,proxy->hash_map,ret);
    // this is error, TO-DO:error handler
    if(NULL==ret){
        goto do_action_send_exit;
    }else{
        if(NULL==ret->p_s){
            goto do_action_send_exit;
        }else{
            bufferevent_write(ret->p_s,msg->data,msg->data_size);
        }
    }
do_action_send_exit:
    LEAVE_FUNC
    return;
}

static void do_action_close(int data_size,void* data,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    proxy_close_msg* msg = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&msg->header.connection_id,proxy->hash_map,ret);
    // this is error, TO-DO:error handler
    if(NULL==ret){
        goto do_action_close_exit;
    }else{
        if(ret->p_s!=NULL){
            bufferevent_free(ret->p_s);
            ret->p_s = NULL;
        }
        if(ret->p_c!=NULL){
            bufferevent_free(ret->p_c);
            ret->p_c = NULL;
        }
    }
do_action_close_exit:
    LEAVE_FUNC
    return;
}

static void update_state(int data_size,void* data,void* arg){
    ENTER_FUNC
    debug_log("in update state,the current rec is %lu.\n",*((db_key_type*)(data)));
    proxy_node* proxy = arg;
    db_key_type* rec_no = data;
    pthread_mutex_lock(&proxy->lock);
    proxy->highest_rec = (proxy->highest_rec<*rec_no)?*rec_no:proxy->highest_rec;
    debug_log("in update state,the highest rec is %lu.\n",proxy->highest_rec);
    pthread_mutex_unlock(&proxy->lock);
    LEAVE_FUNC
    return;
}

//fake update state, we take out the data directly without re-
//visit the database
static void fake_update_state(int data_size,void* data,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    proxy_msg_header* header = data;
    FILE* output = proxy->log_file;
    if(output==NULL){
        output = stdout;
    }
    struct timeval endtime;
    gettimeofday(&endtime,NULL);
    fprintf(output,"\n%lu.%lu,%lu.%lu,%lu.%lu,%lu.%lu\n",header->received_time.tv_sec,
            header->received_time.tv_usec,header->created_time.tv_sec,
                    header->created_time.tv_usec,endtime.tv_sec,endtime.tv_usec,
                  endtime.tv_sec,endtime.tv_usec);
    switch(header->action){
        case P_CONNECT:
            fprintf(output,"%lu,connects.\n",
                    header->connection_id);
            break;
        case P_SEND:
            fprintf(output,"%lu,sends data:%s.\n",
                    header->connection_id,((proxy_send_msg*)header)->data);
            break;
        case P_CLOSE:
            fprintf(output,"%lu,closes.\n",
                    header->connection_id);
            break;
        default:
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
void consensus_on_event(struct bufferevent* bev,short ev,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    if(ev&BEV_EVENT_CONNECTED){
        debug_log("Connected to Consensus.\n");
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        int err = EVUTIL_SOCKET_ERROR();
		debug_log("%s.\n",evutil_socket_error_to_string(err));
        bufferevent_free(bev);
        proxy->con_conn = NULL;
        event_add(proxy->re_con,&reconnect_timeval);
    }
    LEAVE_FUNC
    return;
}

//void consensus_on_read(struct bufferevent* bev,void*);
void connect_consensus(proxy_node* proxy){
    ENTER_FUNC
    proxy->con_conn = bufferevent_socket_new(proxy->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(proxy->con_conn,NULL,NULL,consensus_on_event,proxy);
    bufferevent_enable(proxy->con_conn,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(proxy->con_conn,(struct sockaddr*)&proxy->sys_addr.c_addr,proxy->sys_addr.c_sock_len);
    LEAVE_FUNC
    return;
}


void reconnect_on_timeout(int fd,short what,void* arg){
    // this function should also test whether it should re-set 
    // a thread to run the consensus module but currently 
    // ignore this
    connect_consensus(arg);
}


static void server_side_on_read(struct bufferevent* bev,void* arg){
    ENTER_FUNC
    socket_pair* pair = arg;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    int cur_len = 0;
    void* msg = NULL;
    len = evbuffer_get_length(input);
    debug_log("there is %u bytes data in the buffer in total\n",
            (unsigned)len);
    // every time we just send 1024 bytes data to the client
    while(len>0){
        cur_len = (len>1024)?1024:len;
        msg = (void*)malloc(cur_len);
        if(NULL==msg){goto server_side_on_read_exit;}
        evbuffer_remove(input,msg,cur_len);
        if(pair->p_c!=NULL){
            bufferevent_write(pair->p_c,msg,cur_len);
        }
        free(msg);
        msg=NULL;
        len = evbuffer_get_length(input);
    }
server_side_on_read_exit:
    LEAVE_FUNC
    return;
}

// how to handle this? what if in a certain replica,whether we should replica
// this request to all the other replicas?

static void server_side_on_err(struct bufferevent* bev,short what,void* arg){
    ENTER_FUNC
    socket_pair* pair = arg;
    proxy_node* proxy = pair->proxy;
    struct timeval recv_time;
    if(what & BEV_EVENT_CONNECTED){
        debug_log("connection has established between %lu and the real server.\n",pair->key);
    }else if((what & BEV_EVENT_EOF) || ( what & BEV_EVENT_ERROR)){
        gettimeofday(&recv_time,NULL);
        req_sub_msg* close_msg = build_req_sub_msg(pair->key,pair->counter++,P_CLOSE,0,NULL);
        ((proxy_close_msg*)close_msg->data)->header.received_time = recv_time;
        if(NULL!=close_msg && NULL!=proxy->con_conn){
            bufferevent_write(proxy->con_conn,close_msg,REQ_SUB_SIZE(close_msg));
            free(close_msg);
        }
    }
    LEAVE_FUNC
    return;
}

static void client_process_data(socket_pair* pair,struct bufferevent* bev,size_t data_size){
    ENTER_FUNC
    void* msg_buf = (char*)malloc(CLIENT_MSG_HEADER_SIZE+data_size);
    req_sub_msg* con_msg=NULL;
    if(NULL==msg_buf){
        goto client_process_data_exit;
    }
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb,msg_buf,CLIENT_MSG_HEADER_SIZE+data_size);
    client_msg_header* msg_header = msg_buf;
    proxy_node* proxy = pair->proxy;
    struct timeval recv_time;
    switch(msg_header->type){
        case C_SEND_WR:
            gettimeofday(&recv_time,NULL);
            debug_log("received msg from client, the message is %s.\n",((client_msg*)msg_header)->data);
            con_msg = build_req_sub_msg(pair->key,pair->counter++,P_SEND,
                    msg_header->data_size,((client_msg*)msg_header)->data);
            ((proxy_send_msg*)con_msg->data)->header.received_time = recv_time;
            if(NULL!=con_msg && NULL!=proxy->con_conn){
                bufferevent_write(proxy->con_conn,con_msg,REQ_SUB_SIZE(con_msg));
            }
            break;
        default:
            debug_log("Unknown Client Msg Type %d\n",
                    msg_header->type);
            goto client_process_data_exit;
    }
client_process_data_exit:
    if(NULL!=msg_buf){free(msg_buf);}
    if(NULL!=con_msg){free(con_msg);}
    LEAVE_FUNC
    return;
};

// check whether there is enough data on the client evbuffer
static void client_side_on_read(struct bufferevent* bev,void* arg){
    ENTER_FUNC
    socket_pair* pair = arg;
    client_msg_header* header = NULL;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    debug_log("there is %u bytes data in the buffer in total\n",
            (unsigned)len);
    while(len>=CLIENT_MSG_HEADER_SIZE){
        header = (client_msg_header*)malloc(CLIENT_MSG_HEADER_SIZE);
        if(NULL==header){return;}
        evbuffer_copyout(input,header,CLIENT_MSG_HEADER_SIZE);
        int data_size = header->data_size;
        if(len>=(CLIENT_MSG_HEADER_SIZE+data_size)){
           client_process_data(pair,bev,data_size); 
        }else{
            break;
        }
        free(header);
        header=NULL;
        len = evbuffer_get_length(input);
    }
    if(NULL!=header){free(header);}
    LEAVE_FUNC
    return;
};

static void client_side_on_err(struct bufferevent* bev,short what,void* arg){
    ENTER_FUNC
    socket_pair* pair = arg;
    proxy_node* proxy = pair->proxy;
    struct timeval recv_time;
    if(what&BEV_EVENT_CONNECTED){
        debug_log("client %lu connects.\n",pair->key);

    }else if((what & BEV_EVENT_EOF) || ( what & BEV_EVENT_ERROR)){
        gettimeofday(&recv_time,NULL);
        req_sub_msg* close_msg = build_req_sub_msg(pair->key,pair->counter++,P_CLOSE,0,NULL);
        ((proxy_close_msg*)close_msg->data)->header.received_time = recv_time;
        if(NULL!=close_msg && NULL!=proxy->con_conn){
            bufferevent_write(proxy->con_conn,close_msg,REQ_SUB_SIZE(close_msg));
            free(close_msg);
        }
    }
    LEAVE_FUNC
    return;
}


static req_sub_msg* build_req_sub_msg(hk_t s_key,counter_t counter,int type,size_t data_size,void* data){
    req_sub_msg* msg = NULL;
    switch(type){
        case P_CONNECT:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+PROXY_CONNECT_MSG_SIZE);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = PROXY_CONNECT_MSG_SIZE;
            proxy_connect_msg* co_msg = (void*)msg->data;
            co_msg->header.action = P_CONNECT;
            co_msg->header.connection_id = s_key;
            co_msg->header.counter = counter;
            gettimeofday(&co_msg->header.created_time,NULL);
            break;
        case P_SEND:
            // consensus
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+sizeof(proxy_send_msg)+data_size);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = sizeof(proxy_send_msg)+data_size;
            //proxy
            proxy_send_msg* send_msg = (void*)msg->data;
            send_msg->header.action = P_SEND;
            send_msg->header.connection_id = s_key;
            send_msg->header.counter = counter;
            send_msg->data_size = data_size;
            memcpy(send_msg->data,data,data_size);
            gettimeofday(&send_msg->header.created_time,NULL);
            break;
        case P_CLOSE:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+PROXY_CONNECT_MSG_SIZE);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = PROXY_CONNECT_MSG_SIZE;
            proxy_close_msg* cl_msg = (void*)msg->data;
            cl_msg->header.action = P_CLOSE;
            cl_msg->header.connection_id = s_key;
            cl_msg->header.counter = counter;
            gettimeofday(&cl_msg->header.created_time,NULL);
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
    debug_log( "In proxy: a client connection is established.%d\n",fd);
    if(proxy->con_conn==NULL){
        debug_log("In proxy: We have lost the connection to consensus component now.\n");
        close(fd);
    }else{
        socket_pair* new_conn = malloc(sizeof(socket_pair));
        memset(new_conn,0,sizeof(socket_pair));
        struct timeval cur;
        gettimeofday(&cur,NULL);
        new_conn->key = gen_key(proxy->node_id,proxy->pair_count++,
                cur.tv_sec);
        new_conn->p_c = bufferevent_socket_new(proxy->base,fd,BEV_OPT_CLOSE_ON_FREE);
        new_conn->counter = 0;
        new_conn->proxy = proxy;
        bufferevent_setcb(new_conn->p_c,client_side_on_read,NULL,client_side_on_err,new_conn);
        bufferevent_enable(new_conn->p_c,EV_READ|EV_PERSIST|EV_WRITE);
        MY_HASH_SET(new_conn,proxy->hash_map);
        // connect operation should be consistent among all the proxies.
        struct timeval recv_time;
        gettimeofday(&recv_time,NULL);
        req_msg = build_req_sub_msg(new_conn->key,new_conn->counter++,P_CONNECT,0,NULL); 
        ((proxy_connect_msg*)req_msg->data)->header.received_time = recv_time;
        bufferevent_write(proxy->con_conn,req_msg,REQ_SUB_SIZE(req_msg));
    }
    if(req_msg!=NULL){
        free(req_msg);
    }
    return;
}

static void proxy_singnal_handler_sys(int sig){
    ENTER_FUNC
    proxy_node* proxy = hack_arg;
    if(sig&SIGTERM){
        debug_log("Node Proxy Received SIGTERM .Now Quit.\n");
        if(proxy->sub_thread!=0){
            pthread_kill(proxy->sub_thread,SIGQUIT);
            debug_log("wating consensus comp to quit.\n");
            pthread_join(proxy->sub_thread,NULL);
        }
    }
    event_base_loopexit(proxy->base,NULL);
    LEAVE_FUNC
    return;
}

static void proxy_singnal_handler(evutil_socket_t fid,short what,void* arg){
    ENTER_FUNC
    proxy_node* proxy = arg;
    if(what&EV_SIGNAL){
        debug_log("Node Proxy Received SIGTERM .Now Quit.\n");
        if(proxy->sub_thread!=0){
            pthread_kill(proxy->sub_thread,SIGQUIT);
            debug_log("wating consensus comp to quit.\n");
            pthread_join(proxy->sub_thread,NULL);
        }
    }
    event_base_loopexit(proxy->base,NULL);
    LEAVE_FUNC
    return;
}

proxy_node* proxy_init(int node_id,const char* start_mode,const char* config_path,
        const char* log_path,int fake_mode){
    ENTER_FUNC

    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        debug_log("cannot malloc the proxy.\n");
        goto proxy_exit_error;
    }

    memset(proxy,0,sizeof(proxy_node));

    if(pthread_mutex_init(&proxy->lock,NULL)){
        debug_log("cannot init the lock.\n");
        goto proxy_exit_error;
    }
    
    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto proxy_exit_error;
    }

//    proxy->log_file = NULL; 
    if(log_path!=NULL){
        proxy->log_file = fopen(log_path,"w");
    }
    proxy->fake = fake_mode;
    proxy->base = base;
    proxy->node_id = node_id;
    proxy->action_period.tv_sec = 0;
    proxy->action_period.tv_usec = 1000000;
    proxy->do_action = evtimer_new(proxy->base,proxy_do_action,(void*)proxy);
    if(proxy_read_config(proxy,config_path)){
        goto proxy_exit_error;
    }
    // Open database 
    proxy->db_ptr = initialize_db(proxy->db_name,0);
    // if we cannot create the database and the proxy runs not in the fake mode, then we will fail 
    if(proxy->db_ptr==NULL && !proxy->fake){
        goto proxy_exit_error;
    }
    // ensure the value is NULL at first
    proxy->hash_map=NULL;
    proxy->listener = evconnlistener_new_bind(proxy->base,proxy_on_accept,
                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                -1,(struct sockaddr*)&proxy->sys_addr.p_addr,
                sizeof(proxy->sys_addr.p_addr));
    if(proxy->listener==NULL){
        debug_log("cannot set up the listener\n");
        goto proxy_exit_error;
    }


    if(proxy->fake){ 
        proxy->con_node = system_initialize(node_id,start_mode,
                config_path,1,fake_update_state,proxy->db_ptr,proxy);
    }else{
        proxy->con_node = system_initialize(node_id,start_mode,
                config_path,0,update_state,NULL,proxy);
    }

    if(NULL==proxy->con_node){
        debug_log("cannot initialize node\n");
        goto proxy_exit_error;
    }

    pthread_create(&proxy->sub_thread,NULL,t_consensus,proxy->con_node);

    //proxy->sig_handler = evsignal_new(proxy->base,SIGTERM,proxy_singnal_handler,proxy);
    //evsignal_add(proxy->sig_handler,NULL);
    hack_arg = proxy;
    signal(SIGTERM,proxy_singnal_handler_sys);

    LEAVE_FUNC
	return proxy;

proxy_exit_error:
    if(NULL!=proxy){
        if(NULL!=proxy->con_node){
            // to do
            if(proxy->sub_thread!=0){
                pthread_kill(proxy->sub_thread,SIGQUIT);
                debug_log("wating consensus comp to quit.\n");
                pthread_join(proxy->sub_thread,NULL);
            }
        }
        free(proxy);
    }
    ERR_LEAVE_FUNC
    return NULL;
}

void proxy_run(proxy_node* proxy){
    ENTER_FUNC
    proxy->re_con = evtimer_new(proxy->base,
            reconnect_on_timeout,proxy);
    connect_consensus(proxy);
    evtimer_add(proxy->do_action,&proxy->action_period);
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

