#include "common-header.h"
#include "node.h"
#include "config.h"

int max_waiting_connections = MAX_ACCEPT_CONNECTIONS;

static connection* peers_pool[MAX_ACCEPT_CONNECTIONS];

static unsigned current_connection = 3;


void on_read(struct bufferevent* bev,void* arg);
void on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg);
void on_error_cb(struct bufferevent* bev,short error,void* arg);

static void* test_close(void* args){
	sleep(5);
	//int s_fd = *((int*)args);
	//close(s_fd);
	fprintf(stderr, "unused thread\n");
    return NULL;
};

void on_error_cb(struct bufferevent* bev,short error,void *arg){
    if(error&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    return;
}


void on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    struct event_base* my_base = arg;
    fprintf(stderr, "a new connection is established and the socket is %d\n",fd);
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        fprintf(stderr,"it has exceeded the predefined maximal concurrent connections\n");
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
    fprintf(stderr,"there is %u bytes data in the buffer in total\n",(unsigned)len);
    while((n=evbuffer_remove(input,buf,sizeof(buf)))>0){
        printf("%s\n",buf);
    }
}


int main(int argc, char **argv) {
    // set up base
	struct event_base* base = event_base_new();

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    node my_node;
    my_node.base = base;
    my_node.node_id = atoi("0");
    if(read_configuration_file(&my_node,"./nodes.cfg")){
        goto exit_error;
    }

    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&my_node.my_address.sin_addr,ipv4_address,INET_ADDRSTRLEN);

    fprintf(stderr,"current nodes group is %d\n",my_node.group_size);

    fprintf(stderr,"current node's ip address is %s:%d\n",ipv4_address,ntohs(my_node.my_address.sin_port));

    struct evconnlistener* listener = evconnlistener_new_bind(base,on_accept,(void*)base,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,(struct sockaddr*)&my_node.my_address,sizeof(my_node.my_address));
    if(!listener){
        fprintf(stderr, "cannot set up the listener\n");
        goto exit_error;
    }

	event_base_dispatch(base);
	event_base_free(base);
exit_succ:
	return EXIT_SUCCESS;
exit_error:
    return EXIT_FAILURE;
}

