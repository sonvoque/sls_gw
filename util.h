/*
|-------------------------------------------------------------------|
| HCMC University of Technology                                     |
| Telecommunications Departments                                    |
| Command Line Interface for Smart Lighting System (SLS)            |
| Version: 1.0                                                      |
| Author: sonvq@hcmut.edu.vn                                        |
| Date: 01/2017                                                     |
|-------------------------------------------------------------------|
*/
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
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include <mysql/mysql.h>
#include "sls.h"
#include "sls_cli.h"

void	encrypt_payload(cmd_struct_t *cmd);
void	decrypt_payload(cmd_struct_t *cmd);
void	generate_crc16(cmd_struct_t *cmd);
bool 	check_crc16(cmd_struct_t *cmd);
uint16_t hash( uint16_t a);
int  	convert_str2array(const char *hex_str, unsigned char *byte_array, int byte_array_max);
void 	convert_array2str(unsigned char *bin, unsigned int binsz, char **result);
void 	float2Bytes(float val,uint8_t* bytes_array);
float 	timedifference_msec(struct timeval t0, struct timeval t1);
uint16_t gen_random_num();