#include <iostream>
#include <vector>
#include <set>
#include <bitset>
#include <list>
#include <map>
#include <cmath>
#include <algorithm>
#include "../compress.h"
#include "./fineANN.h"
#include "../lz4.h"
#include "xxhash.h"

extern "C" {
	#include "../xdelta3/xdelta3.h"
}
#define INF 987654321
// The max size of dataset GB
#define MAXDATASETSIZE 100ULL
#define MINICHUNKSIZE 4096ULL
#define MAXELEMENTS MAXDATASETSIZE*1024*1024*1024/MINICHUNKSIZE

using namespace std;

typedef pair<int, int> ii;

int main(int argc, char* argv[]) {
	int xdeltaSavedSpace = 0;
	unsigned long inputSize = 0;
	int procingIdx = 0;
	if (argc < 3) {
		cerr << "usage: ./fineANN [input_file] [threshold] [spaceMethod=AsymWei]\n";
		exit(0);
	}
	int threshold = atoi(argv[2]);
	Gear::gear_init();

    int dim = 288;               // Dimension of the elements
    long max_elements = MAXELEMENTS;   // Maximum number of elements, should be known beforehand
    int M = 288;                 // Tightly connected with internal dimensionality of the data
                                // strongly affects the memory consumption
    int ef_construction = 200;  // Controls index search speed/build speed tradeoff
    printf("feature len:%d\n", FEALEN);
    printf("max elements:%d\n", max_elements);
    // Initing index
//    hnswlib::L2Space space(dim);
//    hnswlib::HammingSpace space(dim);
//    hnswlib::AsymHammingSpace space(dim);
    hnswlib::AsymWeightHammingSpace space(dim);

    hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, max_elements, M, ef_construction);

	FASTCDC* cdc = new FASTCDC(4096);
	DATA_IO f(argv[1], cdc);
	f.N = 0;
//	f.read_file();
	f.treaverse(argv[1]);
	cout<<"file chunks number: "<<f.N<<endl;
	map<XXH64_hash_t, int> dedup;
	list<ii> dedup_lazy_recipe;
	NetworkHash network(256);

	unsigned long long total = 0;
	f.time_check_start();
	for (int i = 0; i < f.N; ++i) {
		char* blockAddr = std::get<0>(f.trace[i]);
		int blockSize = std::get<1>(f.trace[i]);
		inputSize += blockSize;
//		printf("Block number:%d, block addr: 0x%lx, block size: %d\n", i, blockAddr, blockSize);
		XXH64_hash_t h = XXH64(blockAddr, blockSize, 0);

		if (dedup.count(h)) { // deduplication
			dedup_lazy_recipe.push_back({i, dedup[h]});
			procingIdx++;
			continue;
		}

		dedup[h] = i;

		if (network.push(blockAddr, blockSize, i)) {	//if match the batch size of network
			vector<pair<MYHASH, int*>> myhash = network.request();
			for (int j = 0; j < myhash.size(); ++j) {
				procingIdx++;
//				cout<<"procssing block: "<<procingIdx<<endl;
				RECIPE r;

				//first is hash value, second is chunk id
				MYHASH h = myhash[j].first;
				int index = *(myhash[j].second);

				char* chunkAddr = std::get<0>(f.trace[index]);
				int chunkSize = std::get<1>(f.trace[index]);
				int comp_self = LZ4_compress_default(chunkAddr, compressed,
							 chunkSize, 2 * BLOCK_SIZE);
				int dcomp_ann = INF;
				//get the index of similar chunk
				std::priority_queue<std::pair<float, hnswlib::labeltype>> result =
									 alg_hnsw->searchKnn(h, 1);
				hnswlib::labeltype dcomp_ann_ref;
				if(!result.empty()){
					dcomp_ann_ref = result.top().second;
					dcomp_ann = xdelta3_compress( chunkAddr,
							 chunkSize,
							 std::get<0>(f.trace[dcomp_ann_ref]),
							 std::get<1>(f.trace[dcomp_ann_ref]),
							 delta_compressed, 1);
				}
				alg_hnsw->addPoint(h, index);

				set_offset(r, total);

				//choose the minimum type of compress to store
				if (min(comp_self, chunkSize) > dcomp_ann) { // delta compress
					set_size(r, (unsigned long)(dcomp_ann - 1));
					set_ref(r, dcomp_ann_ref);
					set_flag(r, 0b11);
					f.write_file(delta_compressed, dcomp_ann);
					total += dcomp_ann;
					xdeltaSavedSpace += chunkSize - dcomp_ann;
				} else {
					if (comp_self < chunkSize) { // self compress
						set_size(r, (unsigned long)(comp_self - 1));
						set_flag(r, 0b01);
						f.write_file(compressed, comp_self);
						total += comp_self;
					}
					else { // no compress
						set_flag(r, 0b00);
						f.write_file(chunkAddr, chunkSize);
						total += chunkSize;
					}
				}
#ifdef PRINT_HASH
				cout << index << ' ' << h << '\n';
#endif

				//porcess the duplication
				while (!dedup_lazy_recipe.empty() && dedup_lazy_recipe.begin()->first < index) {
					RECIPE rr;
					set_ref(rr, dedup_lazy_recipe.begin()->second);
					set_flag(rr, 0b10);
					f.recipe_insert(rr);
					dedup_lazy_recipe.pop_front();
				}
				f.recipe_insert(r);
			}
		}
	}
	// LAST REQUEST
	{
		vector<pair<MYHASH, int*>> myhash = network.request();
		for (int j = 0; j < myhash.size(); ++j) {
			RECIPE r;

			MYHASH h = myhash[j].first;
			int index = *(myhash[j].second);

			char* chunkAddr = std::get<0>(f.trace[index]);
			int chunkSize = std::get<1>(f.trace[index]);

			int comp_self = LZ4_compress_default(chunkAddr, compressed, chunkSize, 2 * BLOCK_SIZE);
			int dcomp_ann = INF;
			//get the index of similar chunk
			std::priority_queue<std::pair<float, hnswlib::labeltype>> result =
								 alg_hnsw->searchKnn(h, 1);
			hnswlib::labeltype dcomp_ann_ref = result.top().second;
			alg_hnsw->addPoint(h, index);

			if (dcomp_ann_ref != -1) {
				dcomp_ann = xdelta3_compress(chunkAddr,
					 chunkSize,
					 std::get<0>(f.trace[dcomp_ann_ref]),
					 std::get<1>(f.trace[dcomp_ann_ref]),
					 delta_compressed,
					 1);
			}

			set_offset(r, total);

			if (min(comp_self, chunkSize) > dcomp_ann) { // delta compress
				set_size(r, (unsigned long)(dcomp_ann - 1));
				set_ref(r, dcomp_ann_ref);
				set_flag(r, 0b11);
				f.write_file(delta_compressed, dcomp_ann);
				total += dcomp_ann;
			} else {
				if (comp_self < chunkSize) { // self compress
					set_size(r, (unsigned long)(comp_self - 1));
					set_flag(r, 0b01);
					f.write_file(compressed, comp_self);
					total += comp_self;
				} else { // no compress
					set_flag(r, 0b00);
					f.write_file(chunkAddr, chunkSize);
					total += chunkSize;
				}
			}
#ifdef PRINT_HASH
				cout << index << ' ' << h << '\n';
#endif

			while (!dedup_lazy_recipe.empty() && dedup_lazy_recipe.begin()->first < index) {
				RECIPE rr;
				set_ref(rr, dedup_lazy_recipe.begin()->second);
				set_flag(rr, 0b10);
				f.recipe_insert(rr);
				dedup_lazy_recipe.pop_front();
			}
			f.recipe_insert(r);
		}
	}
	f.recipe_write();
	cout << "Total time: " << f.time_check_end() << "us\n";

	printf("ANN %s with model %s\n", argv[1], argv[2]);
	printf("xdelta saved space: %d, (%.2lf%%)\n", xdeltaSavedSpace, (double)xdeltaSavedSpace*100/(inputSize));
	printf("Input size: %lu, final size: %llu (%.2lf%%)\n", inputSize, total, (double)total * 100 / inputSize);
}