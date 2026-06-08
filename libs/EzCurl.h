#pragma once


#include <curl/curl.h>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>
#include <cstring>

#include "Defines.h"
#include "MemBuffer.h"
#include "StrUtils.h"

namespace libs {
    static size_t writeFnCallback(char *data, size_t size, size_t nmemb, void *userdata) {
        // fprintf(stdout, "writeFnCallback: %ld\n", size * nmemb);
        MemBuf* buf = (MemBuf*) userdata;
        buf->write(data, size * nmemb);
        return size * nmemb;
    }

    static size_t readFnCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
        // fprintf(stdout, "readFnCallback: %ld\n", size * nmemb);
        MemBuf* buf = (MemBuf*) userdata;
        return buf->read(ptr, size * nmemb);
    }
    class CurlWrapper {
    public:
        struct CurlHeaders {
            CurlHeaders() : headers(nullptr) {}
            ~CurlHeaders() {
                if (headers)
                    curl_slist_free_all(headers);
            }
            void addHeader(const char *h) {
                headers = curl_slist_append(headers, h);
            }
            void addHeader(const std::string &h) {
                headers = curl_slist_append(headers, h.c_str());
            }

            void addHeader(const std::string& key, const std::string& value) {
                headers = curl_slist_append(headers, simple_format("{}: {}", key, value).c_str());
            }
            curl_slist *handle() const {
                return headers;
            }
            curl_slist *headers;
        };

    public:
        static void create() {
            if (refCnt.fetch_add(1, std::memory_order_acq_rel) == 0) {
                curl_global_init(CURL_GLOBAL_ALL);
                return;
            }
        }

        static void destroy() {
            if (refCnt.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                curl_global_cleanup();
            }
        }

    public:
        CurlWrapper() : curl(), headers() {
            CurlWrapper::create();
            curl = curl_easy_init();
            headers.addHeader("Content-Type: application/vnd.apache.thrift.json; charset=UTF-8");
            headers.addHeader("Accept: application/vnd.apache.thrift.json; charset=utf-8");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.handle());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 60000);
        }

        ~CurlWrapper() {
            curl_easy_cleanup(curl);
            CurlWrapper::destroy();
        }

        std::pair<int, std::string> Post(const std::string &url, const std::string &body) {
            // return < 0 if curl error code, else HTTPCode
            auto membuf = std::make_unique<MemBuf>();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, body.size());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFnCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) membuf.get());

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                return {-res, {}};
            }
            long resCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resCode);
            return {static_cast<int>(resCode), static_cast<std::string>(*membuf)};
        }

        std::pair<int, std::string> Get(const std::string &url) {
            auto membuf = std::make_shared<MemBuf>();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFnCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) membuf.get());

            auto res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));
                return {-res, {}};
            }
            long resCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resCode);
            return {static_cast<int>(resCode), static_cast<std::string>(*membuf)};
        }

        void setProxy(const std::string &proxy) {
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
        }

        void setWriteCbData(std::shared_ptr<MemBuf> userdata) {
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFnCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) userdata.get());
        }

        CURL *handle() {
            return curl;
        }

    private:
        CURL *curl;
        CurlHeaders headers;
        static inline std::atomic_int refCnt{0};
    };
}