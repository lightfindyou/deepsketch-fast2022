#include <iostream>
#include <vector>
#include <cmath>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <sys/stat.h>

#define BLOCK_SIZE 4096
#define INF 987654321
#define COARSE_T 2048
#define MAX_THREAD 256
using namespace std;

int N = 0;
int NUM_THREAD;
vector<char*> trace;
char buf[MAX_THREAD][BLOCK_SIZE];
char compressed[MAX_THREAD][2 * BLOCK_SIZE];
vector<vector<int>> cluster;

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
			fwrite(trace[u], BLOCK_SIZE, 1, file);
			isErr = fclose(file);
			if(isErr){
				perror("error on close file");
			}
		}
		printf("\n");
	}
	printf("\n");
}

void read_file(char* name) {
	N = 0;
	trace.clear();
	int blockNum = 0;
	vector<int> blockList;
	cout<<"read start!"<<endl;

	FILE* f = fopen(name, "rb");
	while (1) {
		char* ptr = new char[BLOCK_SIZE];
		trace.push_back(ptr);
		int now = fread(trace[N++], 1, BLOCK_SIZE, f);
		if (!now ) {
			free(trace.back());
			trace.pop_back();
			N--;
			cout<<"trace blocks number: "<<N<<endl;
			break;
		}

		blockList.push_back(N);
		blockNum++;
		if(blockNum >= 5){
			blockNum = 0;
			cluster.push_back(vector<int>(blockList));

			blockList.clear();
		}
	}
	fclose(f);
	cout<<"read over!"<<endl;
	cout<<"cluster number:"<<cluster.size()<<endl;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		cerr << "usage: ./writeTest [input_file]\n";
		exit(0);
	}

	read_file(argv[1]);
	print_cluster(cluster);
}