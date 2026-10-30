#include  <gtest/gtest.h>
#include <sodium.h>

#include <string>

TEST(Setup, LibsodiumInitialises)
{
    EXPECT_GE(sodium_init(), 0);
}

TEST(Setup, Sha256MatchesKnownAnswer)
{
    ASSERT_GE(sodium_init(), 0);

    const std::string input = "abc";
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(input.data()), input.size());
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    sodium_bin2hex(hex, sizeof(hex), hash, sizeof(hash));

    EXPECT_EQ(std::string(hex), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Setup, Sha256DoesntMatchKnownAnswer)
{
    ASSERT_GE(sodium_init(), 0);

    const std::string input = "abc";
    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, reinterpret_cast<const unsigned char*>(input.data()), input.size());
    char hex[crypto_hash_sha256_BYTES * 2 + 1];
    sodium_bin2hex(hex, sizeof(hex), hash, sizeof(hash));

    EXPECT_NE(std::string(hex), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ac");
}