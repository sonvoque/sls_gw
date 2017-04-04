/*
Shell cmd for controlling the SLS
author Vo Que Son <sonvq@hcmut.edu.vn>
*/
//#include <mysql.h> 
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>


#include "sls.h"
#include "sls_cli.h"


#define BUFSIZE 2048
#define MAXBUF  sizeof(cmd_struct_t)
#define SERVICE_PORT	21234	/* hard-coded port number */

#define clear() printf("\033[H\033[J")
#define TIME_OUT    5      //seconds

static  int     rev_bytes;
static  struct  sockaddr_in6 rev_sin6;
static  int     rev_sin6len;
static  char    rev_buffer[MAXBUF];
static  int     port;
static  char    dst_ipv6addr[50];
static  char    str_port[5];
static  char    cmd[20];
static  char    arg[32];

struct pollfd fd;
int res;

static  char    dst_ipv6addr_list[20][50] ={"aaaa::212:4b00:5af:8406",
											"aaaa::212:4b00:5af:8570",
                                            "aaaa::212:4b00:5a9:8f83",
                                            "aaaa::212:4b00:5a9:8fd5",                                              
											"aaaa::212:4b00:5af:83f8",
											"aaaa::212:4b00:5af:851f",
											"aaaa::212:4b00:5af:8422",
											"aaaa::212:4b00:5af:84dd",
                                            "aaaa::212:4b00:5a9:8ff2",
                                            "aaaa::212:4b00:5a9:8f91", 
                                                                    };

static  cmd_struct_t  tx_cmd, rx_reply;
static  cmd_struct_t *cmdPtr;
static  char *p;

// bien cho server
static  cmd_struct_t  pi_tx_cmd, pi_rx_reply;
static  cmd_struct_t *pi_cmdPtr;
static  char *pi_p; 

/*prototype definition */
static void print_cmd();
static void prepare_cmd();
static	uint8_t node_id;

struct timeval t0;
struct timeval t1;
float elapsed;


/*------------------------------------------------*/
void prepare_cmd() {
    tx_cmd.sfd = SFD;
    tx_cmd.seq ++;
    tx_cmd.err_code = 0;  
    //tx_cmd.len = sizeof(tx_cmd);
    //tx_cmd.type = MSG_TYPE_REQ;
}


/*------------------------------------------------*/
void print_cmd(cmd_struct_t command) {
    int i;
    printf("SFD=0x%02X; ",command.sfd);
    printf("node_id=%02d; ",command.len);
    printf("seq=%02d; ",command.seq);
    printf("type=0x%02X; ",command.type);
    printf("cmd=0x%02X; ",command.cmd);
    printf("err_code=0x%04X; \n",command.err_code); 
    printf("data=[");
    for (i=0;i<MAX_CMD_DATA_LEN;i++) 
        printf("0x%02X,",command.arg[i]);
    printf("]\n");
}  


/*------------------------------------------------*/
int convert(const char *hex_str, unsigned char *byte_array, int byte_array_max) {
    int hex_str_len = strlen(hex_str);
    int i = 0, j = 0;
    // The output array size is half the hex_str length (rounded up)
    int byte_array_size = (hex_str_len+1)/2;
    if (byte_array_size > byte_array_max) {
        // Too big for the output array
        return -1;
    }

    if (hex_str_len % 2 == 1){
        // hex_str is an odd length, so assume an implicit "0" prefix
        if (sscanf(&(hex_str[0]), "%1hhx", &(byte_array[0])) != 1){
            return -1;
        }
        i = j = 1;
    }

    for (; i < hex_str_len; i+=2, j++){
        if (sscanf(&(hex_str[i]), "%2hhx", &(byte_array[j])) != 1){
            return -1;
        }
    }
    return byte_array_size;
}

void ip6_send_cmd (int nodeid, int port);

float timedifference_msec(struct timeval t0, struct timeval t1){
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}


int main(int argc, char* argv[]) {
//-------------------------------------------------------------------------------------------
// Khoi tao socket cho server de nhan du lieu
    struct sockaddr_in pi_myaddr;	/* our address */
	struct sockaddr_in pi_remaddr;	/* remote address */
	socklen_t pi_addrlen = sizeof(pi_remaddr);		/* length of addresses */
	int pi_recvlen;			/* # bytes received */
	int pi_fd;				/* our socket */
	int pi_msgcnt = 0;			/* count # of messages we received */
	unsigned char pi_buf[BUFSIZE];	/* receive buffer */

	/* create a UDP socket */
	if ((pi_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket\n");
		return 0;
	}

	/* bind the socket to any valid IP address and a specific port */
	memset((char *)&pi_myaddr, 0, sizeof(pi_myaddr));
	pi_myaddr.sin_family = AF_INET;
	pi_myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	pi_myaddr.sin_port = htons(SERVICE_PORT);

	if (bind(pi_fd, (struct sockaddr *)&pi_myaddr, sizeof(pi_myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}

    clear();
    for (;;) {
		printf("\nGateway is waiting on port %d for commands\n", SERVICE_PORT);
		pi_recvlen = recvfrom(pi_fd, pi_buf, BUFSIZE, 0, (struct sockaddr *)&pi_remaddr, &pi_addrlen);
		if (pi_recvlen > 0) {
			printf("1. Received a COMMAND (%d bytes)\n", pi_recvlen);
  		pi_p = (char *) (&pi_buf); 
  		pi_cmdPtr = (cmd_struct_t *)pi_p;
  		pi_rx_reply = *pi_cmdPtr;
		
		node_id = pi_cmdPtr->len;
        rx_reply = *pi_cmdPtr;

		gettimeofday(&t0, 0);
        if (pi_cmdPtr->cmd==CMD_GW_HELLO) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else if (pi_cmdPtr->cmd==CMD_GET_GW_STATUS) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else if (pi_cmdPtr->cmd==CMD_GW_SHUTDOWN) {
            printf(" - Command Analysis: received CW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else if (pi_cmdPtr->cmd==CMD_GW_TURN_ON_ALL) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else if (pi_cmdPtr->cmd==CMD_GW_TURN_OFF_ALL) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else if (pi_cmdPtr->cmd==CMD_GW_DIM_ALL) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            rx_reply.type = MSG_TYPE_REP;
        }
        else {  // not command for GW: send to node and wait for a reply
            printf(" - Command Analysis: received LED command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            tx_cmd = *pi_cmdPtr;
            gettimeofday(&t0, 0);
            ip6_send_cmd(node_id, 3000);
			gettimeofday(&t1, 0);
            elapsed = timedifference_msec(t0, t1);
            printf(" - GW-Cmd execution delay %.2f (ms)\n", elapsed);
        }
	}
	else
		printf("uh oh - something went wrong!\n");
	
    // send REPLY to sender NODE
	sprintf(pi_buf, "ACK %d", pi_msgcnt++);
	printf("4. Sending RESPONE to user \"%s\"\n", pi_buf);
	if (sendto(pi_fd, &rx_reply, sizeof(rx_reply), 0, (struct sockaddr *)&pi_remaddr, pi_addrlen) < 0)
		perror("sendto");
	}
    return 0;
}

void ip6_send_cmd(int nodeid, int port) {
    int sock;
    int status, i;
    struct addrinfo sainfo, *psinfo;
    struct sockaddr_in6 sin6;
    int sin6len;
    char buffer[MAXBUF];
    char str_app_key[32];
    unsigned char byte_array[16];

    sin6len = sizeof(struct sockaddr_in6);

    print_cmd(tx_cmd);
    strcpy(dst_ipv6addr,dst_ipv6addr_list[node_id]);
    sprintf(str_port,"%d",port);
  
    prepare_cmd();

    strtok(buffer, "\n");

    sock = socket(PF_INET6, SOCK_DGRAM,0);

    memset(&sin6, 0, sizeof(struct sockaddr_in6));
    sin6.sin6_port = htons(port);
    sin6.sin6_family = AF_INET6;
    sin6.sin6_addr = in6addr_any;

    status = bind(sock, (struct sockaddr *)&sin6, sin6len);
    if(-1 == status)
        perror("bind"), exit(1);

    memset(&sainfo, 0, sizeof(struct addrinfo));
    memset(&sin6, 0, sin6len);

    sainfo.ai_flags = 0;
    sainfo.ai_family = PF_INET6;
    sainfo.ai_socktype = SOCK_DGRAM;
    sainfo.ai_protocol = IPPROTO_UDP;
    status = getaddrinfo(dst_ipv6addr, str_port, &sainfo, &psinfo);
  
    status = sendto(sock, &tx_cmd, sizeof(tx_cmd), 0,(struct sockaddr *)psinfo->ai_addr, sin6len);
    if (status > 0)     {
        printf("\n2. Forward REQUEST (%d bytes) to [%s]:%s  ....done\n",status, dst_ipv6addr,str_port);        
    }
    else {
        printf("\n2. Forward REQUEST to [%s]:%s  ....ERROR\n",dst_ipv6addr,str_port);  
    }    

    /*wait for a reply */
    fd.fd = sock;
    fd.events = POLLIN;
    res = poll(&fd, 1, TIME_OUT*1000); // timeout
    if (res == 0)   {
        printf(" - Time-out: GW forwarding command \n");
        rx_reply = tx_cmd;
        rx_reply.err_code = ERR_TIME_OUT;
        rx_reply.type = MSG_TYPE_REP;
    }
    else if (res == -1) {
        printf(" - ERROR: GW forwarding command \n");
    }
    else{
        // implies (fd.revents & POLLIN) != 0
        rev_bytes = recvfrom((int)sock, rev_buffer, MAXBUF, 0,(struct sockaddr *)(&rev_sin6), (socklen_t *) &rev_sin6len);
        if (rev_bytes>=0) {
            printf("3. Got REPLY (%d bytes):\n",rev_bytes);   
            p = (char *) (&rev_buffer); 
            cmdPtr = (cmd_struct_t *)p;
            rx_reply = *cmdPtr;
        }
    }

    shutdown(sock, 2);
    close(sock); 
    
    // free memory
    freeaddrinfo(psinfo);
    psinfo = NULL;
}