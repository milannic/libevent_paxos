#include "common-header.h"
#include "node.h"
#include "config.h"

int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static connection* peers_pool[MAX_ACCEPT_CONNECTIONS];

static unsigned current_connection = 3;


void on_read(struct bufferevent* bev,void* arg);
void on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg);
void on_error_cb(struct bufferevent* bev,short error,void* arg);


void on_error_cb(struct bufferevent* bev,short error,void *arg){
    if(error&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    return;
}

void on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    struct event_base* my_base = arg;
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        paxos_log("it has exceeded the predefined maximal concurrent connections\n");
        close(fd);
    }else{
        connection* new_connection = (connection*)malloc(sizeof(connection));
        new_connection->sock_id = fd;
        new_connection->sock_len= socklen;
        new_connection->peer_address =(struct sockaddr_in*) address;
        new_connection->base = my_base;
        struct bufferevent* new_buff_event = bufferevent_socket_new(my_base,new_connection->sock_id,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(new_buff_event,on_read,NULL,NULL,(void*)new_connection);
        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
        new_connection->my_buff_event = new_buff_event;
        peers_pool[current_connection++] = new_connection;
    }
};


void on_read(struct bufferevent* bev,void* arg){
//	connection* my_con = arg;
    char buf[1024];
    int n;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    paxos_log("there is %u bytes data in the buffer in total\n",(unsigned)len);
    while((n=evbuffer_remove(input,buf,sizeof(buf)))>0){
        printf("%s\n",buf);
    }
}


void usage(){
    paxos_log("Usage : -n NODE_ID -s [rb]\n");
}


int main(int argc, char **argv) {

    char* start_option = NULL;
    int node_id = -1;
    int c;

    while((c = getopt (argc,argv,"n:s:")) != -1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 's':
                start_option = optarg;
                if(*start_option!='b' && *start_option!='r'){
                    paxos_log("Unknown Start Option\n");
                    usage();
                    goto exit_error;
                }
                break;
            case '?':
                if(optopt == 'n'){
                    paxos_log("Option -n requires an argument");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 's'){
                    paxos_log("Option -s requires an argument");
                    usage();
                    goto exit_error;
                }
                break;
            default:
                paxos_log("Unknown Option %d \n",c);
                goto exit_error;
        }
    }

    if(argc<5 || optind>argc){
        usage();
        goto exit_error;
    }

    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto exit_error;
    }

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    node my_node;
    my_node.base = base;
    my_node.node_id = node_id;

    if(read_configuration_file(&my_node,"./nodes.cfg")){
        goto exit_error;
    }
    
    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&my_node.my_address.sin_addr,ipv4_address,INET_ADDRSTRLEN);

    debug_log("current node's ip address is %s:%d\n",ipv4_address,ntohs(my_node.my_address.sin_port));


    if(initialize_node(&my_node)){
        paxos_log("cannot initialize node\n");
        goto exit_error;
    }
    struct evconnlistener* listener = evconnlistener_new_bind(base,on_accept,(void*)base,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,(struct sockaddr*)&my_node.my_address,sizeof(my_node.my_address));

    if(!listener){
        paxos_log("cannot set up the listener\n");
        goto exit_error;
    }

	event_base_dispatch(base);
	event_base_free(base);

	return EXIT_SUCCESS;
exit_error:
    return EXIT_FAILURE;

}

