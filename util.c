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
#include "util.h"

//-------------------------------------------------------------------------------------------
void encrypt_payload(cmd_struct_t *cmd) {
	cmd_struct_t tem;
	uint8_t payload[16];
    printf("\n - Encryption process ... done \n");
    tem = *cmd;
}

//-------------------------------------------------------------------------------------------
void decrypt_payload(cmd_struct_t *cmd) {
    printf(" - Decryption process ... done \n");
}

//-------------------------------------------------------------------------------------------
void generate_crc16(cmd_struct_t *cmd) {
    printf(" - Generate CRC16 process ... done \n");
}

//-------------------------------------------------------------------------------------------
bool check_crc16(cmd_struct_t *cmd) {
    printf(" - Check CRC16 process ... done\n");

    return true;
}

/*------------------------------------------------*/
uint16_t hash(uint16_t a) {
    uint32_t tem;
    tem =a;
    tem = (a+0x7ed55d16) + (tem<<12);
    tem = (a^0xc761c23c) ^ (tem>>19);
    tem = (a+0x165667b1) + (tem<<5);
    tem = (a+0xd3a2646c) ^ (tem<<9);
    tem = (a+0xfd7046c5) + (tem<<3);
    tem = (a^0xb55a4f09) ^ (tem>>16);
   return tem & 0xFFFF;
}

/*------------------------------------------------*/
void convert_array2str(unsigned char *bin, unsigned int binsz, char **result) {
    char hex_str[]= "0123456789ABCDEF";
    unsigned int  i;

    *result = (char *)malloc(binsz * 2 + 1);
    (*result)[binsz * 2] = 0;

    if (!binsz)
        return;

    for (i = 0; i < binsz; i++) {
        (*result)[i * 2 + 0] = hex_str[(bin[i] >> 4) & 0x0F];
        (*result)[i * 2 + 1] = hex_str[(bin[i]     ) & 0x0F];
    }  
}

/*------------------------------------------------*/
int convert_str2array(const char *hex_str, unsigned char *byte_array, int byte_array_max) {
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

/*---------------------------------------------------------------------------
//float float_example = 1.11;
//uint8_t bytes[4];
//float2Bytes(float_example, &bytes[0]);
/*---------------------------------------------------------------------------*/
void float2Bytes(float val,uint8_t* bytes_array){
  union {
    float float_variable;
    uint8_t temp_array[4];
  } u;
  u.float_variable = val;
  memcpy(bytes_array, u.temp_array, 4);
}

/*---------------------------------------------------------------------------*/
uint16_t gen_random_num() {
    uint16_t random1, random2;
    random1 = rand();
    random2 = rand();    
    return (random1<<8) | (random2);   
}