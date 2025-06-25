/*
 * File:   RandomUtils.h
 * Author: thaipq
 *
 * Created on Thu May 08 2025 10:52:08 AM
 */

#ifndef LIBS_RANDOMUTILS_H
#define LIBS_RANDOMUTILS_H

#include <random>

namespace libs {

    static std::string random_str(int32_t len) {
        static const char alphanum[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";
        std::random_device rd;
        std::mt19937 gen(rd());
        static thread_local std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2); // skip '\0'
        std::string ret;
        ret.resize(len);
        for (int i = 0; i < len; ++i) {
            ret[i] = alphanum[dis(gen)];
        }
        return std::move(ret);
    }

    template<typename NumType = int>
    NumType random_num(const NumType from, const NumType to) {
        // closed interval
        using dist_type = std::uniform_int_distribution<NumType>;
        std::random_device rd;
        std::default_random_engine seedGen(rd());
        static thread_local dist_type dis;
        return dis(seedGen, typename dist_type::param_type{from, to});
    }

}

#endif// LIBS_RANDOMUTILS_H