#include "AES.h"
#include <openssl/evp.h>
#include <openssl/err.h>
#include <cstring>
#include <stdexcept>

const uint8_t fixed_key[16] = {36,156,50,234,92,230,49,9,174,170,205,160,98,236,29,243};
const uint8_t fixed_key2[16] = {209, 12, 199, 173, 29, 74, 44, 128, 194, 224, 14, 44, 2, 201, 110, 28};
const AES mFixedKey(fixed_key);
const AES mFixedKey2(fixed_key2);

AES::AES() : encrypt_ctx(nullptr), decrypt_ctx(nullptr), initialized(false) {
    uint8_t zerokey[16] = {0};
    setKey(zerokey);
}

AES::AES(const block& key_block) : encrypt_ctx(nullptr), decrypt_ctx(nullptr), initialized(false) {
    setKey(key_block);
}

AES::AES(const uint8_t* key_bytes) : encrypt_ctx(nullptr), decrypt_ctx(nullptr), initialized(false) {
    setKey(key_bytes);
}

AES::~AES() {
    cleanupContexts();
}

AES::AES(AES&& other) noexcept 
    : encrypt_ctx(other.encrypt_ctx), decrypt_ctx(other.decrypt_ctx), 
      initialized(other.initialized), key(other.key) {
    other.encrypt_ctx = nullptr;
    other.decrypt_ctx = nullptr;
    other.initialized = false;
}

AES& AES::operator=(AES&& other) noexcept {
    if (this != &other) {
        cleanupContexts();
        encrypt_ctx = other.encrypt_ctx;
        decrypt_ctx = other.decrypt_ctx;
        key = other.key;
        initialized = other.initialized;
        other.encrypt_ctx = nullptr;
        other.decrypt_ctx = nullptr;
        other.initialized = false;
    }
    return *this;
}

void AES::initContexts() {
    cleanupContexts();
    
    encrypt_ctx = EVP_CIPHER_CTX_new();
    decrypt_ctx = EVP_CIPHER_CTX_new();
    
    if (!encrypt_ctx || !decrypt_ctx) {
        throw std::runtime_error("Failed to create EVP contexts");
    }
    
    if (EVP_EncryptInit_ex(encrypt_ctx, EVP_aes_128_ecb(), nullptr, (const unsigned char*)&key, nullptr) != 1) {
        cleanupContexts();
        throw std::runtime_error("Failed to initialize encryption context");
    }
    
    if (EVP_DecryptInit_ex(decrypt_ctx, EVP_aes_128_ecb(), nullptr, (const unsigned char*)&key, nullptr) != 1) {
        cleanupContexts();
        throw std::runtime_error("Failed to initialize decryption context");
    }
    
    EVP_CIPHER_CTX_set_padding(encrypt_ctx, 0);
    EVP_CIPHER_CTX_set_padding(decrypt_ctx, 0);
    
    initialized = true;
}

void AES::cleanupContexts() {
    if (encrypt_ctx) {
        EVP_CIPHER_CTX_free(encrypt_ctx);
        encrypt_ctx = nullptr;
    }
    if (decrypt_ctx) {
        EVP_CIPHER_CTX_free(decrypt_ctx);
        decrypt_ctx = nullptr;
    }
    initialized = false;
}

void AES::setKey(const block& key_block) {
    memcpy(&key, &key_block, sizeof(block));
    initContexts();
}

void AES::setKey(const uint8_t* key_bytes) {
    memcpy(&key, key_bytes, 16);
    initContexts();
}

void AES::encryptECB(const block& plaintext, block& ciphertext) const {
    if (!initialized) {
        throw std::runtime_error("AES context not initialized");
    }
    
    int len;
    if (EVP_EncryptUpdate(encrypt_ctx, (unsigned char*)&ciphertext, &len, 
                         (const unsigned char*)&plaintext, 16) != 1) {
        throw std::runtime_error("Encryption failed");
    }
}

void AES::encryptECB_MMO(const block& plaintext, block& ciphertext) const {
    encryptECB(plaintext, ciphertext);
    ciphertext = _mm_xor_si128(ciphertext, plaintext);  // MMO construction
}

void AES::decryptECB(const block& ciphertext, block& plaintext) const {
    if (!initialized) {
        throw std::runtime_error("AES context not initialized");
    }
    
    int len;
    if (EVP_DecryptUpdate(decrypt_ctx, (unsigned char*)&plaintext, &len, 
                         (const unsigned char*)&ciphertext, 16) != 1) {
        throw std::runtime_error("Decryption failed");
    }
}

void AES::encryptECBBlocks(const block* plaintexts, uint64_t blockLength, block* ciphertexts) const {
    if (!initialized) {
        throw std::runtime_error("AES context not initialized");
    }
    
    if (blockLength >= 8) {
        int len;
        if (EVP_EncryptUpdate(encrypt_ctx, 
                             (unsigned char*)ciphertexts, &len,
                             (const unsigned char*)plaintexts, 
                             blockLength * 16) != 1) {
            throw std::runtime_error("Batch encryption failed");
        }
    } else {
        for (uint64_t i = 0; i < blockLength; ++i) {
            encryptECB(plaintexts[i], ciphertexts[i]);
        }
    }
}

void AES::encryptECB_MMO_Blocks(const block* plaintexts, uint64_t blockLength, block* ciphertexts) const {
    encryptECBBlocks(plaintexts, blockLength, ciphertexts);
    
    const uint64_t step = 8;
    uint64_t idx = 0;
    uint64_t length = blockLength - blockLength % step;
    
    for (; idx < length; idx += step) {
        ciphertexts[idx + 0] = _mm_xor_si128(ciphertexts[idx + 0], plaintexts[idx + 0]);
        ciphertexts[idx + 1] = _mm_xor_si128(ciphertexts[idx + 1], plaintexts[idx + 1]);
        ciphertexts[idx + 2] = _mm_xor_si128(ciphertexts[idx + 2], plaintexts[idx + 2]);
        ciphertexts[idx + 3] = _mm_xor_si128(ciphertexts[idx + 3], plaintexts[idx + 3]);
        ciphertexts[idx + 4] = _mm_xor_si128(ciphertexts[idx + 4], plaintexts[idx + 4]);
        ciphertexts[idx + 5] = _mm_xor_si128(ciphertexts[idx + 5], plaintexts[idx + 5]);
        ciphertexts[idx + 6] = _mm_xor_si128(ciphertexts[idx + 6], plaintexts[idx + 6]);
        ciphertexts[idx + 7] = _mm_xor_si128(ciphertexts[idx + 7], plaintexts[idx + 7]);
    }
    
    for (; idx < blockLength; ++idx) {
        ciphertexts[idx] = _mm_xor_si128(ciphertexts[idx], plaintexts[idx]);
    }
}

void AES::encryptCTR(uint64_t baseIdx, uint64_t blockLength, block* ciphertext) const {
    if (!initialized) {
        throw std::runtime_error("AES context not initialized");
    }
    
    std::vector<block> counters(blockLength);
    for (uint64_t i = 0; i < blockLength; ++i) {
        counters[i] = _mm_set_epi64x(0, baseIdx + i);
    }
    
    encryptECBBlocks(counters.data(), blockLength, ciphertext);
}