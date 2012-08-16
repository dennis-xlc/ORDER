#include <stdio.h>
#include "assert.h"
#include <malloc.h>
#include <string.h>
#include <map>

using std::map;
                
typedef map<unsigned int, unsigned int> hythreadMap;



typedef int I_32;
typedef short I_16;
typedef signed char I_8; /* chars can be unsigned */
typedef unsigned long long U_64;
typedef unsigned int U_32;
typedef unsigned short U_16;
typedef unsigned char U_8;


typedef map<U_32, U_8> InitOnlyMap;


typedef struct Access_Info {
    U_32 access_count;
    U_32 alloc_count;
    U_32 access_tid;
    U_16 alloc_tid;
    U_16 write_count;
} Access_Info;

typedef struct Alloc_Info {
    U_32 access_count;
    U_32 alloc_count;
} Alloc_Info;

long long thread_local_accesses;
long long init_only_accesses;
long long other_accesses;



#define ACCESS_INFO_NUM (1<<23) // the number of access info in every local access pool
#define ACCESS_INFO_SIZE  sizeof(Access_Info)
#define ALLOC_INFO_SIZE  sizeof(Alloc_Info)


#define THREAD_NUM 64
#define CHUNK_NUM 8

#define OUT_BUFFER_NUM_TOTAL (1<<23)
#define OUT_BUFFER_NUM_EACH (OUT_BUFFER_NUM_TOTAL/(THREAD_NUM*CHUNK_NUM))


int get_smallest_alloc_count_idx(Alloc_Info header[CHUNK_NUM])
{
	int idx = -1;
	U_32 smallest = 0xFFFFFFFF;
	int i;
	for (i = 0 ; i < CHUNK_NUM ; i ++)
	{
		if (header[i].alloc_count != -1 && (header[i].alloc_count & 0x7FFFFFFF) < smallest)
		{
			smallest = (header[i].alloc_count & 0x7FFFFFFF);
			idx = i;
		}
	}
	return idx;
}




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

	fseek(logFile, 0, SEEK_SET);
	while(!feof(logFile))
	{
		int num = fread((void *)&tmap, sizeof(Thread_Map), 1, logFile);
		if (num)
		{
			pthreadid_tid_map[tmap.pthread_id] = tmap.thread_assigned_id;
		}
	}

}


void split_file()
{

	//1Split File
       printf(">>>>>>>>>>>Split File Now>>>>>>>>>>>\n");
	FILE *logFile = fopen64("ORDER_RECORD.log", "r");
	FILE *out[THREAD_NUM][CHUNK_NUM];
	FILE *outRemaining[CHUNK_NUM];
	int i,j;

	if(logFile == NULL){
        	fprintf(stderr, "[xlc]: could NOT open file: ORDER_RECORD.log\n");
	        assert(0);
    	}

	char name[40];

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
	memset(access_pool, 0, ACCESS_INFO_SIZE*ACCESS_INFO_NUM);
	Access_Info * out_buffer[THREAD_NUM][CHUNK_NUM];
	U_32 out_buffer_idx[THREAD_NUM][CHUNK_NUM];

	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			out_buffer[i][j] = (Access_Info *)malloc(ACCESS_INFO_SIZE * OUT_BUFFER_NUM_EACH);
			memset(out_buffer[i][j], 0, ACCESS_INFO_SIZE * OUT_BUFFER_NUM_EACH);
			out_buffer_idx[i][j] = 0;
		}
	}




	
	U_64 cur_pos = 0;
	fseek(logFile, 0, SEEK_SET);	

	while(!feof(logFile)){
//		fseek(logFile, cur_pos, SEEK_SET);	
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
				out_buffer[tid][chunkid][out_buffer_idx[tid][chunkid]] = *record;
				out_buffer_idx[tid][chunkid] ++;
				if (out_buffer_idx[tid][chunkid] == OUT_BUFFER_NUM_EACH)
				{
					fwrite((char *)out_buffer[tid][chunkid], ACCESS_INFO_SIZE, OUT_BUFFER_NUM_EACH, out[tid][chunkid]);
//					printf("Write alloc info : 0x%x\n", OUT_BUFFER_NUM_EACH);
					out_buffer_idx[tid][chunkid] = 0;
				}
			}
			else
			{
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
			fwrite((char *)out_buffer[i][j], ACCESS_INFO_SIZE, out_buffer_idx[i][j], out[i][j]);
//			printf("Write alloc info : 0x%x\n", out_buffer_idx[i][j]);
			fflush(out[i][j]);
			fclose(out[i][j]);
		}
	}

	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			free(out_buffer[i][j]);
		}
	}

}





void verify_tid_map_completeness()
{
	char name[40];
	int i,j;

	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			sprintf(name, "record_%d_%d.tmp", i, j);
			printf(">>>>>>>>>>>Verify TID Mapping : %s>>>>>>>>>>>\n", name);
			FILE *logFile = fopen64(name, "r");

			hythreadMap threadMap;

			if(logFile == NULL)
			{
		        	fprintf(stderr, "[xlc]: could NOT open file\n");
			       assert(0);
		    	}

			Access_Info * access_pool = (Access_Info *)malloc(ACCESS_INFO_SIZE * ACCESS_INFO_NUM);

			fseek(logFile, 0, SEEK_SET);
			while(!feof(logFile))
			{
				int num = fread((void *)access_pool, ACCESS_INFO_SIZE, ACCESS_INFO_NUM, logFile);
				printf("[xlc]: read %d access info from file\n", num);

				for(int k = 0; k < num; k++)
				{
					Access_Info *record = &access_pool[k];
					
					pthreadidTidMap::iterator iter;
		    			iter = pthreadid_tid_map.find(record->access_tid);


		    			if(iter == pthreadid_tid_map.end())
					{
						printf("Missing TID : %d %d alloc_count:%d\n", record->access_tid, record->alloc_tid, record->alloc_count);
						//this is the first time to process
//						assert(0);
		        		}
                                   else
                                   {
//                                          if(tid_map[record->access_tid] != record->alloc_tid)
//                                          {
//						    printf("TID interleaving : %d -> %d\n", tid_map[record->access_tid], record->alloc_tid);
//                                              tid_map[record->access_tid] = record->alloc_tid;
//                                          }
                                   }
			
				}

			}

			free(access_pool);
			fclose(logFile);
		}
	}
}


void process_files()
{
	FILE *out[THREAD_NUM][CHUNK_NUM];
	char name[40];
	int i,j;

	//1Extract First Access Thread Information
	for(i = 0; i < THREAD_NUM; i++)
	{
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			sprintf(name, "record_%d_%d.tmp", i, j);
			printf(">>>>>>>>>>>Process File : %s>>>>>>>>>>>\n", name);
			FILE *logFile = fopen64(name, "r");

			sprintf(name, "record_%d_%d_alloc.tmp", i, j);
			FILE *out = fopen64(name, "w+");

			hythreadMap threadMap;

			if(logFile == NULL || out == NULL)
			{
		        	fprintf(stderr, "[xlc]: could NOT open file\n");
			        assert(0);
		    	}

			Access_Info * access_pool = (Access_Info *)malloc(ACCESS_INFO_SIZE * ACCESS_INFO_NUM);
			Alloc_Info * out_buffer = (Alloc_Info *)malloc(ALLOC_INFO_SIZE * OUT_BUFFER_NUM_TOTAL);

//			int cur_pos = 0;
			U_32 out_buffer_idx = 0;

			fseek(logFile, 0, SEEK_SET);
			while(!feof(logFile))
			{
				int num = fread((void *)access_pool, ACCESS_INFO_SIZE, ACCESS_INFO_NUM, logFile);
				printf("[xlc]: read %d access info from file\n", num);
			
				for(int i = 0; i < num; i++)
				{
					Access_Info *record = &access_pool[i];
					hythreadMap::iterator iter;
		    			iter = threadMap.find(record->alloc_count);

//					if (true)
		    			if(iter == threadMap.end())
					{
						//this is the first time to process
						threadMap[record->alloc_count] = record->access_count;
						/*
						if (record->alloc_count == 1195 && record->alloc_tid == 3)
						{
							printf("alloc info : (%d, %d) -> %d\n", record->alloc_tid, record->alloc_count, record->access_count);
						}
						*/
//						printf("AllocTID: %d,  AllocCount: %d,  AccessTID: %d,  AccessCount: %d \n",
//		                                	record->alloc_tid, record->alloc_count, record->access_tid, record->access_count);
		        		}
			
				}
//				cur_pos += num * ACCESS_INFO_SIZE;
			}

			hythreadMap::iterator iter;
			for(iter = threadMap.begin(); iter != threadMap.end(); iter++)
			{
				Alloc_Info alloc;
				alloc.alloc_count= iter->first;
				alloc.access_count = iter->second;

				out_buffer[out_buffer_idx] = alloc;
				out_buffer_idx++;
				if (out_buffer_idx == OUT_BUFFER_NUM_TOTAL)
				{
					fwrite((char *)out_buffer, ALLOC_INFO_SIZE, OUT_BUFFER_NUM_TOTAL, out);
					printf("Write alloc info : 0x%x\n", out_buffer_idx * ALLOC_INFO_SIZE);
					out_buffer_idx = 0;
				}
//				fwrite((char *)&alloc, ALLOC_INFO_SIZE, 1, out);
//				assert(iter->second == 1);
//				if (iter->second != 1)
//					exit(0);
			}

			fwrite((char *)out_buffer, ALLOC_INFO_SIZE, out_buffer_idx, out);
			printf("Write alloc info : 0x%x\n", out_buffer_idx * ALLOC_INFO_SIZE);


			free(access_pool);
			free(out_buffer);
			fclose(logFile);
			fflush(out);
			fclose(out);
		}
	}
}

void process_init_only()
{
	printf("Processing init only object alloc info...\n");
	char alloc_in_name[40];
	char alloc_out_name[40];
	char init_in_name[40];

         int buf_size = 1000;
	Alloc_Info * in_buffer = (Alloc_Info *)malloc(ALLOC_INFO_SIZE * buf_size);
	Alloc_Info * out_buffer = (Alloc_Info *)malloc(ALLOC_INFO_SIZE * buf_size);
	U_32 buf[buf_size];
	
	for(int t_num = 0; t_num < THREAD_NUM; t_num++){
		for(int c_num = 0 ; c_num < CHUNK_NUM ; c_num ++){
			sprintf(alloc_in_name, "record_%d_%d_alloc.tmp", t_num, c_num);
			sprintf(alloc_out_name, "record_%d_%d_alloc_init.tmp", t_num, c_num);
			sprintf(init_in_name, "initOnly_%d_%d.init", t_num, c_num);
			
			FILE *alloc_in_file = fopen64(alloc_in_name, "r");
			FILE *alloc_out_file = fopen64(alloc_out_name, "w+");
			FILE *init_in_file = fopen64(init_in_name, "r");
			if(alloc_in_file == NULL || alloc_out_file == NULL)
			{
				fprintf(stderr, "[xlc]: could NOT open file \n");
			}

			printf("process file : %s\n", alloc_in_name);


			//1 step 1.init vector
			printf("init vector\n");
			
			InitOnlyMap initOnlyMap;
			fseek(init_in_file, 0, SEEK_SET);
			while(!feof(init_in_file))
			{
                                U_32 init_only_alloc_count = 0;
                                U_8 init_only_flag = 1;
				fread((void *)&init_only_alloc_count, sizeof(U_32), 1, init_in_file);
                                fread((void *)&init_only_flag, sizeof(U_8), 1, init_in_file);
                                if (feof(init_in_file))
                                        break;
                                assert(init_only_flag != 1);
//				for(int k = 0; k < num; k++){
				initOnlyMap[init_only_alloc_count] = init_only_flag;
//				}
			}

			printf("process init only\n");
			int out_buf_cnt = 0;
			InitOnlyMap::iterator iter;
			while(!feof(alloc_in_file))
			{
				int num = fread((void *)in_buffer, ALLOC_INFO_SIZE, buf_size, alloc_in_file);
				for(int k = 0; k < num; k++){
					Alloc_Info *allocInfo = &in_buffer[k];
					iter = initOnlyMap.find(allocInfo->alloc_count);
					if(iter == initOnlyMap.end()){
                                           assert(allocInfo->access_count < 0x80000000);
						memcpy((void *)&(out_buffer[out_buf_cnt]), (void *)allocInfo, ALLOC_INFO_SIZE);
						out_buf_cnt++;
						if(out_buf_cnt == buf_size){
							fwrite((char *)out_buffer, ALLOC_INFO_SIZE, out_buf_cnt, alloc_out_file);
							out_buf_cnt = 0;
						}
//                                                printf("here count others.\n");
                                                other_accesses += allocInfo->access_count;
					}
                                        else
                                        {
                                                if (initOnlyMap[allocInfo->alloc_count] == 0)
                                                {
                                                    assert(allocInfo->access_count < 0x80000000);
                                                    allocInfo->access_count |= 0x80000000;
                                                    memcpy((void *)&(out_buffer[out_buf_cnt]), (void *)allocInfo, ALLOC_INFO_SIZE);
                                                    out_buf_cnt++;
                                                    if(out_buf_cnt == buf_size){
                                                        fwrite((char *)out_buffer, ALLOC_INFO_SIZE, out_buf_cnt, alloc_out_file);
                                                        out_buf_cnt = 0;
                                                    }
//                                                        printf("here count init only, %d,%d.\n", t_num, allocInfo->access_count & 0x7FFFFFFF);
                                                    init_only_accesses += allocInfo->access_count;
                                                }
                                                else
                                                {
                                                    assert(allocInfo->access_count < 0x80000000);
                                                    memcpy((void *)&(out_buffer[out_buf_cnt]), (void *)allocInfo, ALLOC_INFO_SIZE);
                                                    out_buf_cnt++;
                                                    if(out_buf_cnt == buf_size){
                                                        fwrite((char *)out_buffer, ALLOC_INFO_SIZE, out_buf_cnt, alloc_out_file);
                                                        out_buf_cnt = 0;
                                                    }

//                                                        printf("here count thread local.\n");
                                                    thread_local_accesses += allocInfo->access_count;
                                                }
                                        }
				}
			}
			//write the reset out
			fwrite((char *)out_buffer, ALLOC_INFO_SIZE, out_buf_cnt, alloc_out_file);

			fflush(alloc_out_file);
			fclose(alloc_in_file);
			fclose(alloc_out_file);
			fclose(init_in_file);
			
		}
	}

	free(in_buffer);
	free(out_buffer);
	printf("End of process init only object alloc info...\n");
}

void merge_files()
{
	char name[40];
	int i,j;
	
	//1Merge file
	for(i = 0; i < THREAD_NUM; i++)
	{
		printf("here i = %d\n", i);

		sprintf(name, "record_%d_alloc.log", i);
		printf(">>>>>>>>>>>Merge File : %s>>>>>>>>>>>\n", name);
		FILE *out = fopen64(name, "w+");


		FILE *in[CHUNK_NUM];
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			sprintf(name, "record_%d_%d_alloc_init.tmp", i, j);
			in[j] = fopen64(name, "r");
			if(in[j] == NULL)
			{
				fprintf(stderr, "[xlc]: could NOT open file :%s\n", name);
			}
		}



		Alloc_Info header[CHUNK_NUM];
		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			fseek(in[j], 0, SEEK_SET);	
			fread((void *)&header[j], ALLOC_INFO_SIZE, 1, in[j]);
		}

		int chunk_idx;
		while((chunk_idx = get_smallest_alloc_count_idx(header)) != -1)
		{
                     if ((header[chunk_idx].access_count & 0x80000000) != 0)
                     {
//                         printf("in merge, init only : %d, %d -> %d\n", i, header[chunk_idx].alloc_count, header[chunk_idx].access_count);
			}
			fwrite((char *)&header[chunk_idx], ALLOC_INFO_SIZE, 1, out);
			int num = fread((void *)&header[chunk_idx], ALLOC_INFO_SIZE, 1, in[chunk_idx]);
			if (num <= 0)
			{
				header[chunk_idx].alloc_count = -1;
			}
		}



		for (j = 0 ; j < CHUNK_NUM ; j ++)
		{
			fclose(in[j]);
		}

		fflush(out);
		fclose(out);

	}


}

void verify_files()
{
	char name[40];
	U_32 i,j,chunk_idx;
	Alloc_Info * alloc_pool = (Alloc_Info *)malloc(ALLOC_INFO_SIZE * ACCESS_INFO_NUM);

        //1Verification
        for(i = 0; i < THREAD_NUM; i++)
        {
                sprintf(name, "record_%d_alloc.log", i, j);
                printf(">>>>>>>>>>>Verify File : %s>>>>>>>>>>>\n", name);
                FILE *in = fopen64(name, "r");


                fseek(in, 0, SEEK_SET);
                chunk_idx = 0;
                U_32 last_count = 0; 
                while(!feof(in))
                {
                	int num = fread((void *)alloc_pool, ALLOC_INFO_SIZE, ACCESS_INFO_NUM, in);

			if (i != 0)
			{
				for( j = 0; j < num; j++)
				{
					Alloc_Info *alloc_info = &alloc_pool[j];
                                        if(alloc_info->alloc_count != last_count + 1){
						printf("VERIFY ERROR[%d]: Missing from %d to %d \n", i, last_count+1, alloc_info->alloc_count - 1);
						last_count = alloc_info->alloc_count;
					}
					else{
						last_count++;
					}
					/*if (alloc_info->alloc_count != chunk_idx*ACCESS_INFO_NUM+j+1)
					{
						printf("tid, alloc_count, idx in file : %x, %x, %x\n", i, alloc_info->alloc_count, chunk_idx*ACCESS_INFO_NUM+j);
					}*/
				}
			}
			chunk_idx++;
		}

	}
}

int main()
{
        thread_local_accesses = 0;
        init_only_accesses = 0;
        other_accesses = 0;

	get_tid_map();
	split_file();
//	verify_tid_map_completeness();
	process_files();
         //process init only
         process_init_only();
         //end process init only
	merge_files();
//	verify_files();

        printf("thread_local_accesses in alloc : %lld\n", thread_local_accesses);
        printf("init_only_accesses in alloc : %lld\n", init_only_accesses);
        printf("other_accesses in alloc : %lld\n", other_accesses);

	//1Caution!!!: remaining thread file is not processed!



}


