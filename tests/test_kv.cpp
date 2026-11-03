#include  <gtest/gtest.h>

#include "kv/kv_store.h"

TEST(KVStore, PutAndGetSimpleValue)
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

TEST(KVStore, PutValueWithoutKey)
{
    KVStore kv;
    Request r1;
    r1.op = Operation::Put;
    r1.value = "1";

    Result res = kv.execute(r1);
    EXPECT_EQ(res.found, Status::InvalidKey);
}

TEST(KVStore, PutValueAndGetAnother)
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

TEST(KVStore, Put2DifferentValueWithSameKey)
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

