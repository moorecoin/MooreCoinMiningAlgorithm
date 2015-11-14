// copyright (c) 2012-2013 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "serialize.h"
#include "streams.h"
#include "hash.h"
#include "test/test_moorecoin.h"

#include <stdint.h>

#include <boost/test/unit_test.hpp>

using namespace std;

boost_fixture_test_suite(serialize_tests, basictestingsetup)

boost_auto_test_case(sizes)
{
    boost_check_equal(sizeof(char), getserializesize(char(0), 0));
    boost_check_equal(sizeof(int8_t), getserializesize(int8_t(0), 0));
    boost_check_equal(sizeof(uint8_t), getserializesize(uint8_t(0), 0));
    boost_check_equal(sizeof(int16_t), getserializesize(int16_t(0), 0));
    boost_check_equal(sizeof(uint16_t), getserializesize(uint16_t(0), 0));
    boost_check_equal(sizeof(int32_t), getserializesize(int32_t(0), 0));
    boost_check_equal(sizeof(uint32_t), getserializesize(uint32_t(0), 0));
    boost_check_equal(sizeof(int64_t), getserializesize(int64_t(0), 0));
    boost_check_equal(sizeof(uint64_t), getserializesize(uint64_t(0), 0));
    boost_check_equal(sizeof(float), getserializesize(float(0), 0));
    boost_check_equal(sizeof(double), getserializesize(double(0), 0));
    // bool is serialized as char
    boost_check_equal(sizeof(char), getserializesize(bool(0), 0));

    // sanity-check getserializesize and c++ type matching
    boost_check_equal(getserializesize(char(0), 0), 1);
    boost_check_equal(getserializesize(int8_t(0), 0), 1);
    boost_check_equal(getserializesize(uint8_t(0), 0), 1);
    boost_check_equal(getserializesize(int16_t(0), 0), 2);
    boost_check_equal(getserializesize(uint16_t(0), 0), 2);
    boost_check_equal(getserializesize(int32_t(0), 0), 4);
    boost_check_equal(getserializesize(uint32_t(0), 0), 4);
    boost_check_equal(getserializesize(int64_t(0), 0), 8);
    boost_check_equal(getserializesize(uint64_t(0), 0), 8);
    boost_check_equal(getserializesize(float(0), 0), 4);
    boost_check_equal(getserializesize(double(0), 0), 8);
    boost_check_equal(getserializesize(bool(0), 0), 1);
}

boost_auto_test_case(floats_conversion)
{
    // choose values that map unambigiously to binary floating point to avoid
    // rounding issues at the compiler side.
    boost_check_equal(ser_uint32_to_float(0x00000000), 0.0f);
    boost_check_equal(ser_uint32_to_float(0x3f000000), 0.5f);
    boost_check_equal(ser_uint32_to_float(0x3f800000), 1.0f);
    boost_check_equal(ser_uint32_to_float(0x40000000), 2.0f);
    boost_check_equal(ser_uint32_to_float(0x40800000), 4.0f);
    boost_check_equal(ser_uint32_to_float(0x44444444), 785.066650390625f);

    boost_check_equal(ser_float_to_uint32(0.0f), 0x00000000);
    boost_check_equal(ser_float_to_uint32(0.5f), 0x3f000000);
    boost_check_equal(ser_float_to_uint32(1.0f), 0x3f800000);
    boost_check_equal(ser_float_to_uint32(2.0f), 0x40000000);
    boost_check_equal(ser_float_to_uint32(4.0f), 0x40800000);
    boost_check_equal(ser_float_to_uint32(785.066650390625f), 0x44444444);
}

boost_auto_test_case(doubles_conversion)
{
    // choose values that map unambigiously to binary floating point to avoid
    // rounding issues at the compiler side.
    boost_check_equal(ser_uint64_to_double(0x0000000000000000ull), 0.0);
    boost_check_equal(ser_uint64_to_double(0x3fe0000000000000ull), 0.5);
    boost_check_equal(ser_uint64_to_double(0x3ff0000000000000ull), 1.0);
    boost_check_equal(ser_uint64_to_double(0x4000000000000000ull), 2.0);
    boost_check_equal(ser_uint64_to_double(0x4010000000000000ull), 4.0);
    boost_check_equal(ser_uint64_to_double(0x4088888880000000ull), 785.066650390625);

    boost_check_equal(ser_double_to_uint64(0.0), 0x0000000000000000ull);
    boost_check_equal(ser_double_to_uint64(0.5), 0x3fe0000000000000ull);
    boost_check_equal(ser_double_to_uint64(1.0), 0x3ff0000000000000ull);
    boost_check_equal(ser_double_to_uint64(2.0), 0x4000000000000000ull);
    boost_check_equal(ser_double_to_uint64(4.0), 0x4010000000000000ull);
    boost_check_equal(ser_double_to_uint64(785.066650390625), 0x4088888880000000ull);
}
/*
python code to generate the below hashes:

    def reversed_hex(x):
        return binascii.hexlify(''.join(reversed(x)))
    def dsha256(x):
        return hashlib.sha256(hashlib.sha256(x).digest()).digest()

    reversed_hex(dsha256(''.join(struct.pack('<f', x) for x in range(0,1000)))) == '8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c'
    reversed_hex(dsha256(''.join(struct.pack('<d', x) for x in range(0,1000)))) == '43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96'
*/
boost_auto_test_case(floats)
{
    cdatastream ss(ser_disk, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << float(i);
    }
    boost_check(hash(ss.begin(), ss.end()) == uint256s("8e8b4cf3e4df8b332057e3e23af42ebc663b61e0495d5e7e32d85099d7f3fe0c"));

    // decode
    for (int i = 0; i < 1000; i++) {
        float j;
        ss >> j;
        boost_check_message(i == j, "decoded:" << j << " expected:" << i);
    }
}

boost_auto_test_case(doubles)
{
    cdatastream ss(ser_disk, 0);
    // encode
    for (int i = 0; i < 1000; i++) {
        ss << double(i);
    }
    boost_check(hash(ss.begin(), ss.end()) == uint256s("43d0c82591953c4eafe114590d392676a01585d25b25d433557f0d7878b23f96"));

    // decode
    for (int i = 0; i < 1000; i++) {
        double j;
        ss >> j;
        boost_check_message(i == j, "decoded:" << j << " expected:" << i);
    }
}

boost_auto_test_case(varints)
{
    // encode

    cdatastream ss(ser_disk, 0);
    cdatastream::size_type size = 0;
    for (int i = 0; i < 100000; i++) {
        ss << varint(i);
        size += ::getserializesize(varint(i), 0, 0);
        boost_check(size == ss.size());
    }

    for (uint64_t i = 0;  i < 100000000000ull; i += 999999937) {
        ss << varint(i);
        size += ::getserializesize(varint(i), 0, 0);
        boost_check(size == ss.size());
    }

    // decode
    for (int i = 0; i < 100000; i++) {
        int j = -1;
        ss >> varint(j);
        boost_check_message(i == j, "decoded:" << j << " expected:" << i);
    }

    for (uint64_t i = 0;  i < 100000000000ull; i += 999999937) {
        uint64_t j = -1;
        ss >> varint(j);
        boost_check_message(i == j, "decoded:" << j << " expected:" << i);
    }
}

boost_auto_test_case(compactsize)
{
    cdatastream ss(ser_disk, 0);
    vector<char>::size_type i, j;

    for (i = 1; i <= max_size; i *= 2)
    {
        writecompactsize(ss, i-1);
        writecompactsize(ss, i);
    }
    for (i = 1; i <= max_size; i *= 2)
    {
        j = readcompactsize(ss);
        boost_check_message((i-1) == j, "decoded:" << j << " expected:" << (i-1));
        j = readcompactsize(ss);
        boost_check_message(i == j, "decoded:" << j << " expected:" << i);
    }
}

static bool iscanonicalexception(const std::ios_base::failure& ex)
{
    std::ios_base::failure expectedexception("non-canonical readcompactsize()");

    // the string returned by what() can be different for different platforms.
    // instead of directly comparing the ex.what() with an expected string,
    // create an instance of exception to see if ex.what() matches 
    // the expected explanatory string returned by the exception instance. 
    return strcmp(expectedexception.what(), ex.what()) == 0;
}


boost_auto_test_case(noncanonical)
{
    // write some non-canonical compactsize encodings, and
    // make sure an exception is thrown when read back.
    cdatastream ss(ser_disk, 0);
    vector<char>::size_type n;

    // zero encoded with three bytes:
    ss.write("\xfd\x00\x00", 3);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);

    // 0xfc encoded with three bytes:
    ss.write("\xfd\xfc\x00", 3);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);

    // 0xfd encoded with three bytes is ok:
    ss.write("\xfd\xfd\x00", 3);
    n = readcompactsize(ss);
    boost_check(n == 0xfd);

    // zero encoded with five bytes:
    ss.write("\xfe\x00\x00\x00\x00", 5);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);

    // 0xffff encoded with five bytes:
    ss.write("\xfe\xff\xff\x00\x00", 5);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);

    // zero encoded with nine bytes:
    ss.write("\xff\x00\x00\x00\x00\x00\x00\x00\x00", 9);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);

    // 0x01ffffff encoded with nine bytes:
    ss.write("\xff\xff\xff\xff\x01\x00\x00\x00\x00", 9);
    boost_check_exception(readcompactsize(ss), std::ios_base::failure, iscanonicalexception);
}

boost_auto_test_case(insert_delete)
{
    // test inserting/deleting bytes.
    cdatastream ss(ser_disk, 0);
    boost_check_equal(ss.size(), 0);

    ss.write("\x00\x01\x02\xff", 4);
    boost_check_equal(ss.size(), 4);

    char c = (char)11;

    // inserting at beginning/end/middle:
    ss.insert(ss.begin(), c);
    boost_check_equal(ss.size(), 5);
    boost_check_equal(ss[0], c);
    boost_check_equal(ss[1], 0);

    ss.insert(ss.end(), c);
    boost_check_equal(ss.size(), 6);
    boost_check_equal(ss[4], (char)0xff);
    boost_check_equal(ss[5], c);

    ss.insert(ss.begin()+2, c);
    boost_check_equal(ss.size(), 7);
    boost_check_equal(ss[2], c);

    // delete at beginning/end/middle
    ss.erase(ss.begin());
    boost_check_equal(ss.size(), 6);
    boost_check_equal(ss[0], 0);

    ss.erase(ss.begin()+ss.size()-1);
    boost_check_equal(ss.size(), 5);
    boost_check_equal(ss[4], (char)0xff);

    ss.erase(ss.begin()+1);
    boost_check_equal(ss.size(), 4);
    boost_check_equal(ss[0], 0);
    boost_check_equal(ss[1], 1);
    boost_check_equal(ss[2], 2);
    boost_check_equal(ss[3], (char)0xff);

    // make sure getandclear does the right thing:
    cserializedata d;
    ss.getandclear(d);
    boost_check_equal(ss.size(), 0);
}

boost_auto_test_suite_end()
