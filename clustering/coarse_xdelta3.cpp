#include <assert.h>
#include <math.h>
#include <memory.h>
#include <openssl/md5.h>
#include <stdint.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include "xxhash.h"
//#include "../xxhash.h"
#include "../xdelta3/xdelta3.h"
#define INF 987654321
#define COARSE_T 2048
#define MAX_THREAD 256
using namespace std;

#define SymbolCount 256
#define DigistLength 16
#define SeedLength 64
#define MaxChunkSizeOffset 3
#define MinChunkSizeOffset 2

int N = 0;
int NUM_THREAD;
vector<tuple<char*, int>> trace;
char compressed[MAX_THREAD][8198];
uint64_t g_gear_matrix_fast[SymbolCount];

uint64_t MaskS_fast;
uint64_t MaskL_fast;

enum{
    Mask_64B,
    Mask_128B,
    Mask_256B,
    Mask_512B,
    Mask_1KB,
    Mask_2KB,
    Mask_4KB,
    Mask_8KB,
    Mask_16KB,
    Mask_32KB,
    Mask_64KB,
    Mask_128KB
};

uint64_t g_condition_mask[] = {
    //Do not use 1-32B, for aligent usage
        0x0000000000000000,// 1B
        0x0000000001000000,// 2B
        0x0000000003000000,// 4B
        0x0000010003000000,// 8B
        0x0000090003000000,// 16B
        0x0000190003000000,// 32B

        0x0000590003000000,// 64B
        0x0000590003100000,// 128B
        0x0000590003500000,// 256B
        0x0000590003510000,// 512B
        0x0000590003530000,// 1KB
        0x0000590103530000,// 2KB
        0x0000d90103530000,// 4KB
        0x0000d90303530000,// 8KB
        0x0000d90303531000,// 16KB
        0x0000d90303533000,// 32KB
        0x0000d90303537000,// 64KB
        0x0000d90703537000// 128KB
};

static int chunkMax, chunkAvg, chunkMin;

void fastcdc_init(int chunkSize){
    char seed[SeedLength];
    int index;
    for(int i=0; i<SymbolCount; i++){
        for(int j=0; j<SeedLength; j++){
            seed[j] = i;
        }

        g_gear_matrix_fast[i] = 0;
        unsigned char md5_result[DigistLength];

        MD5_CTX md5_ctx;
        MD5_Init(&md5_ctx);
        MD5_Update(&md5_ctx, seed, SeedLength);
        MD5_Final(md5_result, &md5_ctx);

        memcpy(&g_gear_matrix_fast[i], md5_result, sizeof(uint64_t));
    }

//    chunkMin = 2048;
//   chunkMax = 65536;
//    chunkAvg = expectCS;
	chunkAvg = chunkSize;
	chunkMax = chunkSize*2;
	chunkMin = chunkSize/8;
    index = log2(chunkAvg);
    assert(index>6);
    assert(index<17);
    MaskS_fast = g_condition_mask[index+1];
    MaskL_fast = g_condition_mask[index-1];
}


int fastcdc_chunk_data(unsigned char *p, int n){

    uint64_t fingerprint=0;
    //uint64_t digest __attribute__((unused));
    //int i=chunkMin;//, Mid=chunkMin + 8*1024;
    int i=0;//, Mid=chunkMin + 8*1024;
    int Mid = chunkAvg;
    //return n;

    if(n<=chunkMin) //the minimal  subChunk Size.
        return n;
    //windows_reset();
    if(n >chunkMax)
        n =chunkMax;
    else if(n<Mid)
        Mid = n;

    while(i<Mid){
        fingerprint = (fingerprint<<1) + (g_gear_matrix_fast[p[i]]);
        if ((!(fingerprint & MaskS_fast /*0x0000d90f03530000*/))) { //AVERAGE*2, *4, *8
            return i;
        }
        i++;
    }

    while(i<n){
        fingerprint = (fingerprint<<1) + (g_gear_matrix_fast[p[i]]);
        if ((!(fingerprint & MaskL_fast /*0x0000d90003530000*/))) { //Average/2, /4, /8
            return i;
        }
        i++;
    }
    //printf("\r\n==chunking FINISH!\r\n");
    return i;
}

int do_xdelta3(int i, int j, int id) {
	return xdelta3_compress(std::get<0>(trace[i]), std::get<1>(trace[i]),
		 std::get<0>(trace[j]), std::get<1>(trace[j]), compressed[id], 1);
}

typedef tuple<int, int, int, int> BFinfo;

struct BruteForceClusterArgument {
	pthread_mutex_t* mutex;
	int id;
	vector<int>* todo;
	int* rep_list;
	int* rep_cnt;
	queue<int>* produceQ;
	priority_queue<BFinfo, vector<BFinfo>, greater<BFinfo>>* resultQ;
};

void* BF(void* argu) {
	BruteForceClusterArgument* arg = (BruteForceClusterArgument*)argu;

	vector<int>& todo = *(arg->todo);
	int* rep_list = arg->rep_list;
	queue<int>& produceQ = *(arg->produceQ);
	priority_queue<BFinfo, vector<BFinfo>, greater<BFinfo>>& resultQ = *(arg->resultQ);

	while (1) {
		pthread_mutex_lock(arg->mutex);
		if (produceQ.empty()) {
			pthread_mutex_unlock(arg->mutex);
			continue;
		}
		int i = produceQ.front();
		if (i == INF) {
			pthread_mutex_unlock(arg->mutex);
			return NULL;
		}
		produceQ.pop();
		int mx = *(arg->rep_cnt);
		pthread_mutex_unlock(arg->mutex);

		int compress_min = INF;
		int ref_index = -1;
		for (int j = 0; j < mx; ++j) {
			int now = do_xdelta3(todo[i], rep_list[j], arg->id);
			if (now < compress_min) {
				compress_min = now;
				ref_index = j;
			}
		}
		pthread_mutex_lock(arg->mutex);
		resultQ.push({i, mx, compress_min, ref_index});
		pthread_mutex_unlock(arg->mutex);
	}
}

// todo: list of blocks not included in cluster
// cluster: first element is representative
void bruteForceCluster(vector<int>& todo, vector<vector<int>>& cluster, int threshold) {
	int MAX_QSIZE = 2 * NUM_THREAD;

	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);

	int* rep_list = new int[cluster.size() + todo.size()];
	int rep_cnt = 0;
	for (int i = 0; i < (int)cluster.size(); ++i) {
		rep_list[rep_cnt++] = cluster[i][0];
	}

	// Priority Queue: {index in todo, checked index of rep_list, size, ref index of rep_list}
	queue<int> produceQ;
	priority_queue<BFinfo, vector<BFinfo>, greater<BFinfo>> resultQ;
	
	for (int i = 0; i < min(MAX_QSIZE, todo.size()); ++i) produceQ.push(i);

	BruteForceClusterArgument arg[NUM_THREAD - 1];
	for (int i = 0; i < NUM_THREAD - 1; ++i) {
		arg[i].mutex = &mutex;
		arg[i].id = i;
		arg[i].todo = &todo;
		arg[i].rep_list = rep_list;
		arg[i].rep_cnt = &rep_cnt;
		arg[i].produceQ = &produceQ;
		arg[i].resultQ = &resultQ;
	}

	// Create thread
	pthread_t tid[NUM_THREAD];
	for (int i = 0; i < NUM_THREAD - 1; ++i) pthread_create(&tid[i], NULL, BF, (void*)&arg[i]);

	for (int i = 0; i < (int)todo.size(); ++i) {
		BFinfo now;
		while (1) {
			pthread_mutex_lock(&mutex);

			if (resultQ.empty()) now = {-1, -1, -1, -1};
			else now = resultQ.top();

			if (get<0>(now) == i) {
				resultQ.pop();
				pthread_mutex_unlock(&mutex);
				break;
			}
			pthread_mutex_unlock(&mutex);
		}

		int compress_min = get<2>(now);
		int ref_index = get<3>(now);

		for (int j = get<1>(now); j < rep_cnt; ++j) {
			int e = do_xdelta3(todo[i], rep_list[j], NUM_THREAD - 1);
			if (e < compress_min) {
				compress_min = e;
				ref_index = j;
			}
		}

		if (compress_min <= threshold) {
			cluster[ref_index].push_back(todo[i]);
		}
		else {
			cluster.push_back(vector<int>(1, todo[i]));
			pthread_mutex_lock(&mutex);
			rep_list[rep_cnt++] = todo[i];
			pthread_mutex_unlock(&mutex);
		}

		if (i + MAX_QSIZE < (int)todo.size()) {
			pthread_mutex_lock(&mutex);
			produceQ.push(i + MAX_QSIZE);
			pthread_mutex_unlock(&mutex);
		}

		if (i % 1000 == 999) {
			fprintf(stderr, "%d, qsize: %d, rep_cnt: %d\n", i, (int)resultQ.size(), rep_cnt);
		}
	}
	pthread_mutex_lock(&mutex);
	produceQ.push(INF);
	pthread_mutex_unlock(&mutex);

	for (int i = 0; i < NUM_THREAD - 1; ++i) pthread_join(tid[i], NULL);

	delete[] rep_list;

	return;
}

void print_cluster(vector<vector<int>>& cluster) {
	char* basePath = "/home/xzjin/src/deepsketch-fast2022/clustering/results/blocks";
	for (int i = 0; i < cluster.size(); ++i) {
		printf("%d ", cluster[i].size());
		char dir[256];
		sprintf(dir, "%s/%d", basePath, i);
		int isErr = mkdir(dir, 0777);
		if(isErr && (errno != EEXIST)){
			perror("error on mkdir");
		}
		for (int u: cluster[i]) {
			char fileName[256];
			printf("%d ", u);

			sprintf(fileName, "%s/%d", dir, u);
			FILE* file = fopen(fileName, "w");
			if(!file){
				perror("error on openfile");
			}
//			cout<<"block number: "<<u<<endl;
//			cout<<"trace address: 0x"<<std::hex<<trace[u]<<endl;
			fwrite(std::get<0>(trace[u]), std::get<1>(trace[u]), 1, file);
			isErr = fclose(file);
			if(isErr){
				perror("error on close file");
			}
		}
		printf("\n");
	}
	printf("\n");
}

void readFile(char* fileName, int s) {
	trace.clear();
	int fileSize = s;
	FILE* f = fopen(fileName, "rb");
	if(!f){
		perror("open file failed");
	}
	char* data = new char[s+1024];
	fileSize = fread(data, 1, s, f);
	while (fileSize>=0) {
		int chunkSize = fastcdc_chunk_data((unsigned char*)data, fileSize);
		fileSize -= chunkSize;
		auto const chunk = std::make_tuple(data, chunkSize);
		trace.push_back(chunk);
		N++;
	}
	fclose(f);
}

void joinPath(char* path, char* file){
	strcat(path, "/");
	strcat(path, file);
}

void joinPathtoStr(char* target, char* path, char* file){
	strcpy(target, path);
	strcat(target, "/");
	strcat(target, file);
}

void treaverse(char* name){
        DIR* dir;
        struct dirent *ent;
        struct stat states;

        dir = opendir(name);
		if(!dir){
			perror("open dir failed");
		}

		ent=readdir(dir);
		if(!ent){
			perror("read dir failed");
		}
				
		do{
			char filePath[1025];
			joinPathtoStr(filePath, name, ent->d_name);
//			printf("stat file: %s\n", filePath);
            int t = stat(filePath, &states);
			if(t){
				perror("stat dir failed");
			}
            if(!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name)){
                goto nextFile;
            }else{
//                printf("%s/%s\n",name,ent->d_name);
				readFile(filePath, states.st_size);
                if(S_ISDIR(states.st_mode)){
                    treaverse(filePath);
                }
            };

nextFile:
			ent = readdir(dir);
//			printf("name: %s, ent: %lx\n", name, ent);
        }while(ent);

        closedir(dir);
}

int main(int argc, char* argv[]) {
	char path[1024];
	if (argc != 3) {
		cerr << "usage: ./coarse [input_file] [num_thread]\n";
		exit(0);
	}
	NUM_THREAD = atoi(argv[2]);
	strcpy(path, argv[1]);
	fastcdc_init(4096);

	treaverse(path);
	cout<<"read file over."<<endl;

	set<uint64_t> dedup;
	vector<int> unique_list;
	for (int i = 0; i < N; ++i) {
		XXH64_hash_t h = XXH64(std::get<0>(trace[i]), std::get<1>(trace[i]), 0);

		if (dedup.count(h)) continue;
		else {
			dedup.insert(h);
			unique_list.push_back(i);
		}
	}

	vector<vector<int>> cluster;
	bruteForceCluster(unique_list, cluster, COARSE_T);

	unique_list.clear();
	vector<vector<int>> newcluster;
	for (int i = 0; i < cluster.size(); ++i) {
		int lz4_min = INF;
		int rep = -1;
		for (int j: cluster[i]) {
			double r = rand() / (double)RAND_MAX;
			if (r < ((int)cluster[i].size() - 1000) / (double)cluster[i].size()) continue;

			int sum = 0;
			for (int k: cluster[i]) {
				sum += do_xdelta3(j, k, 0);
			}

			if (sum < lz4_min) {
				lz4_min = sum;
				rep = j;
			}
		}
		newcluster.push_back(vector<int>(1, rep));
		for (int j: cluster[i]) {
			if (j != rep) unique_list.push_back(j);
		}
	}

	bruteForceCluster(unique_list, newcluster, COARSE_T);
	print_cluster(newcluster);
}
