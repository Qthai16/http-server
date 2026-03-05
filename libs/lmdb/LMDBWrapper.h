/*
 * File:   LMDBWrapper.h
 * Author: thaipq
 *
 * Created on Thu May 22 2025 2:15:46 PM
 */

#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <thread>
#include <chrono>
#include <lmdb.h>

#include "Defines.h"
#include "FileUtils.h"

#if __cplusplus >= 201703L
#include <shared_mutex>
using RWMutex = std::shared_mutex;
#define SCOPED_READ_LOCK(mtx) std::shared_lock LIB_CONCAT(rlock, __LINE__)(mtx)
#define SCOPED_WRITE_LOCK(mtx) std::lock_guard LIB_CONCAT(wlock, __LINE__)(mtx)
#else
// todo: may use boost::shared_mutex or folly rw spin lock
#endif

// todo: multi put key/value: single txn, put multi key/value
// todo: set sync/no sync env flag
// todo: optimization, could using mdb_txn_renew + mdb_txn_reset pair
// todo: check open db voi flag: MDB_WRITEMAP|MDB_MAPASYNC

#define LMDB_DB_PERM          0664
#define lmdb_void_ptr_cast(p) ((void *) p)

// #define LMDB_LOG_ASSERT       LOGE("assert file[%s] line[%d]", __FILE__, __LINE__);
#ifdef DEBUG
#define LMDBLOG_O(fmt, ...)                  \
    do {                                     \
        fprintf(stdout, fmt, ##__VA_ARGS__); \
        fprintf(stdout, "\n");               \
    } while (false)
#define LMDBLOG_W(fmt, ...)                  \
    do {                                     \
        fprintf(stderr, fmt, ##__VA_ARGS__); \
        fprintf(stderr, "\n");               \
    } while (false)
#else
// todo: use logger
#define LMDBLOG_O(fmt, ...)
#define LMDBLOG_W(fmt, ...)
// #define LMDBLOG_O(fmt, ...) Logger::I().info(fmt, ##__VA_ARGS__)
// #define LMDBLOG_W(fmt, ...) Logger::I().warn(fmt, ##__VA_ARGS__)
#endif

namespace libs {
namespace lmdb_wrapper {
    static constexpr size_t kDefaultMaxSize = (2ul * 1024ul * 1024ul * 1024ul);
    struct WrapperCtx {
        // for auto scale maxsize when db full
        RWMutex rwLock_;
        std::atomic_int wrTxnCnt_;
        std::atomic_int rdTxnCnt_;

        int total_txn_count() const {
            return wrTxnCnt_.load() + rdTxnCnt_.load();
        }
    };

    struct TxnWrCounter {
        explicit TxnWrCounter(MDB_env *env) : env_(env) {
            auto ctx = (WrapperCtx *) mdb_env_get_userctx(env_);
            ctx->wrTxnCnt_.fetch_add(1, std::memory_order_acq_rel);
        }
        TxnWrCounter(const TxnWrCounter &) = delete;
        TxnWrCounter &operator=(const TxnWrCounter &) = delete;
        TxnWrCounter(TxnWrCounter &&) = delete;
        TxnWrCounter &operator=(TxnWrCounter &&) = delete;
        ~TxnWrCounter() {
            auto ctx = (WrapperCtx *) mdb_env_get_userctx(env_);
            ctx->wrTxnCnt_.fetch_sub(1, std::memory_order_acq_rel);
        }
        MDB_env *env_;
    };

    struct TxnRdCounter {
        explicit TxnRdCounter(MDB_env *env) : env_(env) {
            auto ctx = (WrapperCtx *) mdb_env_get_userctx(env_);
            ctx->rdTxnCnt_.fetch_add(1, std::memory_order_acq_rel);
        }
        TxnRdCounter(const TxnRdCounter &) = delete;
        TxnRdCounter &operator=(const TxnRdCounter &) = delete;
        TxnRdCounter(TxnRdCounter &&) = delete;
        TxnRdCounter &operator=(TxnRdCounter &&) = delete;
        ~TxnRdCounter() {
            auto ctx = (WrapperCtx *) mdb_env_get_userctx(env_);
            ctx->rdTxnCnt_.fetch_sub(1, std::memory_order_acq_rel);
        }
        MDB_env *env_;
    };

    struct Putter {
        explicit Putter(MDB_env *env) : env_(env), txn_(nullptr), txnCounter_(env), rc_{} {
            rc_ = mdb_txn_begin(env_, nullptr, 0, &txn_);
        }
        ~Putter() {
            mdb_txn_commit(txn_);
        }

        int put(void *key, size_t keylen, void *val, size_t vallen, bool override) {
            if (expr_unlikely(rc_ != MDB_SUCCESS))
                return rc_;
            assert(key && keylen > 0 && val && vallen > 0);
            MDB_dbi dbi;
            int rc;
            rc = mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (rc != MDB_SUCCESS) return rc;
            MDB_val mdbkey, mdbval;
            mdbkey.mv_size = keylen;
            mdbkey.mv_data = key;
            mdbval.mv_size = vallen;
            mdbval.mv_data = val;
            unsigned int flag = 0;
            if (expr_unlikely(!override)) flag |= MDB_NOOVERWRITE;
            return mdb_put(txn_, dbi, &mdbkey, &mdbval, flag);
        }

        int remove(void* key, size_t keylen, void* val = nullptr, size_t vallen = 0) {
            if (expr_unlikely(rc_ != MDB_SUCCESS))
                return rc_;
            assert(key && keylen > 0);
            MDB_dbi dbi;
            int rc;
            rc = mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (rc != MDB_SUCCESS) return rc;
            MDB_val mdbkey, mdbval;
            mdbkey.mv_size = keylen;
            mdbkey.mv_data = key;
            MDB_val *pMdbKey = &mdbkey, *pMdbVal = nullptr;
            if (val != nullptr && vallen > 0) {
                mdbval.mv_size = vallen;
                mdbval.mv_data = val;
                pMdbVal = &mdbval;
            }
            return mdb_del(txn_, dbi, pMdbKey, pMdbVal);
        }

        MDB_env *env_;
        MDB_txn *txn_;
        TxnWrCounter txnCounter_;
        int rc_;
    };

    struct Getter {
        explicit Getter(MDB_env *env) : env_(env), txn_(nullptr), txnCounter_(env), rc_{} {
            rc_ = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn_);
        }
        ~Getter() {
            // since we're getter, there nothing to commit to db
            mdb_txn_abort(txn_);
        }

        int get(void *key, size_t keylen, std::string &val) {
            if (expr_unlikely(rc_ != MDB_SUCCESS))
                return rc_;
            assert(key && keylen > 0);
            MDB_dbi dbi;
            int rc;
            rc = mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (rc != MDB_SUCCESS) return rc;
            MDB_val mdbkey, mdbval;
            mdbkey.mv_size = keylen;
            mdbkey.mv_data = key;
            rc = mdb_get(txn_, dbi, &mdbkey, &mdbval);
            if (rc != MDB_SUCCESS) return rc;
            assert(mdbval.mv_data && mdbval.mv_size > 0);
            val.resize(mdbval.mv_size);
            memcpy(&val[0], mdbval.mv_data, mdbval.mv_size);
            return rc;
        }

        int get_view(void *key, size_t keylen, MDB_val &mdbval) {
            assert(key && keylen > 0);
            MDB_dbi dbi;
            int rc;
            rc = mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (rc != MDB_SUCCESS) return rc;
            MDB_val mdbkey;
            mdbkey.mv_size = keylen;
            mdbkey.mv_data = key;
            rc = mdb_get(txn_, dbi, &mdbkey, &mdbval);
            if (rc != MDB_SUCCESS) return rc;
            assert(mdbval.mv_data && mdbval.mv_size > 0);
            return rc;
        }

        MDB_env *env_;
        MDB_txn *txn_;
        TxnRdCounter txnCounter_;
        int rc_;
    };

    struct Cursor {
        using VisitorFn = std::function<bool(void * /*key*/, size_t /*keylen*/, void * /*value*/, size_t /*valLen*/)>;
        using ErrorFn = std::function<void(void * /*key*/, size_t /*keylen*/, int /*rc*/)>;

        explicit Cursor(MDB_env *env) : env_(env), txn_(nullptr), cursor_(nullptr), txnCounter_(env), rc_{} {
            rc_ = mdb_txn_begin(env_, nullptr, MDB_RDONLY, &txn_);
        }
        ~Cursor() {
            mdb_cursor_close(cursor_);
            mdb_txn_abort(txn_);
        }

        int iterate_from(VisitorFn fn, const std::string &startkey) {
            if (expr_unlikely(rc_ != MDB_SUCCESS))
                return rc_;
            MDB_dbi dbi;
            mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (cursor_ == nullptr) {
                mdb_cursor_open(txn_, dbi, &cursor_);
            } else {
                mdb_cursor_renew(txn_, cursor_);
            }
            MDB_val key, val;
            key.mv_data = lmdb_void_ptr_cast(startkey.data());
            key.mv_size = startkey.size();
            int rc;
            auto op = startkey.empty() ? MDB_FIRST : MDB_SET_RANGE;// >= startkey
            do {
                rc = mdb_cursor_get(cursor_, &key, &val, op);
                if (rc != MDB_SUCCESS) break;
                if (!fn(key.mv_data, key.mv_size, val.mv_data, val.mv_size)) {
                    LMDBLOG_O("[lmdb] callback abort");
                    break;
                }
                if (expr_unlikely(op != MDB_NEXT))
                    op = MDB_NEXT;
            } while (rc == MDB_SUCCESS);
            return rc;
        }

        bool key_exist(const char *key, size_t keylen) {
            assert(key && keylen);
            MDB_dbi dbi;
            mdb_dbi_open(txn_, nullptr, 0, &dbi);
            if (cursor_ == nullptr) {
                mdb_cursor_open(txn_, dbi, &cursor_);
            } else {
                mdb_cursor_renew(txn_, cursor_);
            }
            int rc;
            MDB_val mdbkey, mdbval;
            mdbkey.mv_data = lmdb_void_ptr_cast(key);
            mdbkey.mv_size = keylen;
            rc = mdb_cursor_get(cursor_, &mdbkey, &mdbval, MDB_SET);
            return (rc == MDB_SUCCESS);
        }

        MDB_env *env_;
        MDB_txn *txn_;
        MDB_cursor *cursor_;
        TxnRdCounter txnCounter_;
        int rc_;
    };

    struct TuningOpts {
        size_t maxSize = kDefaultMaxSize;// lmdb default max size is 10MB, we set default to 2GB
        bool readOnly = false;
        bool txnSync = true;          // lmdb default always sync to filesystem after txn commit, set this to false skip it (application code must explitcit call 'mdb_env_sync')
        unsigned int maxReaders = 126;// lmdb default is 126 readers
        unsigned int putRetry = 1;    // retry put if lmdb reached config max size
        unsigned int getRetry = 1;    // retry get if lmdb need to sync max size
    };

    class LMDBWrapper {
    public:
        explicit LMDBWrapper(const char *path, const char *filename, TuningOpts opts) : env_(), opts_(opts), usrCtx_{}, isOpened(false) {
            init(path, filename, opts);
        }
        ~LMDBWrapper() {
            if (!opts_.readOnly && !opts_.txnSync) {
                LMDBLOG_O("[lmdb] sync env");
                mdb_env_sync(env_, true);// force env sync
            }
        }

        MDB_env *handle() const {
            return env_;
        }

        const WrapperCtx &usr_stat() const {
            return usrCtx_;
        }

        bool is_opened() const {
            return isOpened;
        }

        static void print_err(const char *method, int rc) {
            LMDBLOG_W("%s: (%d) %s", method, rc, mdb_strerror(rc));
        }

    public:
        int put(void *key, size_t keylen, void *val, size_t vallen, bool override = true) {
            SCOPED_READ_LOCK(usrCtx_.rwLock_);
            Putter putter(env_);
            auto rc = putter.put(key, keylen, val, vallen, override);
            if (rc != MDB_SUCCESS) {
                std::string keystr((const char *) key, keylen);
                LMDBLOG_W("[put] failed, key: '%s', error: (%d) %s", keystr.c_str(), rc, mdb_strerror(rc));
            }
            return rc;
        }

        int remove(void* key, size_t keylen, void* val = nullptr, size_t vallen = 0) {
            SCOPED_READ_LOCK(usrCtx_.rwLock_);
            Putter putter(env_);
            auto rc = putter.remove(key, keylen, val, vallen);
            if (rc != MDB_SUCCESS) {
                std::string keystr((const char *) key, keylen);
                LMDBLOG_W("[remove] failed, key: '%s', error: (%d) %s", keystr.c_str(), rc, mdb_strerror(rc));
            }
            return rc;
        }

        int put(const std::string &key, const std::string &val, bool override = true) {
            return put(lmdb_void_ptr_cast(key.data()), key.size(), lmdb_void_ptr_cast(val.data()), val.size(), override);
        }

        int put_safe(const std::string &key, const std::string &val, bool override = true) {
            int rc;
            int retry = 0;
            int maxRetry = std::max(static_cast<int>(opts_.putRetry), 1);
            auto cursize = opts_.maxSize;
            do {
                rc = put(key, val, override);
                if (expr_unlikely(shouldAllocPage() || (rc == MDB_MAP_FULL))) {
                    // printf("--- debug: wrtxn: %d, rdtxn: %d\n", usrCtx_.wrTxnCnt_.load(), usrCtx_.rdTxnCnt_.load());
                    increaseDbMaxSize(cursize);
                }
            } while (rc == MDB_MAP_FULL && retry++ <= maxRetry);
            return rc;
        }

        int get(void *key, size_t keylen, std::string &val) {
            SCOPED_READ_LOCK(usrCtx_.rwLock_);
            Getter getter(env_);
            auto rc = getter.get(key, keylen, val);
            if (rc != MDB_SUCCESS) {
                std::string keystr((const char *) key, keylen);
                LMDBLOG_W("[get] failed, key: '%s', error: (%d) %s", keystr.c_str(), rc, mdb_strerror(rc));
            }
            return rc;
        }

        int get(const std::string &key, std::string &val) {
            return get(lmdb_void_ptr_cast(key.data()), key.size(), val);
        }

        int get_safe(const std::string &key, std::string &val) {
            int rc;
            int retry = 0;
            int maxRetry = std::max(static_cast<int>(opts_.getRetry), 1);
            auto cursize = opts_.maxSize;
            do {
                rc = get(key, val);
                if (expr_unlikely(rc == MDB_MAP_RESIZED)) {
                    syncDbMaxSize(cursize);
                }
            } while (rc == MDB_MAP_RESIZED && retry++ <= maxRetry);
            return rc;
        }

        int iterate(Cursor::VisitorFn fn) {
            Cursor cursor(env_);
            return cursor.iterate_from(fn, {});
        }

        int iterate_from(Cursor::VisitorFn fn, const std::string &startkey) {
            Cursor cursor(env_);
            return cursor.iterate_from(fn, startkey);
        }

        void sync() {
            if (opts_.txnSync) return;
            mdb_env_sync(env_, true);
        }

        size_t count() const {
            MDB_stat mst;
            mdb_env_stat(env_, &mst);
            return mst.ms_entries;
        }

    private:
        bool shouldAllocPage() const {
            MDB_envinfo mei;
            MDB_stat mst;
            mdb_env_info(env_, &mei);
            mdb_env_stat(env_, &mst);
            const auto maxPages = mei.me_mapsize / mst.ms_psize;
            const auto usedPages = mei.me_last_pgno + 1;
            return (usedPages + 2 >= maxPages);
        }

        void increaseDbMaxSize(size_t oldsize) {
            SCOPED_WRITE_LOCK(usrCtx_.rwLock_);
            if (opts_.maxSize > oldsize) return;
            while (usrCtx_.total_txn_count() != 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            // mdb_env_set_mapsize require that no transaction is active
            auto newSize = opts_.maxSize * 2;
            opts_.maxSize = newSize;
            mdb_env_set_mapsize(env_, newSize);
            LMDBLOG_O("[lmdb] increase db maxsize: %zu --> %zu", oldsize, newSize);
        }

        void syncDbMaxSize(size_t oldsize) {
            SCOPED_WRITE_LOCK(usrCtx_.rwLock_);
            MDB_envinfo envstat;
            // mdb_env_info(env_, &envstat);
            // auto oldsize = envstat.me_mapsize;
            if (oldsize < opts_.maxSize) return;
            // sync new size
            mdb_env_set_mapsize(env_, 0);
            mdb_env_info(env_, &envstat);
            auto newsize = envstat.me_mapsize;
            // update new size
            opts_.maxSize = newsize;
            LMDBLOG_O("[lmdb] sync db maxsize: %zu --> %zu", oldsize, newsize);
        }

        void init(const char *path, const char *name, TuningOpts opts) {
            unsigned int flag = 0;
            std::string pathstr(path);
            libs::makeDir(path);
            if (name) {
                flag |= MDB_NOSUBDIR;
                pathstr.append("/");
                pathstr.append(name);
            };
            if (opts.readOnly)
                flag |= MDB_RDONLY;
            if (!opts_.txnSync)
                flag |= MDB_NOSYNC;// this speed up write txn, but may lost some txn if not flush correctly
            flag |= MDB_NOTLS; // tie reader lock table to txn object, instead of FTNBServer long running worker threads
            int rc;
            mdb_env_create(&env_);
            mdb_env_set_mapsize(env_, opts.maxSize);
            mdb_env_set_maxreaders(env_, opts.maxReaders);
            mdb_env_set_userctx(env_, lmdb_void_ptr_cast(&usrCtx_));
            // create lock + db env. db name is 'data.mdb' if name not set
            rc = mdb_env_open(env_, pathstr.c_str(), flag, LMDB_DB_PERM);
            if (rc != MDB_SUCCESS) {
                // throw exception or assert so client code not need to check is_opened()
                LMDBLOG_O("[lmdb] open '%s' failed: %d, %s", pathstr.c_str(), rc, mdb_strerror(rc));
                return;
            }
            isOpened = true;
            // re-update maxsize
            MDB_envinfo mei;
            mdb_env_info(env_, &mei);
            opts_.maxSize = mei.me_mapsize;
            mdb_env_get_maxreaders(env_, &opts_.maxReaders);
            LMDBLOG_O("[lmdb] open db: %s, maxsize: %zu", pathstr.c_str(), mei.me_mapsize);
        }

    private:
        MDB_env *env_;
        TuningOpts opts_;
        WrapperCtx usrCtx_;
        bool isOpened;

    private:
        friend struct Putter;
        friend struct Getter;
        friend struct Cursor;
    };
};
}// namespace libs::lmdb_wrapper
