#include "./dpf/dpf.h"
#include "datastore.h"

#include <chrono>
#include <iostream>

int testCorr() {
    size_t N = 20;
    datastore store;
    store.reserve(1ULL << N);
    for (size_t i = 0; i < (1ULL << N); i++) {
        store.push_back(_mm256_set_epi64x(i, i, i, i));
    }

    auto keys = DPF::Gen(123456, N);
    auto a = keys.first;
    auto b = keys.second;
    std::vector<uint8_t> aaaa = DPF::EvalFull8(a, N);
    std::vector<uint8_t> bbbb = DPF::EvalFull8(b, N);
    datastore::db_record answerA = store.answer_pir(aaaa);
    datastore::db_record answerB = store.answer_pir(bbbb);
    datastore::db_record answer = _mm256_xor_si256(answerA, answerB);
    if(_mm256_extract_epi64(answer, 0) == 123456) {
        return 0;
    } else {
        std::cout << "PIR answer wrong\n";
        return -1;
    }
}

int main(int argc, char** argv) {
    int res = 0;
    res |= testCorr();
    return res;
}