//
// Created by Ivan Radkevich on 6/13/22.
//

#ifndef FQ_REIMPLEMENTED_READPARSER_H
#define FQ_REIMPLEMENTED_READPARSER_H

// must remove after
#include <iostream>
// here a line to think on in conjunction with other lines
#include <vector>
#include <mutex>




#include <tuple>
#include <random>




#if defined(__SSE2__)
#if defined(HAVE_SIMDE)
#include "simde/x86/sse2.h"
#else
#include <emmintrin.h>
#endif
#endif



// maybe not needed
#include <sstream>
#include <atomic>

#include "gzip_utils.hpp"
#include "kseq.h"
#include "concurrentqueue.h"



#define ALWAYS_INLINE inline __attribute__((__always_inline__))


static const constexpr size_t MIN_BACKOFF_ITERS = 32;
static const size_t MAX_BACKOFF_ITERS = 1024;

ALWAYS_INLINE static void cpuRelax() {
#if defined(__SSE2__)  // AMD and Intel
#if defined(HAVE_SIMDE)
    simde_mm_pause();
#else
    _mm_pause();
#endif
#elif defined(__i386__) || defined(__x86_64__)
    asm volatile("pause");
#elif defined(__aarch64__)
  asm volatile("wfe");
#elif defined(__armel__) || defined(__ARMEL__)
  asm volatile ("nop" ::: "memory");
#elif defined(__arm__) || defined(__aarch64__)
  __asm__ __volatile__ ("yield" ::: "memory");
#elif defined(__ia64__)  // IA64
  __asm__ __volatile__ ("hint @pause");
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
   __asm__ __volatile__ ("or 27,27,27" ::: "memory");
#else  // everything else.
   asm volatile ("nop" ::: "memory");
#endif
}


ALWAYS_INLINE void yieldSleep() {
    using namespace std::chrono;
    std::chrono::microseconds ytime(500);
    std::this_thread::sleep_for(ytime);
}

ALWAYS_INLINE void backoffExp(size_t& curMaxIters) {
    thread_local std::uniform_int_distribution<size_t> dist;

    // see : https://github.com/coryan/google-cloud-cpp-common/blob/a6e7b6b362d72451d6dc1fec5bc7643693dbea96/google/cloud/internal/random.cc
#if defined(__linux) && defined(__GLIBCXX__) && __GLIBCXX__ >= 20200128
    thread_local std::random_device rd("/dev/urandom");
#else
    thread_local std::random_device rd;
#endif  // defined(__GLIBCXX__) && __GLIBCXX__ >= 20200128

    thread_local std::minstd_rand gen(rd());
    const size_t spinIters =
            dist(gen, decltype(dist)::param_type{0, curMaxIters});
    curMaxIters = std::min(2 * curMaxIters, MAX_BACKOFF_ITERS);
    for (size_t i = 0; i < spinIters; i++) {
        cpuRelax();
    }
}


ALWAYS_INLINE void backoffOrYield(size_t& curMaxDelay) {
    if (curMaxDelay >= MAX_BACKOFF_ITERS) {
        yieldSleep();
        curMaxDelay = MIN_BACKOFF_ITERS;
    }
    backoffExp(curMaxDelay);
}

KSEQ_INIT(gzFile, gzread)




struct ReadTuple {
    std::vector<Read> mates;

    ReadTuple(int n_mates) : mates(n_mates) {}
    Read& operator[](int i) { return mates[i]; }
};

class ReadChunk {
public:
    ReadChunk(size_t want, int n_mates=2) : group_(want, ReadTuple(n_mates)), want_(want), have_(want) {}
    inline void have(size_t num) { have_ = num; }
    inline size_t size() { return have_; }
    inline size_t want() const { return want_; }
    /* T& operator[](size_t i) { return group_[i]; }
     typename std::vector<T>::iterator begin() { return group_.begin(); }
     typename std::vector<T>::iterator end() { return group_.begin() + have_; }
 */

    /* std::string& operator[](size_t i) { return group_[i]; }
     std::vector<std::string>::iterator begin() { return group_.begin(); }
     std::vector<std::string>::iterator end() { return group_.begin() + have_; }
  */
    ReadTuple& operator[](size_t i) { return group_[i]; }
    std::vector<ReadTuple>::iterator begin() { return group_.begin(); }
    std::vector<ReadTuple>::iterator end() { return group_.begin() + have_; }


private:
    //std::vector<std::string> group_;
    std::vector<ReadTuple> group_;
    size_t want_;
    size_t have_;
};


class ReadGroup {
public:
    ReadGroup(moodycamel::ProducerToken&& pt, moodycamel::ConsumerToken&& ct)
            : pt_(std::move(pt)), ct_(std::move(ct)) {}
    moodycamel::ConsumerToken& consumerToken() { return ct_; }
    moodycamel::ProducerToken& producerToken() { return pt_; }
    //  a reference to the chunk this ReadGroup owns
    std::unique_ptr<ReadChunk>& chunkPtr() { return chunk_; }
    // get a *moveable* reference to the chunk this ReadGroup owns
    std::unique_ptr<ReadChunk>&& takeChunkPtr() { return std::move(chunk_); }
    inline void have(size_t num) { chunk_->have(num); }
    inline size_t size() { return chunk_->size(); }
    inline size_t want() const { return chunk_->want(); }
    /*std::string& operator[](size_t i) { return (*chunk_)[i]; }
    typename std::vector<std::string>::iterator begin() { return chunk_->begin(); }
    typename std::vector<std::string>::iterator end() {
        return chunk_->begin() + chunk_->size();
    }*/

    ReadTuple& operator[](size_t i) { return (*chunk_)[i]; };
    typename std::vector<ReadTuple>::iterator begin() { return chunk_->begin(); }
    typename std::vector<ReadTuple>::iterator end()
    {

            return chunk_->begin() + chunk_->size();
        }

    void setChunkEmpty() { chunk_.release(); }
    bool empty() const { return chunk_.get() == nullptr; }

private:
    std::unique_ptr<ReadChunk> chunk_{nullptr};
    moodycamel::ProducerToken pt_;
    moodycamel::ConsumerToken ct_;
};


class ReadParser {
public:
    // ReadParser(std::vector<std::string> files, uint32_t numConsumers, uint32_t numParsers = 1, uint32_t chunkSize = 1000)

    ReadParser(std::vector<std::vector<std::string>> files, uint32_t numConsumers, uint32_t numParsers = 1,
               uint32_t chunkSize = 1000)
            : filestreams_(files), blockSize_(chunkSize) {

        //  int n_files = files.size();
      //  std::cout << "ici 190" << std::endl;
        numParsers_ = numParsers;
        if (numParsers_ > files.size()) {
            std::cout << "need more parsers" << std::endl;
            numParsers_ = files.size();

        }


        readQueue_ = moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(
                4 * numConsumers, numParsers, 0);

        seqContainerQueue_ =
                moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(
                        4 * numConsumers, 1 + numConsumers, 0);

        workQueue_ = moodycamel::ConcurrentQueue<uint32_t>(numParsers_);

        // push all file ids on the queue
        for (size_t i = 0; i < files.size(); ++i) {
            assert(workQueue_.enqueue(i));
        }

        // every parsing thread gets a consumer token for the seqContainerQueue
        // and a producer token for the readQueue.

        // Each parser should have a consumer token for spaceQueue_ to get empty spaces and one producer token for the readQueue_ to load reads
        for (size_t i = 0; i < numParsers_; ++i) {
            consumeContainers_.emplace_back(new moodycamel::ConsumerToken(seqContainerQueue_));
            produceReads_.emplace_back(new moodycamel::ProducerToken(readQueue_));
        }

        // enqueue the appropriate number of read chunks so that we can start
        // filling them once the parser has been started.

        // create empty chunks and push to spaceQueue_
        moodycamel::ProducerToken produceContainer(seqContainerQueue_);
        for (size_t i = 0; i < 4 * numConsumers; ++i) {
            auto chunk = std::make_unique<ReadChunk>(blockSize_);
            seqContainerQueue_.enqueue(produceContainer, std::move(chunk));
        }

       // IF ANYTHING RESTORE
        /*numParsing_ = 0;
        for (int i = 0; i < numParsers; ++i) {
            ++numParsing_;
            parsingThreads_.emplace_back(new std::thread([this, i]() {
                this->parse_read_tuples(this->consumeContainers_[i].get(), this->produceReads_[i].get());
            }));
        }*/
    };


    ~ReadParser() {
        //std::cout<<"aargh 248"<<std::endl;
        for (auto& thread: parsingThreads_) thread->join();
       // std::cout<<"aargh 266"<<std::endl;
        // Otherwise, we are good to go (i.e., destruct)
    };
    bool start() {
        numParsing_ = 0;

        if (numParsing_ == 0) {
            isActive_ = true;



            for (size_t i = 0; i < filestreams_.size(); ++i) {
                auto &s = filestreams_[i];

            }
            threadResults_.resize(numParsers_);
            std::fill(threadResults_.begin(), threadResults_.end(), 0);


            //std::cout << numParsers_ << std::endl;
            for (size_t i = 0; i < numParsers_; ++i) {
                ++numParsing_;
                parsingThreads_.emplace_back(new std::thread([this, i]() {
                    this->threadResults_[i] = parse_read_tuples(this->consumeContainers_[i].get(),
                                                                this->produceReads_[i].get());

                }));
            }

            return true;
        } else {

            return false;
        }
    }


    bool stop() {
       // std::cout<<"aargh 248"<<std::endl;
        for (auto& thread: parsingThreads_) thread->detach();
        numParsing_--;
       // std::cout<<"aargh 266"<<std::endl;
        bool ret = true;
        return ret;

    };

    ReadGroup getReadGroup() {
        return ReadGroup(getProducerToken_(), getConsumerToken_());
    };

    bool refill(ReadGroup &seqs) {




        if (!seqs.empty()) {
            assert(seqContainerQueue_.enqueue(seqs.producerToken(), std::move(seqs.chunkPtr())));
            assert(seqs.empty());
        }
        finishedWithGroup(seqs);
        auto curMaxDelay = MIN_BACKOFF_ITERS;


        while (numParsing_ > 0) {

            if (readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr())) {


                return true;
            }

            backoffOrYield(curMaxDelay);
        }

        return readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr());
    };

    void finishedWithGroup(ReadGroup &s) {
        {
            // If this read group is holding a valid chunk, then give it back
            if (!s.empty()) {
                seqContainerQueue_.enqueue(s.producerToken(), std::move(s.takeChunkPtr()));
                s.setChunkEmpty();
            }
        }
    };


private:
    moodycamel::ProducerToken getProducerToken_() {
        return moodycamel::ProducerToken(seqContainerQueue_);
    };

    moodycamel::ConsumerToken getConsumerToken_() {
        return moodycamel::ConsumerToken(readQueue_);
    };

    std::vector<std::vector<std::string>> filestreams_;
    uint32_t numParsers_;
    std::atomic<uint32_t> numParsing_;

    // NOTE: Would like to use std::future<int> here instead, but that
    // solution doesn't seem to work.  It's unclear exactly why
    // see (https://twitter.com/nomad421/status/917748383321817088)
    std::vector<std::unique_ptr<std::thread>> parsingThreads_;

    // holds the results of the parsing threads, which is simply equal to
    // the return value of kseq_read() for the last call to that function.
    // A value < -1 signifies some sort of error.
    std::vector<int> threadResults_;

    size_t blockSize_;
    moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>> readQueue_,
            seqContainerQueue_;

    // holds the indices of files (file-pairs) to be processed
    moodycamel::ConcurrentQueue<uint32_t> workQueue_;

    std::vector<std::unique_ptr<moodycamel::ProducerToken>> produceReads_;
    std::vector<std::unique_ptr<moodycamel::ConsumerToken>> consumeContainers_;
    bool isActive_{false};


    int parse_read_tuples(
                          moodycamel::ConsumerToken *cCont, moodycamel::ProducerToken *pRead) {

        //using namespace klibpp;

        size_t curMaxDelay = MIN_BACKOFF_ITERS;
        std::unique_ptr<ReadChunk> local;
        std::vector<iGZipFile> input_streams;
        uint32_t cur_waiting;

        cur_waiting = 0;
        while (!seqContainerQueue_.try_dequeue(*cCont, local)) backoffOrYield(curMaxDelay);

        ReadTuple *s;

        uint32_t fn{0};
        //std::cout<<inputfiles[0][fn]<<"  inputfiles[0][fn] "<<std::endl;
       // std::cout << filestreams_[0][fn] << "  filestreams_[0][fn]   " << std::endl;
        s = &((*local)[cur_waiting]);

        while (workQueue_.try_dequeue(fn)) {
            input_streams.clear();
            for (size_t i = 0; i < filestreams_.size(); ++i) {
                input_streams.emplace_back(filestreams_[fn][i]);
            }
            int cnt = 0;
            for (int i = 0; i < filestreams_.size(); ++i)
            {
                cnt += input_streams[i].next((*s)[i]);
            }

            while (cnt > 0) {
                ++cur_waiting;
                if (cur_waiting == blockSize_) {
                    //local->size() = fn; //change have_
                    local->have(cur_waiting);
                    curMaxDelay = MIN_BACKOFF_ITERS;
                    while (!readQueue_.try_enqueue(*pRead, std::move(local))) backoffOrYield(curMaxDelay);
                    cur_waiting = 0;
                    curMaxDelay = MIN_BACKOFF_ITERS;
                    while (!seqContainerQueue_.try_dequeue(*cCont, local)) backoffOrYield(curMaxDelay);
                }
                s = &((*local)[cur_waiting]);
                cnt = 0;
                for (int i = 0; i < filestreams_.size(); ++i)
                {
                    cnt += input_streams[i].next((*s)[i]);
                }
            }

            if (fn > 0) {
                local->have(cur_waiting);
                curMaxDelay = MIN_BACKOFF_ITERS;
                while (!readQueue_.try_enqueue(*pRead, std::move(local))) backoffOrYield(curMaxDelay);
            }


            --numParsing_;
            return 0;
        }

    };
};

#endif FQ_REIMPLEMENTED_READPARSER_H
