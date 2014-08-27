/*
 * =====================================================================================
 *
 *       Filename:  client.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/26/2014 10:19:51 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../src/include/replica-sys/message.h"
#include "../src/include/util/common-header.h"


int main ( int argc, char *argv[] )
{
    char* server_address=NULL;
    int server_port;
    int c;
    int node_id;
    while((c=getopt(argc,argv,"n:s:p:"))!=-1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 's':
                server_address = optarg;
                break;
            case 'p':
                server_port = atoi(optarg);
                break;
            case '?':
                fprintf(stderr,"Option -%c needs requires an argument\n",optopt);
                break;
            default:
                fprintf(stderr,"Unknown Option %c\n",c);
                goto main_exit_error;
        }
    }
    if(argc<7 || optind>argc){
        fprintf(stderr,"Parameter Error\n");
        goto main_exit_error;
    }
    fprintf(stderr,"the node id is %d\n",node_id);
    fprintf(stderr,"the server address is %s\n",server_address);
    fprintf(stderr,"the server address is %d\n",server_port);
    struct sockaddr_in server_sock_addr;
    server_sock_addr.sin_family = AF_INET;
    server_sock_addr.sin_port = htons(server_port);
    inet_pton(AF_INET,server_address,&server_sock_addr.sin_addr);
    int my_socket = socket(AF_INET,SOCK_STREAM,0);
    if(connect(my_socket,(struct sockaddr*)&server_sock_addr,sizeof(server_sock_addr))!=0){
        fprintf(stderr,"%s",strerror(errno));
        goto main_exit_error;
    }
    return EXIT_SUCCESS;
main_exit_error:
    return EXIT_FAILURE;
}				/* ----------  end of function main  ---------- */
