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
#include <vector>
#include <bitset>
#include <iostream>
#include <sys/time.h>
#include <unistd.h>

#define BLOCK_SIZE 4096

// offset: 40bit (~1TB)
// size: 12bit (~4KB)
// ref: 28bit (1TB / 4KB)
// flag: 2bit (00: not-compressed, 01: self-compressed, 10: deduped, 11: delta-compressed)
// total: 40 + 12 + 28 + 2 = 82bit

typedef std::bitset<82> RECIPE;

static inline void set_offset(RECIPE& r, unsigned long long t) { r |= (RECIPE(t) << 42); }
static inline void set_size(RECIPE& r, unsigned long t) { r |= (RECIPE(t) << 30); }
static inline void set_ref(RECIPE& r, unsigned long t) { r |= (RECIPE(t) << 2); }
static inline void set_flag(RECIPE& r, unsigned long t) { r |= (RECIPE(t) << 0); }
static inline unsigned long long get_offset(RECIPE& r) { return ((r << 0) >> 42).to_ullong(); }
static inline unsigned long get_size(RECIPE& r) { return ((r << 40) >> 70).to_ulong(); }
static inline unsigned long get_ref(RECIPE& r) { return ((r << 52) >> 54).to_ulong(); }
static inline unsigned long get_flag(RECIPE& r) { return ((r << 80) >> 80).to_ulong(); }

char compressed[2 * BLOCK_SIZE];
char delta_compressed[2 * BLOCK_SIZE];

#define SymbolCount 256
#define DigistLength 16
#define SeedLength 64
#define MaxChunkSizeOffset 3
#define MinChunkSizeOffset 2

int N = 0;
int NUM_THREAD;
std::vector<std::tuple<char*, int>> trace;
//char compressed[MAX_THREAD][8198];


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

//limit the mask to lower 4 byte to constrain window size
uint64_t g_condition_mask[] = {
    //Do not use 1-32B, for aligent usage
        0x0000000000000000,// 1B
        0x0000000001000000,// 2B
        0x0000000003000000,// 4B
        0x0000000013000000,// 8B
        0x0000000093000000,// 16B
        0x0000000093001000,// 32B

        0x0000000093005000,// 64B
        0x0000000093005100,// 128B
        0x0000000093005500,// 256B
        0x0000000093015500,// 512B
        0x0000000093035500,// 1KB
        0x0000000093035510,// 2KB
        0x000000009303d510,// 4KB
};


class FASTCDC{
public:
	FASTCDC(int chunkSize){
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
		this->chunkAvg = chunkSize;
		this->chunkMax = chunkSize*2;
		this->chunkMin = chunkSize/8;
	    index = log2(this->chunkAvg);
	    assert(index>6);
	    assert(index<17);
	    MaskS_fast = g_condition_mask[index+1];
	    MaskL_fast = g_condition_mask[index-1];
	}
	int fastcdc_chunk_data(unsigned char *p, int n);

private:
	int chunkMax, chunkAvg, chunkMin;
	uint64_t MaskS_fast;
	uint64_t MaskL_fast;
	uint64_t g_gear_matrix_fast[SymbolCount];
};

int FASTCDC::fastcdc_chunk_data(unsigned char *p, int n){

    uint64_t fingerprint=0;
    //uint64_t digest __attribute__((unused));
    //int i=chunkMin;//, Mid=chunkMin + 8*1024;
    int i=0;//, Mid=chunkMin + 8*1024;
    int Mid = this->chunkAvg;
    //return n;

    if(n <= this->chunkMin) //the minimal  subChunk Size.
        return n;
    //windows_reset();
    if(n > this->chunkMax)
        n = this->chunkMax;
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

struct DATA_IO {
	int N;
//	std::vector<char*> trace;
	std::vector<std::tuple<char*, int>> trace;
	std::vector<RECIPE> recipe;

	char fileName[100];
	char outputFileName[100];
	char recipeName[100];

	FILE* out;

	struct timeval start_time, end_time;
	DATA_IO(char* name, FASTCDC* cdc);
	~DATA_IO();
	void read_file();

	FASTCDC* cdc;
	void readFile(char* fileName, unsigned long s);
	void treaverse(char* path);
	inline void write_file(char* data, int size);
	inline void recipe_insert(RECIPE r);
	void recipe_write();
	void time_check_start();
	long long time_check_end(); 
	long fileNum = 0;
	long totalSize = 0;
};

DATA_IO::DATA_IO(char* name, FASTCDC* cdc) {
	sprintf(fileName, "%s", name);
	sprintf(outputFileName, "./_output");
	sprintf(recipeName, "./_recipe");
	this->cdc = cdc;
	out = NULL;
}

DATA_IO::~DATA_IO() {
	if (out) fclose(out);

//	for (int i = 0; i < N; ++i) {
//		free(trace[i]);
//	}
}

void DATA_IO::read_file() {
//	N = 0;
//	trace.clear();
//
//	FILE* f = fopen(fileName, "rb");
//	while (1) {
//		char* ptr = new char[BLOCK_SIZE];
//		auto chunk = std::make_tuple(ptr, BLOCK_SIZE);
////		trace.push_back(ptr);
//		trace.push_back(chunk);
//		int now = fread(trace[N++], 1, BLOCK_SIZE, f);
//		if (!now) {
//			free(trace.back());
//			trace.pop_back();
//			N--;
//			break;
//		}
//	}
//	fclose(f);
}

void DATA_IO::readFile(char* fileName, unsigned long s) {
	unsigned long fileSize = s;
//	cout<<"chunking file:"<<fileName<<" file size: "<<s<<endl;
	FILE* f = fopen(fileName, "rb");
	if(!f){
		perror("open file failed");
	}
	char* data = new char[s+1024];
	char* addrBegin = data;
	char* addrEnd = &data[s+1024];
	fileSize = fread(data, 1, s, f);
	while (fileSize>0) {
		int chunkSize = this->cdc->fastcdc_chunk_data((unsigned char*)data, fileSize);
//		cout<<"chunking size:"<<chunkSize<<" left size: "<<fileSize<<endl;
		auto chunk = std::make_tuple(data, chunkSize);
		trace.push_back(chunk);

		data = &data[chunkSize];
		fileSize -= chunkSize;
		N++;
	}
	fclose(f);
//	std::cout<<"chunking file:"<<fileName<<" over, chunks number: "<<N<<std::endl;
//	printf("addr range: %lx ~ %lx\n", addrBegin, addrEnd);
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

void DATA_IO::treaverse(char* path){
        DIR* dir;
        struct dirent *ent;
        struct stat states;

        dir = opendir(path);
		if(!dir){
			perror("open dir failed");
			std::cout<<"dir: "<<path<<std::endl;
		}

		ent=readdir(dir);
		if(!ent){
			perror("read dir failed");
			std::cout<<"dir: "<<path<<std::endl;
		}

		do{
			char filePath[1025];
			joinPathtoStr(filePath, path, ent->d_name);
//			printf("stat file: %s\n", filePath);
            int t = stat(filePath, &states);
			if(t){
				perror("stat dir failed");
				std::cout<<"dir: "<<filePath<<std::endl;
			}
            if(!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name)){
                goto nextFile;
            }else{
                if(S_ISDIR(states.st_mode)){
                    treaverse(filePath);
                }else{
					readFile(filePath, states.st_size);
					this->fileNum++;
					this->totalSize += states.st_size;
//	                printf("%s/%s, fileNum:%5d, total Size:%ld\n",filePath, ent->d_name, this->fileNum, this->totalSize);
				}
            };

nextFile:
			ent = readdir(dir);
//			printf("name: %s, ent: %lx\n", name, ent);
        }while(ent);

        closedir(dir);
}

inline void DATA_IO::write_file(char* data, int size) {
	if (out == NULL) out = fopen(outputFileName, "wb");
	fwrite(data, size, 1, out);
}

inline void DATA_IO::recipe_insert(RECIPE r) {
	recipe.push_back(r);
}

void DATA_IO::recipe_write() {
	FILE* f = fopen(recipeName, "wb");
	for (int i = 0; i < N; ++i) {
		fwrite(&recipe[i], 1, sizeof(RECIPE), f);
	}
	fclose(f);
}

void DATA_IO::time_check_start() {
	gettimeofday(&start_time, NULL);
}

long long DATA_IO::time_check_end() {
	gettimeofday(&end_time, NULL);
	return (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_usec - start_time.tv_usec);
}


