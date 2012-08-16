
#define _FILE_OFFSET_BITS 64

#include <string.h>
#include <stdio.h>
#include "assert.h"
#include <malloc.h>

#include <map>
#include <vector>
#include <algorithm>

using std::map;
using std::vector;

typedef int I_32;
typedef short I_16;
typedef signed char I_8; /* chars can be unsigned */
typedef unsigned int U_32;
typedef unsigned long long U_64;
typedef unsigned short U_16;
typedef unsigned char U_8;

                
typedef map<unsigned int, U_64> objectOffsetMap;
typedef map<U_32, U_8> InitOnlyMap;

typedef struct Access_Info {
    U_32 access_count;
    U_32 alloc_count;
    U_32 access_tid;
    U_16 alloc_tid;
    U_16 write_count;
} Access_Info;

#define ACCESS_INFO_NUM (1<<23) // the number of access info in every local access pool
#define ACCESS_INFO_SIZE  sizeof(Access_Info)

typedef struct ILInfo{
    U_32 alloc_count;
    U_32 last_tid;
    U_32 next_acc_count;
    U_32 next_tid;
    U_16 alloc_tid;
    U_16 initOnly;	
}ILInfo;

typedef struct ILInfo_Pool{
    ILInfo *pool;
    U_32 used_num;
    U_64 pro_num;
} ILInfo_Pool;


long long thread_local_accesses;
long long init_only_accesses;
long long other_accesses;

long long thread_local_interleaving;
long long init_only_interleaving;
long long other_interleaving;


#define ILInfo_SIZE sizeof(ILInfo)
#define ILInfo_NUM 10000
#define NEXT_TID_OFFSET  (2 * sizeof(U_32))

#define FILE_NUM 10

static ILInfo_Pool  ILInfo_pool;

void init_ILInfo_pool()
{
    unsigned int ILInfo_pool_size = ILInfo_SIZE * ACCESS_INFO_NUM;
    void * pool = malloc(ILInfo_pool_size);
    if(pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc ILInfo_pool!\n");
        assert(0);
    }
    memset(pool, 0, ILInfo_pool_size);

    ILInfo_pool.pool = (ILInfo *)pool;
    ILInfo_pool.used_num = 0;
    ILInfo_pool.pro_num = 0;
	
}

void reset_ILInfo_pool()
{
	memset(ILInfo_pool.pool, 0, ILInfo_SIZE * ACCESS_INFO_NUM);
	ILInfo_pool.used_num = 0;
    	ILInfo_pool.pro_num = 0;
}

void destruct_ILInfo_pool()
{
	free(ILInfo_pool.pool);
}


/****************************************************************************************/
typedef struct InterInfo{
    U_32 alloc_count;
    U_32 last_tid;
    U_32 next_acc_count;
    U_32 next_tid;
    U_16 alloc_tid;	
}InterInfo;

typedef struct InterInfo_Pool{
    InterInfo *pool;
    U_32 used_num;
    U_64 pro_num;
} InterInfo_Pool;

#define InterInfo_SIZE sizeof(InterInfo)
#define InterInfo_NUM 10000
#define NEXT_TID_OFFSET  (2 * sizeof(U_32))


static InterInfo_Pool  InterInfo_pool;

void init_InterInfo_pool()
{
    unsigned int InterInfo_pool_size = InterInfo_SIZE * ACCESS_INFO_NUM;
    void * pool = malloc(InterInfo_pool_size);
    if(pool == NULL){
        fprintf(stderr,"[xlc]: Could NOT malloc InterInfo_pool!\n");
        assert(0);
    }
    memset(pool, 0, InterInfo_pool_size);

    InterInfo_pool.pool = (InterInfo *)pool;
    InterInfo_pool.used_num = 0;
    InterInfo_pool.pro_num = 0;
	
}

void reset_InterInfo_pool()
{
	memset(InterInfo_pool.pool, 0, InterInfo_SIZE * ACCESS_INFO_NUM);
	InterInfo_pool.used_num = 0;
    	InterInfo_pool.pro_num = 0;
}

void destruct_InterInfo_pool()
{
	free(InterInfo_pool.pool);
}

/***************************************************************************************/
#define THREAD_NUM 64
#define CHUNK_NUM 8

typedef struct Thread_Map {
    U_32 thread_global_id;
    U_32 pthread_id;
    U_32 thread_assigned_id;
} Thread_Map;

typedef map<unsigned int, unsigned int> pthreadidTidMap;

pthreadidTidMap pthreadid_tid_map;

void get_tid_map()
{
	char name[40];

	sprintf(name, "THREAD_MAP.log");
	printf(">>>>>>>>>>>Get TID Mapping : %s>>>>>>>>>>>\n", name);
	FILE *logFile = fopen64(name, "r");

	Thread_Map tmap;

	fseeko64(logFile, (U_64)0, SEEK_SET);
	while(!feof(logFile))
	{
		int num = fread((void *)&tmap, sizeof(Thread_Map), 1, logFile);
		if (num)
		{
			pthreadid_tid_map[tmap.pthread_id] = tmap.thread_assigned_id;
		}
	}

}


void split_file();
void process_interleaving();
void process_init_only();
void merge_interleaving();

int main()
{
        thread_local_accesses = 0;
        init_only_accesses = 0;
        other_accesses = 0;

        thread_local_interleaving = 0;
        init_only_interleaving = 0;
        other_interleaving = 0;

	//first, split the log into smaller files
	get_tid_map();
        split_file();

	process_interleaving();
//	process_init_only();
	merge_interleaving();


        printf("thread_local_accesses in interleaving : %lld\n", thread_local_accesses);
        printf("init_only_accesses in interleaving : %lld\n", init_only_accesses);
        printf("other_accesses in interleaving : %lld\n", other_accesses);

        printf("thread_local_interleaving in interleaving : %lld\n", thread_local_interleaving);
        printf("init_only_interleaving in interleaving : %lld\n", init_only_interleaving);
        printf("other_interleaving in interleaving : %lld\n", other_interleaving);

}

void split_file()
{
 	//pass1.split the log into several small file based on the alloc tid and alloc count
	printf(">>>>>>>>>>>Split File Now>>>>>>>>>>>\n");
	FILE *logFile = fopen64("ORDER_RECORD.log", "r");
	FILE *out[THREAD_NUM][CHUNK_NUM];
	FILE *outRemaining[CHUNK_NUM];
	int i,j;

	if(logFile == NULL){
        	fprintf(stderr, "[xlc]: could NOT open file: ORDER_RECORD.log\n");
	        assert(0);
    	}

	char name[20];
	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			sprintf(name, "record_%d_%d.tmp", i, j);
			out[i][j] = fopen64(name, "w+");
			if(out[i][j] == NULL)
			{
				fprintf(stderr, "[xlc]: could NOT open file :%s\n", name);
			}
		}
	}	

	for (i = 0 ; i < CHUNK_NUM ; i ++)
	{
		sprintf(name, "record_rem_%d.tmp", i);
		outRemaining[i] = fopen64(name, "w+");
		if(outRemaining[i] == NULL)
		{
			fprintf(stderr, "[xlc]: could NOT open file :%s\n", name);
		}
	}




	Access_Info * access_pool = (Access_Info *)malloc(ACCESS_INFO_SIZE * ACCESS_INFO_NUM);
	
	U_64 cur_pos = 0;
	fseeko64(logFile, (U_64)0, SEEK_SET);	

	while(!feof(logFile)){
		//fseeko64(logFile, (U_64)cur_pos, SEEK_SET);	
		int num = fread((void *)access_pool, ACCESS_INFO_SIZE, ACCESS_INFO_NUM, logFile);
		printf("[xlc]: read %d access info from file\n", num);	
		
		for(i = 0; i < num; i++){
			Access_Info *record = &access_pool[i];
			
			assert(pthreadid_tid_map.find(record->access_tid) != pthreadid_tid_map.end());
			record->access_tid = pthreadid_tid_map[record->access_tid];
			
			if (record->alloc_tid < THREAD_NUM)
			{
				int tid = record->alloc_tid;
				int chunkid = record->alloc_count % CHUNK_NUM;
			
				fwrite((char *)record, ACCESS_INFO_SIZE, 1, out[tid][chunkid]);
			}
			else
			{
			         assert(0);
				int chunkid = record->alloc_count % CHUNK_NUM;
			
				fwrite((char *)record, ACCESS_INFO_SIZE, 1, outRemaining[chunkid]);
			}
		}
		cur_pos += num * ACCESS_INFO_SIZE;
	
	}

	free(access_pool);
	fclose(logFile);
	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			fflush(out[i][j]);
			fclose(out[i][j]);
		}
	}

	for (i = 0 ; i < CHUNK_NUM ; i ++)
	{
	         fflush(outRemaining[i]);
		fclose(outRemaining[i]);
	}
}

void process_interleaving()
{
//process each file
	Access_Info *access_info = (Access_Info *)malloc(ACCESS_INFO_SIZE * ACCESS_INFO_NUM);//used to compare
	assert(access_info);
	init_ILInfo_pool();

	int buf_size = 10000;
	ILInfo * in_buffer = (ILInfo *)malloc(ILInfo_SIZE * buf_size);
	ILInfo * out_buffer = (ILInfo *)malloc(ILInfo_SIZE * buf_size);
	
	char in_name[30];
	char out_name[30];
	char initOnly_name[40];
	char inter_out_name[30];
	for(int t_num = 0; t_num < THREAD_NUM; t_num++)
	{
		for (int c_num= 0 ; c_num < CHUNK_NUM ; c_num ++)
		{
			sprintf(in_name, "record_%d_%d.tmp", t_num, c_num);
			sprintf(out_name, "interOrig_%d_%d.tmp", t_num, c_num);
			sprintf(initOnly_name, "initOnly_%d_%d.init", t_num, c_num);
			sprintf(inter_out_name, "inter_%d_%d.tmp", t_num, c_num);
			
			FILE *logFile = fopen64(in_name, "r");
			FILE *out = fopen64(out_name, "w+");
			FILE *modifyHandle = fopen64(out_name, "w+");
			FILE *init_only_file = fopen64(initOnly_name, "w+");
			FILE *inter_out_file = fopen64(inter_out_name, "w+");
	
			if(logFile == NULL || out == NULL || modifyHandle == NULL || init_only_file == NULL){
		        	    fprintf(stderr, "[xlc]: could NOT open file\n");
			    assert(0);
		    	}

			printf("process file %s\n", in_name);

			memset(access_info, 0, ACCESS_INFO_SIZE * ACCESS_INFO_NUM);
			reset_ILInfo_pool();
			
			objectOffsetMap objOfstMap;
			InitOnlyMap initOnlyMap;
			
			U_64 acc_cur_pos = 0;
			fseeko64(logFile, (U_64)acc_cur_pos, SEEK_SET);	
		    
			while(!feof(logFile)){
				int num = fread((void *)access_info, ACCESS_INFO_SIZE, ACCESS_INFO_NUM, logFile);

				for(int i = 0; i < num; i++){
					Access_Info *record = &access_info[i];
					assert((record->alloc_count % CHUNK_NUM) == c_num);

					//process init only
					InitOnlyMap::iterator initOnly_iter;
					initOnly_iter = initOnlyMap.find(record->alloc_count);
					if(initOnly_iter == initOnlyMap.end()){// first access
						initOnlyMap[record->alloc_count] = 2;
					}
					else if(initOnlyMap[record->alloc_count] != 1){
						if(record->write_count != 0){
							initOnlyMap[record->alloc_count] = 1;
						}
                                                else{
                                                        initOnlyMap[record->alloc_count] = 0;
                                                }
					}
					//end process init only

					objectOffsetMap::iterator iter;
				    	iter = objOfstMap.find(record->alloc_count);
					if(iter == objOfstMap.end()){
						objOfstMap[record->alloc_count] = acc_cur_pos + i;
					}
					else{
						
						U_64 last_offset = objOfstMap[record->alloc_count];
						if(last_offset >= ILInfo_pool.pro_num){//the last record is still in the pool
							U_64 pool_offset = last_offset - ILInfo_pool.pro_num;
							ILInfo *last_record = &(ILInfo_pool.pool[pool_offset]);
							assert(last_record->alloc_count == record->alloc_count);
							assert(last_record->last_tid != record->access_tid);
							last_record->next_tid = record->access_tid;
							last_record->next_acc_count = record->access_count;
						}
						else{//the last record is swap into the file
							U_64 file_offset = last_offset * ILInfo_SIZE + NEXT_TID_OFFSET;
							fseeko64(modifyHandle, (U_64)file_offset, SEEK_SET);
							fwrite((char *)&record->access_count, sizeof(U_32), 1, modifyHandle);
							fwrite((char *)&record->access_tid, sizeof(U_32), 1, modifyHandle);

							//for debug
							ILInfo last_record;
							fseeko64(modifyHandle, (U_64)(last_offset * ILInfo_SIZE), SEEK_SET);
							fread((void *)&last_record, ILInfo_SIZE, 1, modifyHandle);
							assert(last_record.alloc_count == record->alloc_count);
							assert(last_record.last_tid != record->access_tid);
							//end
						}
						objOfstMap[record->alloc_count] = acc_cur_pos + i;
					}

					ILInfo_pool.pool[i].alloc_tid = record->alloc_tid;
					ILInfo_pool.pool[i].alloc_count = record->alloc_count;
					ILInfo_pool.pool[i].last_tid = record->access_tid;
					ILInfo_pool.pool[i].next_tid = 0;
					ILInfo_pool.pool[i].next_acc_count = 0;
					ILInfo_pool.used_num++;
					
				}

				fwrite((char *)ILInfo_pool.pool, ILInfo_SIZE, ILInfo_pool.used_num, out);
				ILInfo_pool.pro_num  += ILInfo_pool.used_num;
				ILInfo_pool.used_num = 0;
				
				acc_cur_pos += num;
			}

			fflush(out);
			fflush(modifyHandle);

			/*****************process init only******************************/
			fseeko64(out, (U_64)0, SEEK_SET);
			InitOnlyMap::iterator iter;
			int out_buf_cnt = 0;
			
			while(!feof(out))
			{
				int num = fread((void *)in_buffer, ILInfo_SIZE, buf_size, out);
				for(int k = 0; k < num; k++){
					ILInfo *interleaving = &in_buffer[k];
					assert((interleaving->alloc_count % CHUNK_NUM) == c_num);
					iter = initOnlyMap.find(interleaving->alloc_count);
					assert(iter != initOnlyMap.end());
					if(((U_8)iter->second) == 0){
						interleaving->initOnly = 1;
//                                                printf("count initOnly number.\n");
                                                init_only_accesses += interleaving->next_acc_count;

                                                init_only_interleaving ++;
					}
                                        else if(((U_8)iter->second) == 2){
                                                assert(interleaving->next_acc_count == 0);
                                        }
                                        else
                                        {
//                                                printf("here count others.\n");
                                                other_accesses += interleaving->next_acc_count;
                                                other_interleaving ++;
                                        }
					
					memcpy((void *)&(out_buffer[out_buf_cnt]), (void *)interleaving, ILInfo_SIZE);
					out_buf_cnt++;
					if(out_buf_cnt == buf_size){
						fwrite((char *)out_buffer, ILInfo_SIZE, out_buf_cnt, inter_out_file);
						out_buf_cnt = 0;
					}
				}
			}
			//write the reset out
			fwrite((char *)out_buffer, ILInfo_SIZE, out_buf_cnt, inter_out_file);
			
			fflush(inter_out_file);
			fclose(inter_out_file);
			/***************************************************************/

			//write out the init only object
			for(InitOnlyMap::iterator it = initOnlyMap.begin(); it != initOnlyMap.end(); it++) 
                           {
                                U_32 alloc_count = (U_32)it->first;
                                U_8 flag = (U_8)it->second;
			     if(flag == 0 || flag == 2){
                                     fwrite((char *)&alloc_count, sizeof(U_32), 1, init_only_file);
                                     fwrite((char *)&flag, sizeof(U_8), 1, init_only_file);
			     }
                            }
			//end
			
			fclose(logFile);	
			fclose(out);
			fclose(modifyHandle);
			fflush(init_only_file);
			fclose(init_only_file);
			remove(in_name);
			remove(out_name);
		}
	}

	free(in_buffer);
	free(out_buffer);
	free(access_info);
	destruct_ILInfo_pool();
}

/*
void process_init_only()
{
	printf("process init only!!!\n");
	char inter_in_name[40];
	char inter_out_name[40];
	char init_in_name[40];

         int buf_size = 10000;
	ILInfo * in_buffer = (ILInfo *)malloc(ILInfo_SIZE * buf_size);
	ILInfo * out_buffer = (ILInfo *)malloc(ILInfo_SIZE * buf_size);
	U_32 buf[buf_size];
	
	for(int i = 0; i < THREAD_NUM; i++){
		for(int j = 0 ; j < CHUNK_NUM ; j ++){
			sprintf(inter_in_name, "interOrig_%d_%d.tmp", i, j);
			sprintf(inter_out_name, "inter_%d_%d.tmp", i, j);
			sprintf(init_in_name, "initOnly_%d_%d.init", i, j);
			
			FILE *inter_in_file = fopen64(inter_in_name, "r");
			FILE *inter_out_file = fopen64(inter_out_name, "w+");
			FILE *init_in_file = fopen64(init_in_name, "r");
			if(inter_in_file == NULL || inter_out_file == NULL || init_in_file == NULL)
			{
				fprintf(stderr, "[xlc]: could NOT open file \n");
			}
			printf("Processing %s\n", inter_in_name);
			
			//1 step 1.init vector
			vector<U_32> initOnlyVector;
			fseeko64(init_in_file, 0, SEEK_SET);
			while(!feof(init_in_file))
			{
				int num = fread((void *)&buf, sizeof(U_32), buf_size, init_in_file);
				for(int k = 0; k < num; k++){
					initOnlyVector.push_back(buf[k]);
				}
			}

			int out_buf_cnt = 0;
			vector<U_32>::iterator iter;
			while(!feof(inter_in_file))
			{
				int num = fread((void *)in_buffer, ILInfo_SIZE, buf_size, inter_in_file);
				for(int k = 0; k < num; k++){
					ILInfo *interleaving = &in_buffer[k];
					iter = find(initOnlyVector.begin(), initOnlyVector.end(), interleaving->alloc_count);
					if(iter != initOnlyVector.end()){
						interleaving->initOnly = 1;
					}
					memcpy((void *)&(out_buffer[out_buf_cnt]), (void *)interleaving, ILInfo_SIZE);
					out_buf_cnt++;
					if(out_buf_cnt == buf_size){
						fwrite((char *)out_buffer, ILInfo_SIZE, out_buf_cnt, inter_out_file);
						out_buf_cnt = 0;
					}
				}
			}
			//write the reset out
			fwrite((char *)out_buffer, ILInfo_SIZE, out_buf_cnt, inter_out_file);

			fflush(inter_out_file);
			fclose(inter_in_file);
			fclose(inter_out_file);
			fclose(init_in_file);
			
		}
	}

	free(in_buffer);
	free(out_buffer);
}
*/

void merge_interleaving()
{
	printf(">>>>>>>>>>>Merge File Now>>>>>>>>>>>\n");
	FILE *logFile = fopen64("ORDER_RECORD.log", "r");
	FILE *outLog = fopen64("orderInter.log", "w+");
	FILE *temp_in[THREAD_NUM][CHUNK_NUM];
	FILE *temp_inRemaining[CHUNK_NUM];
	int i,j;

	if(logFile == NULL){
        		fprintf(stderr, "[xlc]: could NOT open file: ORDER_RECORD.log\n");
	        assert(0);
    	}

	if(outLog == NULL){
        		fprintf(stderr, "[xlc]: could NOT open file: orderInter.log\n");
	        assert(0);
    	}

	char name[40];
	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			sprintf(name, "inter_%d_%d.tmp", i, j);
			temp_in[i][j] = fopen64(name, "r");
			if(temp_in[i][j] == NULL)
			{
				fprintf(stderr, "[xlc]: could NOT open file :%s\n", name);
			}
			else{
				fseeko64(temp_in[i][j], (U_64)0, SEEK_SET);
			}
		}
	}	

	for (i = 0 ; i < CHUNK_NUM ; i ++)
	{
		sprintf(name, "record_rem_%d.tmp", i);
		temp_inRemaining[i] = fopen64(name, "r");
		if(temp_inRemaining[i] == NULL)
		{
			fprintf(stderr, "[xlc]: could NOT open file :%s\n", name);
		}
		else{
			fseeko64(temp_inRemaining[i], (U_64)0, SEEK_SET);
		}
	}




	Access_Info * access_pool = (Access_Info *)malloc(ACCESS_INFO_SIZE * ACCESS_INFO_NUM);
	init_InterInfo_pool();

         int last_access_count = 0;
	int inter_count = 0;
	fseeko64(logFile, (U_64)0, SEEK_SET);	

	ILInfo interInfoWithInit;
	while(!feof(logFile)){
		int num = fread((void *)access_pool, ACCESS_INFO_SIZE, ACCESS_INFO_NUM, logFile);
		printf("[xlc]: read %d access info from file\n", num);	
		
		for(i = 0; i < num; i++){
			Access_Info *record = &access_pool[i];
			
			if (record->alloc_tid < THREAD_NUM)
			{
				int tid = record->alloc_tid;
				int chunkid = record->alloc_count % CHUNK_NUM;

				fread((void *)&interInfoWithInit, ILInfo_SIZE, 1, temp_in[tid][chunkid]);
				assert(interInfoWithInit.alloc_tid == record->alloc_tid);
				assert(interInfoWithInit.alloc_count == record->alloc_count);

				if(interInfoWithInit.initOnly == 1 ||//init only
					(interInfoWithInit.next_tid == 0 && interInfoWithInit.next_acc_count == 0)){//last access
					continue;
				}

				memcpy((void *)&(InterInfo_pool.pool[InterInfo_pool.used_num]), (void *)&interInfoWithInit, InterInfo_SIZE);

				InterInfo_pool.used_num++;
				if(InterInfo_pool.used_num == ACCESS_INFO_NUM){
					fwrite((char *)InterInfo_pool.pool, InterInfo_SIZE, InterInfo_pool.used_num, outLog);
					InterInfo_pool.used_num = 0;
				}
			}
			else
			{
			         assert(0);
				
			}
			
		}
	
	}

         //write the reset out
	fwrite((char *)InterInfo_pool.pool, InterInfo_SIZE, InterInfo_pool.used_num, outLog);
	InterInfo_pool.used_num = 0;

	//printf("the number of the last access  is %d\n", last_access_count);
	//printf("the number of the interleaving is %d\n", inter_count);
	free(access_pool);
	destruct_InterInfo_pool();
	fclose(logFile);
	fflush(outLog);
	fclose(outLog);
	char in_name[40];
	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			fflush(temp_in[i][j]);
			fclose(temp_in[i][j]);
			sprintf(in_name, "inter_%d_%d.tmp", i, j);
                        remove(in_name);
		}
	}

	
	for (i = 0 ; i < CHUNK_NUM ; i ++)
	{
	        fflush(temp_inRemaining[i]);
		fclose(temp_inRemaining[i]);
		sprintf(in_name, "record_rem_%d.tmp", i);
                remove(in_name);
	}
}



