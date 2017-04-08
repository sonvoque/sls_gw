/*
|-------------------------------------------------------------------|
| HCMC University of Technology                                     |
| Telecommunications Departments                                    |
| Gateway software for controlling the SLS                          |
| Version: 1.0                                                      |
| Author: sonvq@hcmut.edu.vn                                        |
| Date: 01/2017                                                     |
|                                                                   |
| compile: gcc -o main main.c $(mysql_config --libs --cflags)       |
|-------------------------------------------------------------------|*/

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
#include <time.h>

//#include <mysql/my_global.h>
#include <mysql/mysql.h>

#include "sls.h"
#include "sls_cli.h"


#define BUFSIZE 2048
#define MAXBUF  sizeof(cmd_struct_t)
#define SERVICE_PORT	21234	/* hard-coded port number */

#define clear() printf("\033[H\033[J")
#define TIME_OUT    1      //seconds

static  int     rev_bytes;
static  struct  sockaddr_in6 rev_sin6;
static  int     rev_sin6len;
static  char    rev_buffer[MAXBUF];
static  int     port;
static  char    dst_ipv6addr[50];
static  char    str_port[5];
static  char    cmd[20];
static  char    arg[32];

static node_db_struct_t node_db;
static node_db_struct_t node_db_list[100]; 

struct pollfd fd;
int res;

static  char    dst_ipv6addr_list[40][50];

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
static	int node_id, num_of_node, timeout_val;

static int read_node_list();
static void run_node_discovery();
static int ip6_send_cmd (int nodeid, int port);
static void init_main();
static bool is_cmd_of_gw(cmd_struct_t cmd);
static void process_gw_cmd(cmd_struct_t cmd);
static void finish_with_error(MYSQL *con);
static void get_db_row(MYSQL_ROW row, int i);
static void execute_sql_cmd(char *sql);
static void show_sql_db();
static void show_local_db();
static void update_sql_db();
static int execute_broadcasd_cmd(uint8_t cmd, int val);



struct timeval t0;
struct timeval t1;
float elapsed;

time_t rawtime;
struct tm * timeinfo;

MYSQL *con;
char *sql_cmd; 
static char sql_server_ipaddr[20] ="localhost";
static char sql_username[20]= "root";
static char sql_password[20]= "Son100480";
static char sql_db[20] = "sls_db";


/*------------------------------------------------*/
void finish_with_error(MYSQL *con) {
  fprintf(stderr, "%s\n", mysql_error(con));
  mysql_close(con);
  //exit(1);        
}

/*------------------------------------------------*/
void init_main() {
    timeout_val = TIME_OUT;
    show_sql_db();
}


/*------------------------------------------------*/
void get_db_row(MYSQL_ROW row, int i) {
    node_db_list[i].index       = atoi(row[0]);
    node_db_list[i].id          = atoi(row[1]);
    strcpy(node_db_list[i].ipv6_addr,row[2]);
    strcpy(node_db_list[i].connected,row[3]);
    node_db_list[i].num_req     = atoi(row[4]);
    node_db_list[i].num_rep     = atoi(row[5]);
    node_db_list[i].num_timeout = atoi(row[6]);
    node_db_list[i].last_cmd    = atoi(row[7]);

    strcpy(dst_ipv6addr_list[node_db_list[i].id], node_db_list[i].ipv6_addr);
    //printf("%02d | %02d | %s | %s | %02d | %02d | %02d | %02d | \n",nodedb->index , nodedb->id,nodedb->ipv6_addr, 
    //    nodedb->connected, nodedb->rx_cmd, nodedb->tx_rep,nodedb->num_timeout, nodedb->last_cmd );
}


/*------------------------------------------------*/
void update_sql_db() {

}

/*------------------------------------------------*/
void show_local_db() {
    int i;
    printf("\n");
    printf("|-----------------------------------------------LOCAL DATABASE----------------------------------------------------|\n");
    printf("| idx | node |             ipv6               | connect | req | rep | t_out | last_cmd |        last_seen         |\n");
    printf("|-----|------|--------------------------------|---------|-----|-----|-------|----------|--------------------------|\n");
    for(i = 0; i < num_of_node; i++) {
        if (i>0) 
            printf("| %3d | %4d | %30s |    %s    | %3d | %3d | %5d |   0x%02X   | %24s |\n",node_db_list[i].index , node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, 
                node_db_list[i].date_time );  
        else
            printf("| %3d | %4d | %30s |   *%s    | %3d | %3d | %5d |   0x%02X   | %24s |\n",node_db_list[i].index , node_db_list[i].id,
                node_db_list[i].ipv6_addr, node_db_list[i].connected, node_db_list[i].num_req, 
                node_db_list[i].num_rep, node_db_list[i].num_timeout, node_db_list[i].last_cmd, 
                node_db_list[i].date_time);  
    }
    printf("|-----------------------------------------------------------------------------------------------------------------|\n");
}


/*------------------------------------------------*/
void show_sql_db() {
    int i, num_fields;

    con = mysql_init(NULL);
    if (con == NULL) {
      finish_with_error(con);
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username, sql_password, sql_db, 0, NULL, 0) == NULL) {
        finish_with_error(con);
    }  

    if (mysql_query(con, "SELECT * FROM sls_db")) {
        finish_with_error(con);
    }
  
    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) {
        finish_with_error(con);
    }

    printf("\n");
    printf("|------------------------------SQL DATABASE-----------------------------------------|\n");
    num_fields = mysql_num_fields(result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))  { 
        for(i = 0; i < num_fields; i++) {
            if (i==0)
                printf("| %5s | ", row[i] ? row[i] : "NULL");
            else if (i==2)
                printf("%25s | ", row[i] ? row[i] : "NULL");
            else
                printf("%5s | ", row[i] ? row[i] : "NULL");
        }
        printf("\n");
    }
    printf("|-----------------------------------------------------------------------------------|\n");

    mysql_free_result(result);
    mysql_close(con);   
}

/*------------------------------------------------*/
int read_node_list(){
    bool sql_db_error;
    int     node;
    int num_fields;
    char    ipv6_addr[50];
   
    FILE *ptr_file;
    char buf[1000];

    num_of_node = 0;
    sql_db_error = false;
    con = mysql_init(NULL);
    printf("DATABASE: sls_db; MySQL client version: %s\n", mysql_get_client_info());
    if (con == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username,sql_password, sql_db,0,NULL,0) == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }  
    if (mysql_query(con, "SELECT * FROM sls_db")) {
        finish_with_error(con);
        sql_db_error = true;
    }
  
    MYSQL_RES *result = mysql_store_result(con);
    if (result == NULL) {
        finish_with_error(con);
        sql_db_error = true;
    }
    else {
        printf("Reading DB: sls_db......\n");
    }

    num_fields = mysql_num_fields(result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)))  { 
        //for(int i = 0; i < num_fields; i++)
            //printf("%s ", row[i] ? row[i] : "NULL");
        get_db_row(row, num_of_node);        
        num_of_node++;
    }

    mysql_free_result(result);
    mysql_close(con);   

    //sql_db_error = true;
    if (sql_db_error==false) {
        printf("SQL-DB succesfully reading node infor from SQL DB....\n");    
    }
    else {
        printf("SQL-DB error reading node infor from config file....\n");    
        num_of_node =0;
        ptr_file =fopen("node_list.txt","r");
        if (!ptr_file)
            return 1;
        while (fgets(buf,1000, ptr_file)!=NULL) {
            sscanf(buf,"%d %s",&node, ipv6_addr);
            strcpy(node_db_list[node].ipv6_addr, ipv6_addr);
            //printf("node = %d,   ipv6 = %s\n",node, node_db_list[node].ipv6_addr);
            num_of_node++;
            }
        fclose(ptr_file);
    }

    printf("I. READ NODE LIST... DONE. Num of nodes: %d\n",num_of_node);
    show_local_db();
    return 0;
}


/*------------------------------------------------*/
void execute_sql_cmd(char *sql) {
    con = mysql_init(NULL);
    if (con == NULL) {
      finish_with_error(con);
    }    
    if (mysql_real_connect(con, sql_server_ipaddr, sql_username, sql_password, sql_db, 0, NULL, 0) == NULL) {
        finish_with_error(con);
    }  

    if (mysql_query(con, sql)) {
        finish_with_error(con);
    }

    mysql_close(con);   
}
/*------------------------------------------------*/
void run_node_discovery(){
    char sql[200];
    int res, i;

    printf("II. RUNNING DISCOVERY PROCESS.....\n");
    for (i = 1; i < num_of_node; i++) {
        tx_cmd.type = MSG_TYPE_REQ;
        tx_cmd.cmd = CMD_RF_HELLO;
        tx_cmd.err_code = 0;
        res = ip6_send_cmd(i, SLS_NORMAL_PORT);
        if (res == -1) {
            printf(" - ERROR: discovery process \n");
        }
        else if (res == 0)   {
            printf(" - Node %d [%s] unavailable\n", i, node_db_list[i].ipv6_addr);
            sprintf(sql,"UPDATE sls_db SET connected='N' WHERE node_id=%d;",i);
            //sql_cmd = sql;
            printf("sql_cmd = %s\n", sql);
            execute_sql_cmd(sql);
        }
        else{
            printf(" - Node %d [%s] available\n", i, node_db_list[i].ipv6_addr);   
            sprintf(sql,"UPDATE sls_db SET connected='Y' WHERE node_id=%d;",i);
            //sql_cmd = sql;
            printf("sql_cmd = %s\n", sql);
            execute_sql_cmd(sql);
        }
    }
}
/*------------------------------------------------*/
void prepare_cmd() {
    tx_cmd.sfd = SFD;
    tx_cmd.seq ++;
}


/*------------------------------------------------*/
void print_cmd(cmd_struct_t command) {
    int i;
    printf("\nSFD=0x%02X; ",command.sfd);
    printf("node_id=%02d; ",command.len);
    printf("seq=%02d; ",command.seq);
    printf("type=0x%02X; ",command.type);
    printf("cmd=0x%02X; ",command.cmd);
    printf("err_code=0x%04X; \n",command.err_code); 
    printf("data=[");
    for (i=0;i<MAX_CMD_DATA_LEN;i++) 
        printf("%02X,",command.arg[i]);
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



/*------------------------------------------------*/
float timedifference_msec(struct timeval t0, struct timeval t1){
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}


/*------------------------------------------------*/
bool is_cmd_of_gw(cmd_struct_t cmd) {
    return (cmd.cmd==CMD_GET_GW_STATUS) ||
            (cmd.cmd==CMD_GW_HELLO) ||
            (cmd.cmd==CMD_GW_SHUTDOWN) ||
            (cmd.cmd==CMD_GW_TURN_ON_ALL) ||
            (cmd.cmd==CMD_GW_TURN_OFF_ALL) ||
            (cmd.cmd==CMD_GW_DIM_ALL) ||            
            (cmd.cmd==CMD_GW_SET_TIMEOUT);
}

/*------------------------------------------------*/
int execute_broadcasd_cmd(uint8_t cmd, int val) {
    int i, num_timeout, res, num_rep;
    uint16_t err_code;

    num_timeout=0;
    num_rep=0;
    printf("Executing broadcast command: 0x%02X, arg = %d ...\n", cmd, val);

    err_code = ERR_NORMAL;
    for (i = 1; i < num_of_node; i++) {
        /* prepare tx_cmd to send to RF nodes*/
        tx_cmd.type = MSG_TYPE_REQ;
        tx_cmd.cmd = cmd;     // CMD_LED_ON;
        tx_cmd.arg[0] = val;
        tx_cmd.err_code = 0;

        node_db_list[i].num_req++;
        res = ip6_send_cmd(i, SLS_NORMAL_PORT);
        if (res == -1) {
            printf(" - ERROR: broadcast process \n");
        }
        else if (res == 0)   {
            printf(" - Send cmd to node %d [%s] failed\n", i, node_db_list[i].ipv6_addr);
            num_timeout++;
            node_db_list[i].num_timeout++;
            err_code = ERR_BROADCAST_CMD;
        }
        else{
            printf(" - Send cmd to node %d [%s] succesful\n", i, node_db_list[i].ipv6_addr);   
            num_rep++;
            node_db_list[i].num_rep++;
            node_db_list[i].last_cmd = tx_cmd.cmd;            
        }
    }
    rx_reply.err_code = err_code;
    rx_reply.arg[0] = num_of_node-1;
    rx_reply.arg[1] = num_rep;
    rx_reply.arg[2] = num_timeout;
}


/*------------------------------------------------*/
void process_gw_cmd(cmd_struct_t cmd) {
    switch (cmd.cmd) {
        case CMD_GW_HELLO:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_SET_TIMEOUT:
            rx_reply.type = MSG_TYPE_REP;
            timeout_val = (*pi_cmdPtr).arg[0];
            break;
        
        case CMD_GET_GW_STATUS:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_SHUTDOWN:
            rx_reply.type = MSG_TYPE_REP;
            break;
        
        case CMD_GW_TURN_ON_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcasd_cmd(CMD_RF_LED_ON, 0);
            break;
        
        case CMD_GW_TURN_OFF_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcasd_cmd(CMD_RF_LED_OFF, 0);
            break;
        
        case CMD_GW_DIM_ALL:
            rx_reply.type = MSG_TYPE_REP;
            execute_broadcasd_cmd(CMD_RF_LED_DIM, cmd.arg[0]);
            break;
    }
}


int main(int argc, char* argv[]) {
//-------------------------------------------------------------------------------------------
// Khoi tao socket cho server de nhan du lieu
    int res;

    clear();
    init_main();

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

    
    /* read node list*/
    read_node_list();
    /* running discovery */
    run_node_discovery();

    for (;;) {        
        
        show_local_db();

		printf("\nIII. GATEWAY WAITING on PORT %d for COMMANDS\n", SERVICE_PORT);
		pi_recvlen = recvfrom(pi_fd, pi_buf, BUFSIZE, 0, (struct sockaddr *)&pi_remaddr, &pi_addrlen);
		if (pi_recvlen > 0) {
			printf("1. Received a COMMAND (%d bytes)\n", pi_recvlen);
  		pi_p = (char *) (&pi_buf); 
  		pi_cmdPtr = (cmd_struct_t *)pi_p;
  		pi_rx_reply = *pi_cmdPtr;
		
		node_id = pi_cmdPtr->len;
        rx_reply = *pi_cmdPtr;

		gettimeofday(&t0, 0);
        if (is_cmd_of_gw(*pi_cmdPtr)==true) {
            printf(" - Command Analysis: received GW command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            process_gw_cmd(*pi_cmdPtr);
        }
        else {  // not command for GW: send to node and wait for a reply
            printf(" - Command Analysis: received LED command, cmdID=0x%02X \n", pi_cmdPtr->cmd);
            tx_cmd = *pi_cmdPtr;
            gettimeofday(&t0, 0);
            res = ip6_send_cmd(node_id, SLS_NORMAL_PORT);
			gettimeofday(&t1, 0);
            elapsed = timedifference_msec(t0, t1);
            printf(" - GW-Cmd execution delay %.2f (ms)\n", elapsed);
            
            /*update local DB */
            node_db_list[node_id].num_req++;
            if (res == -1) {
                printf(" - ERROR: sending process \n");
            }
            else if (res == 0)   {
                //printf(" - Node %d [%s] unavailable\n", node_id, node_db_list[node_id].ipv6_addr);
                node_db_list[node_id].num_timeout++;
                rx_reply.err_code = ERR_TIME_OUT;
                rx_reply.type = MSG_TYPE_REP;
            }
            else{
                //printf(" - Node %d [%s] available\n", node_id, node_db_list[node_id].ipv6_addr);   
                node_db_list[node_id].num_rep++;
                node_db_list[node_id].last_cmd = tx_cmd.cmd;
            }            
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

int ip6_send_cmd(int nodeid, int port) {
    int sock;
    int status, i;
    struct addrinfo sainfo, *psinfo;
    struct sockaddr_in6 sin6;
    int sin6len;
    char buffer[MAXBUF];
    char str_app_key[32];
    unsigned char byte_array[16];
    char str_time[80];

    sin6len = sizeof(struct sockaddr_in6);

    print_cmd(tx_cmd);
    strcpy(dst_ipv6addr,node_db_list[nodeid].ipv6_addr);
    sprintf(str_port,"%d",port);

    //printf("ipv6_send: node = %d, ipv6= %s\n",nodeid, dst_ipv6addr);  
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
    res = poll(&fd, 1, timeout_val*1000); // timeout
    if (res == -1) {
        printf(" - ERROR: GW forwarding command \n");
    }
    else if (res == 0)   {
        printf(" - Time-out: GW forwarding command \n");
        rx_reply = tx_cmd;

        /*update local DB*/
        //node_db_list[nodeid].num_timeout++;
        strcpy(node_db_list[nodeid].connected,"N");
       
        time(&rawtime );
        timeinfo = localtime ( &rawtime );
        strftime(str_time,80,"%x - %I:%M:%S %p", timeinfo);
        strcpy (node_db_list[nodeid].date_time, str_time);
        update_sql_db();
    }
    else{
        // implies (fd.revents & POLLIN) != 0
        rev_bytes = recvfrom((int)sock, rev_buffer, MAXBUF, 0,(struct sockaddr *)(&rev_sin6), (socklen_t *) &rev_sin6len);
        if (rev_bytes>=0) {
            printf("3. Got REPLY (%d bytes):\n",rev_bytes);   
            p = (char *) (&rev_buffer); 
            cmdPtr = (cmd_struct_t *)p;
            rx_reply = *cmdPtr;

            print_cmd(rx_reply);
            /*update local DB*/
            //node_db_list[nodeid].tx_rep++;
            strcpy(node_db_list[nodeid].connected,"Y");
            time(&rawtime );
            timeinfo = localtime ( &rawtime );
            strftime(str_time,80,"%x - %I:%M:%S %p", timeinfo);
            strcpy (node_db_list[nodeid].date_time, str_time);
            update_sql_db();
        }
    }

    shutdown(sock, 2);
    close(sock); 
    
    // free memory
    freeaddrinfo(psinfo);
    psinfo = NULL;
    return res;
}