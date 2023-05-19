#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <openssl/md5.h>

#include "hnswlib/hnswlib.h"
#include "../xxhash.h"
#include "./NGT/Index.h"

// Network

//#define HASH_SIZE 128
#define HASH_SIZE 256
#define FEA_LEN (HASH_SIZE/8 + HASH_SIZE)
#define EFFCTIVE_LEN 191
#define WINDOW_SIZE 4
#define WINDOW_MASK ((1ULL<<(WINDOW_SIZE*8)) - 1)
typedef uint32_t* MYHASH;

class Gear{
    public:
        static uint64_t mask;
        static uint64_t g_gear_matrix[SymbolCount];
        static void gear_init();
        static int gear_chunk_data(unsigned char *p, int n, MYHASH hash);
        static int countOnes(MYHASH hash);
    private:
        static void setbit(MYHASH hash, int index);
};

uint64_t Gear::g_gear_matrix[] = {0};
uint64_t Gear::mask = g_condition_mask[5];

int Gear::countOnes(MYHASH hash){
    int ret = 0;
    for(int i= 0; i< HASH_SIZE/8; i++){
        uint t = hash[i];
        while(t){
            ret++;
            t >>= 1;
        }
    }

    return ret;
}

void Gear::setbit(MYHASH hash, int index){
    int byteOff = index/8;
    int offset = index%8;
    switch (offset) {
    case 0:
    	hash[byteOff] |= 0x1;
        break;
    
    case 1:
    	hash[byteOff] |= 0x2;
        break;
    
    case 2:
    	hash[byteOff] |= 0x4;
        break;
    
    case 3:
    	hash[byteOff] |= 0x8;
        break;
    
    case 4:
    	hash[byteOff] |= 0x10;
        break;
    
    case 5:
    	hash[byteOff] |= 0x20;
        break;
    
    case 6:
    	hash[byteOff] |= 0x40;
        break;
    
    case 7:
    	hash[byteOff] |= 0x80;
        break;
    
    default:
        printf("setbit error!!!\n");
        break;
    }
}

void Gear::gear_init(){
    char seed[SeedLength];
    for(int i=0; i<SymbolCount; i++){
        for(int j=0; j<SeedLength; j++){
            seed[j] = i;
        }

        unsigned char md5_result[DigistLength];

        MD5_CTX md5_ctx;
        MD5_Init(&md5_ctx);
        MD5_Update(&md5_ctx, seed, SeedLength);
        MD5_Final(md5_result, &md5_ctx);

        memcpy(&g_gear_matrix[i], md5_result, sizeof(uint64_t));
    }
}

std::array<int, HASH_SIZE> weight = {0};
int Gear::gear_chunk_data(unsigned char *p, int n, MYHASH hash){

    uint64_t fingerprint=0;
    int i=0;
    while(i < n){
        fingerprint = (fingerprint<<1) + (g_gear_matrix[p[i]]);
        i++;
        if(!(fingerprint & Gear::mask)){
            int index = (fingerprint & WINDOW_MASK)%EFFCTIVE_LEN;
            setbit(hash, index);
            int weightOffset = HASH_SIZE/8 + index;
            hash[weightOffset] += 1;
        }

    }
    return n;
}


class NetworkHash {
   private:
    int BATCH_SIZE;
    bool* memout;
    int* index;
    int cnt;
    class Gear *gear;

   public:
    std::vector<std::pair<MYHASH, int*>> retHash;

    NetworkHash(int BATCH_SIZE) {
        this->BATCH_SIZE = BATCH_SIZE;
        this->memout = new bool[BATCH_SIZE * HASH_SIZE];
        this->index = new int[BATCH_SIZE];
        this->cnt = 0;
        this->gear = new Gear();
//        for(int i = 0; i<BATCH_SIZE; i++){
//            retHash.push_bash(std::pair<MYHASH, int>(std::bitset<HASH_SIZE>{0b0} , 0));
//        }
    }
    ~NetworkHash() {
        delete[] this->memout;
        delete[] this->index;
    }
    bool push(char* ptr, int size, int label);
    std::vector<std::pair<MYHASH, int*>> request();
};


bool NetworkHash::push(char* ptr, int size, int label) {
    if(cnt == 0){
        for(int i=0; i<retHash.size(); i++){
            delete this->retHash[i].first;
            delete this->retHash[i].second;
        }
        retHash.clear();
    }

    MYHASH hash = (uint32_t*)calloc(FEA_LEN, sizeof(uint32_t));
//    MYHASH *hash = new std::array<uint32, FEA_LEN>(0x0);
//    std::bitset<HASH_SIZE> hash(0x0);
//    std::array<int, HASH_SIZE> weight = {0};
    int *index = new int(label);
    Gear::gear_chunk_data((unsigned char*)ptr, size, hash);
    retHash.push_back(std::pair<MYHASH, int*>(hash, index));
//    printf("Number of 1 in hash: %5d weight 13, 27, 38: %3d, %3d, %3d\n",
//            Gear::countOnes(hash), hash[HASH_SIZE/8+13-1],
//            hash[HASH_SIZE/8+27-1], hash[HASH_SIZE/8+38-1]);

    cnt++;
    if (cnt == BATCH_SIZE)
        return true;
    else
        return false;
}

// This function get the hash value into ret pairs
std::vector<std::pair<MYHASH, int*>> NetworkHash::request() {
    if (cnt == 0) return std::vector<std::pair<MYHASH, int*>>();
    cnt = 0;
    return retHash;
}

//// ANN
//class ANN {
//   private:
//    int ANN_SEARCH_CNT, LINEAR_SIZE, NUM_THREAD, THRESHOLD;
//    std::vector<MYHASH> linear;
//    std::unordered_map<MYHASH, std::vector<int>> hashtable;
//    NGT::Property* property;
//    NGT::Index* index;
//
//   public:
//    ANN(int ANN_SEARCH_CNT, int LINEAR_SIZE, int NUM_THREAD, int THRESHOLD,
//        NGT::Property* property, NGT::Index* index) {
//        this->ANN_SEARCH_CNT =
//            ANN_SEARCH_CNT;  // The number of candidates extract from ANN class
//        this->LINEAR_SIZE = LINEAR_SIZE;  // Size of linear buffer
//        this->NUM_THREAD = NUM_THREAD;
//        this->THRESHOLD = THRESHOLD;
//        this->property = property;
//        this->index = index;
//    }
//    int request(MYHASH h);
//    void insert(MYHASH h, int label);
//};
//
//// search the nearest point in ANN
//int ANN::request(MYHASH h) {
//    int dist = 999;
//    int ret = -1;
//
//    // scan cache list
//    for (int i = linear.size() - 1; i >= 0; --i) {
//        int nowdist = (linear[i] ^ h).count();  // hammin distance
//        if (dist > nowdist) {
//            dist = nowdist;
//            ret = hashtable[linear[i]].back();
//        }
//    }
//
//    //change the searched hash into uint array
//    std::vector<uint8_t> query;
//    for (int i = 0; i < property->dimension;
//         ++i) {  
//        query.push_back(
//            (uint8_t)((h << (HASH_SIZE - 8 * i - 8)) >> (HASH_SIZE - 8))
//                .to_ulong());
//    }
//
//    NGT::SearchQuery sc(query);
//    NGT::ObjectDistances objects;
//    sc.setResults(&objects);
//    sc.setSize(this->ANN_SEARCH_CNT);
//    sc.setEpsilon(0.2);
//
//    index->search(sc);  // here search the result
//    // process the search result
//    for (int i = 0; i < objects.size(); ++i) {  // what is the size of object?
//                                                // 0?
//        int nowdist = objects[i].distance;
//
//        if (dist > nowdist) {  // find better result
//            MYHASH now;
//
//            NGT::ObjectSpace& objectSpace = index->getObjectSpace();
//            uint8_t* object = static_cast<uint8_t*>(objectSpace.getObject(objects[i].id));
//            // copy the hash code into MYHASH now
//            for (int j = 0; j < objectSpace.getDimension(); ++j) {
//                for (int k = 0; k < 8; ++k) {
//                    if (object[j] & (1 << k)) {
//                        now.flip(8 * j + k);  
//                    }
//                }
//            }
//            dist = nowdist;
//            ret = hashtable[now].back();
//        } else if (dist == nowdist) { /** found same result, choose the one with bigger
//                                 index, but why use bigger index? */
//            MYHASH now;
//
//            NGT::ObjectSpace& objectSpace = index->getObjectSpace();
//            uint8_t* object = static_cast<uint8_t*>(objectSpace.getObject(objects[i].id));
//            for (int j = 0; j < objectSpace.getDimension(); ++j) {
//                for (int k = 0; k < 8; ++k) {
//                    if (object[j] & (1 << k)) {
//                        now.flip(8 * j + k);
//                    }
//                }
//            }
//            int nowindex = hashtable[now].back();
//
//            if (nowindex > ret) ret = nowindex;
//        }
//    }
//
//    if (dist <= THRESHOLD)
//        return ret;
//    else
//        return -1;
//}
//
//void ANN::insert(MYHASH h, int label) {
//    if (hashtable.count(h)) {
//        hashtable[h].push_back(label);
//        return;
//    }
//
//    hashtable[h].push_back(label);
//    linear.push_back(h);
//
//    //every time reach the LINEAR_SIZE, creat a new index using
//    if (linear.size() == LINEAR_SIZE) {
//        for (int i = 0; i < linear.size(); ++i) {
//            std::vector<uint8_t> query;
//            for (int j = 0; j < property->dimension; ++j) {
//                query.push_back(
//                    (uint8_t)((linear[i] << (HASH_SIZE - 8 * j - 8)) >>
//                              (HASH_SIZE - 8))
//                        .to_ulong());
//            }
//            index->append(query);
//        }
//        index->createIndex(NUM_THREAD);
//
//        linear.clear();
//    }
//}
//
//// The max size of dataset GB
//#define MAXDATASETSIZE 100
//#define MINICHUNKSIZE 4096
//#define MAXELEMENTS (MAXDATASETSIZE*1024*1024*1024/MINICHUNKSIZE)
//
//int main() {
//    int dim = 288;               // Dimension of the elements
//    int max_elements = 10000;   // Maximum number of elements, should be known beforehand
//    int M = 16;                 // Tightly connected with internal dimensionality of the data
//                                // strongly affects the memory consumption
//    int ef_construction = 200;  // Controls index search speed/build speed tradeoff
//    printf("feature len:%d\n", FEALEN);
//    // Initing index
////    hnswlib::L2Space space(dim);
////    hnswlib::HammingSpace space(dim);
////    hnswlib::AsymHammingSpace space(dim);
//    hnswlib::AsymWeightHammingSpace space(dim);
//
//    hnswlib::HierarchicalNSW<float>* alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, max_elements, M, ef_construction);
//
//    // Generate random data
//    std::mt19937 rng;
//    rng.seed(47);
//    std::uniform_real_distribution<> distrib_real;
//    float* data = new float[dim * max_elements];
//    for (int i = 0; i < dim * max_elements; i++) {
//        data[i] = distrib_real(rng);
//    }
//
//    // Add data to index
//    for (int i = 0; i < max_elements; i++) {
//        alg_hnsw->addPoint(data + i * dim, i);
//    }
//
//    // Query the elements for themselves and measure recall
//    float correct = 0;
//    for (int i = 0; i < max_elements; i++) {
//        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
//        hnswlib::labeltype label = result.top().second;
//        if (label == i) correct++;
//    }
//    float recall = correct / max_elements;
//    std::cout << "Recall: " << recall << "\n";
//
//    // Serialize index
//    std::string hnsw_path = "hnsw.bin";
//    alg_hnsw->saveIndex(hnsw_path);
//    delete alg_hnsw;
//
//    // Deserialize index and check recall
//    alg_hnsw = new hnswlib::HierarchicalNSW<float>(&space, hnsw_path);
//    correct = 0;
//    for (int i = 0; i < max_elements; i++) {
//        std::priority_queue<std::pair<float, hnswlib::labeltype>> result = alg_hnsw->searchKnn(data + i * dim, 1);
//        hnswlib::labeltype label = result.top().second;
//        if (label == i) correct++;
//    }
//    recall = (float)correct / max_elements;
//    std::cout << "Recall of deserialized index: " << recall << "\n";
//
//    delete[] data;
//    delete alg_hnsw;
//    return 0;
//}