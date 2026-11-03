//
// Created by sam on 03/11/2026.
//
#include <array>
#include <iostream>
#include <sodium.h>
#include <string>

class Server
{
private:
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];

public:
    Server()
    {
        crypto_sign_keypair(this->pk, this->sk);
    }

    const unsigned char* getPublicKey() const
    {
        return pk;
    }

    std::array<unsigned char, crypto_sign_BYTES>  signMessage(const unsigned char* m, unsigned long long mlen)
    {
        std::array<unsigned char, crypto_sign_BYTES> sig;
        crypto_sign_detached(sig.data(), nullptr, m, mlen, this->sk);
        return sig;
    }

    bool verifySignature(const unsigned char* sig, const unsigned char* m, unsigned long long mlen, const unsigned char* public_key)
    {
        return (crypto_sign_verify_detached(sig, m, mlen, public_key) == 0);
    }

};

void printVerifySignature(bool verify)
{
    std::cout << ((verify)?"SUCCESS\n":"FAILS\n");
}

int main()
{
    if (sodium_init() < 0) {
        return -1;
    }

    Server s1;
    Server s2;
    const std::string m1 = "hello";
    const std::string m2 = "bonjour";
    std::string m3 = "hola";
    std::array<unsigned char, crypto_sign_BYTES> sig1;
    std::array<unsigned char, crypto_sign_BYTES> sig2;
    std::array<unsigned char, crypto_sign_BYTES> sig3;
    const auto* m1_data = reinterpret_cast<const unsigned char*>(m1.data());
    const auto* m2_data = reinterpret_cast<const unsigned char*>(m2.data());

    sig1 = s1.signMessage(m1_data, m1.length());
    std::cout << "Verify: GOOD CASE -> expected: SUCCESS\n";
    printVerifySignature(s2.verifySignature(sig1.data(), m1_data, m1.length(), s1.getPublicKey()));

    std::cout << "\nVerify: Public key is not correct -> expected: FAILS\n";
    printVerifySignature(s2.verifySignature(sig1.data(), m1_data, m1.length(), s2.getPublicKey()));

    std::cout << "\nVerify: Message is not the good one -> expected: FAILS\n";
    printVerifySignature(s2.verifySignature(sig1.data(), m2_data, m2.length(), s1.getPublicKey()));

    std::cout << "\nVerify: Good case with message 2 -> expected: SUCCESS\n";
    sig2 = s2.signMessage(m2_data, m2.length());
    printVerifySignature(s1.verifySignature(sig2.data(), m2_data, m2.length(), s2.getPublicKey()));

    std::cout << "\nVerify: with message altered (here of 1 bit) -> expected: FAILS\n";
    sig3 = s2.signMessage(reinterpret_cast<const unsigned char*>(m3.data()), m3.length());
    m3[0] = 'j';
    printVerifySignature(s1.verifySignature(sig3.data(), reinterpret_cast<const unsigned char*>(m3.data()), m3.length(), s2.getPublicKey()));

    std::cout << "\nVerify: change 1 byte of signature -> expected: FAILS\n";
    sig3 = s2.signMessage(reinterpret_cast<const unsigned char*>(m3.data()), m3.length());
    sig3[0] ^= 1;
    printVerifySignature(s1.verifySignature(sig3.data(), reinterpret_cast<const unsigned char*>(m3.data()), m3.length(), s2.getPublicKey()));

    return 0;
}