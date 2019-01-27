/*
|-------------------------------------------------------------------|
| HCMC University of Technology                                     |
| Telecommunications Departments                                    |
| Utility for Smart Lighting System (SLS)                           |
| Version: 1.0                                                      |
| Author: sonvq@hcmut.edu.vn                                        |
| Date: 01/2017                                                     |
|-------------------------------------------------------------------|
*/
#include "util.h"
#include "aes.h"
#include "sls.h"


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

/*---------------------------------------------------------------------------*/
void phex_16(uint8_t* data_16) { // in chuoi hex 16 bytes
    unsigned char i;
    for(i = 0; i < 16; ++i)
        printf("%.2x ", data_16[i]);
    printf("\n");
}

/*---------------------------------------------------------------------------*/
void phex_64(uint8_t* data_64) { // in chuoi hex 64 bytes
    unsigned char i;
    for(i = 0; i < 4; ++i) 
        phex_16(data_64 + (i*16));
    printf("\n");
}

/*---------------------------------------------------------------------------*/
// ma hoa 64 bytes
void encrypt_cbc(uint8_t* data_encrypted, uint8_t* data, uint8_t* key, uint8_t* iv) { 
    uint8_t data_temp[MAX_CMD_LEN];

    memcpy(data_temp, data, MAX_CMD_LEN);
    printf("\nData: \n");
    phex_64(data);

    AES128_CBC_encrypt_buffer(data_encrypted, data, 64, key, iv);

    //printf("\nData encrypted: \n");
    //phex_64(data_encrypted);
}

void scramble_data(uint8_t* data_encrypted, uint8_t* data, uint8_t* key) {
    int i;
    for (i=0; i<MAX_CMD_LEN; i++) {
        data_encrypted[i] = data[i] ^ key[i%4];
    }
} 

void descramble_data(uint8_t* data_decrypted, uint8_t* data_encrypted, uint8_t* key) {
    int i;
    for (i=0; i<MAX_CMD_LEN; i++) {
        data_decrypted[i] = data_encrypted[i] ^ key[i%4];
    }
} 

//-------------------------------------------------------------------------------------------
void encrypt_payload(cmd_struct_t *cmd, uint8_t* key) {    
    printf(" - Encryption process ... done \n");
    scramble_data((uint8_t *)cmd, (uint8_t *)cmd, key);

    //encrypt_cbc((uint8_t *)cmd, payload, key, iv);
}

/*---------------------------------------------------------------------------*/
void  decrypt_cbc(uint8_t* data_decrypted, uint8_t* data_encrypted, uint8_t* key, uint8_t* iv)  {
    uint8_t data_temp[MAX_CMD_LEN];

    printf("Decrypt Key = ");
    for (int i=0; i<=15; i++) {
        printf("0x%02X,", *(key+i));
    }
    printf("\n");


    memcpy(data_temp, data_encrypted, MAX_CMD_LEN);
    printf("\nData encrypted: \n");
    phex_64(data_encrypted);

    AES128_CBC_decrypt_buffer(data_decrypted+0,  data_encrypted+0,  16, key, iv);
    AES128_CBC_decrypt_buffer(data_decrypted+16, data_encrypted+16, 16, 0, 0);
    AES128_CBC_decrypt_buffer(data_decrypted+32, data_encrypted+32, 16, 0, 0);
    AES128_CBC_decrypt_buffer(data_decrypted+48, data_encrypted+48, 16, 0, 0);

    printf("Data decrypt: \n");
    phex_64(data_decrypted);
}


//-------------------------------------------------------------------------------------------
void decrypt_payload(cmd_struct_t *cmd, uint8_t* key) {
    //decrypt_cbc((uint8_t *)cmd, (uint8_t *)cmd, key, iv);
    descramble_data((uint8_t *)cmd, (uint8_t *)cmd, key);
    printf(" - Decryption process ... done \n");
}


//-------------------------------------------------------------------------------------------
unsigned short gen_crc16(uint8_t *data_p, unsigned short length) {
    unsigned char i;
    unsigned int data;
    unsigned int crc = 0xffff;
    uint8_t len;
    len = length;

    if (len== 0)
        return (~crc);
    do    {
        for (i=0, data=(unsigned int)0xff & *data_p++;
            i < 8; i++, data >>= 1) {
            if ((crc & 0x0001) ^ (data & 0x0001))
                crc = (crc >> 1) ^ POLY;
            else  crc >>= 1;
        }
    } while (--len);

    crc = ~crc;
    data = crc;
    crc = (crc << 8) | (data >> 8 & 0xff);

    return (crc);
}

//-------------------------------------------------------------------------------------------
void gen_crc_for_cmd(cmd_struct_t *cmd) {
    uint16_t crc16_check, i;
    uint8_t byte_arr[MAX_CMD_LEN-2];

    memcpy(&byte_arr, cmd, MAX_CMD_LEN-2);
    crc16_check = gen_crc16(byte_arr, MAX_CMD_LEN-2);
    cmd->crc = (uint16_t)crc16_check;
    printf("\n - Generate CRC16... done,  0x%04X \n", crc16_check);
    //for (i=0; i<MAX_CMD_DATA_LEN; i++) {
    //    printf("0x%02X \n", cmd->arg[i]);
    //}
    //printf("\n");
}


//-------------------------------------------------------------------------------------------
bool check_crc_for_cmd(cmd_struct_t *cmd) {
    uint16_t crc16_check;
    uint8_t byte_arr[MAX_CMD_LEN-2];

    memcpy(&byte_arr, cmd, MAX_CMD_LEN-2);
    crc16_check = gen_crc16(byte_arr, MAX_CMD_LEN-2);

    if (crc16_check == cmd->crc) {
        printf(" - Check CRC16... matched, CRC-cal = 0x%04X \n", crc16_check);
        return true;
    }
    else{
        printf(" - Check CRC16... failed, CRC-cal = 0x%04X; CRC-val =  0x%04X \n",crc16_check, cmd->crc);
        return false;        
    }        
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
void float2Bytes(float val, uint8_t* bytes_array){
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
    random1 = rand() % 255;
    random2 = rand() % 255;    
    return (random1<<8) | (random2);   
}

/*---------------------------------------------------------------------------*/
void gen_random_key_128(unsigned char* key){
    int i;
    unsigned char byte_array[16];
    for (i=0; i<16; i++) {
        byte_array[i] = gen_random_num() & 0xFF;
    }
    strcpy(key,byte_array); 
}




