#include  <gtest/gtest.h>

#include "kv/kv_store.h"

TEST(KVStoreExecute, PutAndGetSimpleValue)
{
    KVStore kv;
    Request r1;
    r1.op = Operation::Put;
    r1.key = "a";
    r1.value = "1";
    Request r2;
    r2.op = Operation::Get;
    r2.key = "a";

    kv.execute(r1);
    Result res1 = kv.execute(r2);
    EXPECT_EQ(res1.found, Status::Ok);
    EXPECT_EQ("1", res1.value);
}

TEST(KVStoreExecute, PutValueWithoutKey)
{
    KVStore kv;
    Request r1;
    r1.op = Operation::Put;
    r1.value = "1";

    Result res = kv.execute(r1);
    EXPECT_EQ(res.found, Status::InvalidKey);
}

TEST(KVStoreExecute, PutValueAndGetAnother)
{
    KVStore kv;
    Request r1;
    r1.op = Operation::Put;
    r1.key = "a";
    r1.value = "1";

    Request r2;
    r2.op = Operation::Get;
    r2.key = "b";

    Result res = kv.execute(r1);
    EXPECT_EQ(res.found, Status::Ok);

    Result res2 = kv.execute(r2);
    EXPECT_EQ(res2.found, Status::NotFound);
}

TEST(KVStoreExecute, Put2DifferentValueWithSameKey)
{
    KVStore kv;
    Request r1;
    r1.op = Operation::Put;
    r1.key = "a";
    r1.value = "1";

    Request r2;
    r2.op = Operation::Put;
    r2.key = "a";
    r2.value = "2";

    Request r3;
    r3.op = Operation::Get;
    r3.key = "a";

    Result res = kv.execute(r1);
    EXPECT_EQ(res.found, Status::Ok);

    Result res2 = kv.execute(r2);
    EXPECT_EQ(res2.found, Status::Ok);

    Result res3 = kv.execute(r3);
    EXPECT_EQ(res3.value, "2");
}

TEST(KVStoreDigest, SameValuesSameOrder)
{
    KVStore kv1;
    KVStore kv2;

    Request r1{.op = Operation::Put, .key = "a", .value = "1"};
    Request r2{.op = Operation::Put, .key = "b", .value = "22"};

    kv1.execute(r1);
    kv2.execute(r1);
    kv1.execute(r2);
    kv2.execute(r2);

    EXPECT_EQ(kv1.digest(), kv2.digest());
}

TEST(KVStoreDigest, SameValuesDifferentOrder)
{
    KVStore kv1;
    KVStore kv2;

    Request r1{.op = Operation::Put, .key = "a", .value = "1"};
    Request r2{.op = Operation::Put, .key = "b", .value = "22"};

    kv1.execute(r1);
    kv1.execute(r2);
    kv2.execute(r2);
    kv2.execute(r1);

    EXPECT_EQ(kv1.digest(), kv2.digest());
}

TEST(KVStoreDigest, SameKeyDifferentValue)
{
    KVStore kv1;
    KVStore kv2;

    Request r1{.op = Operation::Put, .key = "a", .value = "1"};
    Request r2{.op = Operation::Put, .key = "a", .value = "2"};

    kv1.execute(r1);
    kv2.execute(r2);

    EXPECT_NE(kv1.digest(), kv2.digest());
}

TEST(KVStoreDigest, LengthIsImportant)
{
    KVStore kv1;
    KVStore kv2;

    Request r1{.op = Operation::Put, .key = "a", .value = "bc"};
    Request r2{.op = Operation::Put, .key = "ab", .value = "c"};

    kv1.execute(r1);
    kv2.execute(r2);

    EXPECT_NE(kv1.digest(), kv2.digest());
}

TEST(KVStoreDigest, TwoDifferentObjects)
{
    KVStore kv1;
    KVStore kv2;

    Request r1{.op = Operation::Put, .key = "a", .value = "1"};
    Request r2{.op = Operation::Put, .key = "b", .value = "c"};

    kv1.execute(r1);
    kv2.execute(r2);

    EXPECT_NE(kv1.digest(), kv2.digest());
}

TEST(KVStoreDigest, EmptyStore)
{
    KVStore kv1;
    KVStore kv2;

    EXPECT_EQ(kv1.digest(), kv2.digest());
}

