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

class ReadChunk {
public:
    ReadChunk(size_t want) : group_(want), want_(want), have_(want) {}
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
    Read& operator[](size_t i) { return group_[i]; }
    std::vector<Read>::iterator begin() { return group_.begin(); }
    std::vector<Read>::iterator end() { return group_.begin() + have_; }


private:
    //std::vector<std::string> group_;
    std::vector<Read> group_;

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

    Read& operator[](size_t i) { return (*chunk_)[i]; };
    typename std::vector<Read>::iterator begin() { return chunk_->begin(); }
    typename std::vector<Read>::iterator end()
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


class ReadParser
        {
        public:
    // ReadParser(std::vector<std::string> files, uint32_t numConsumers, uint32_t numParsers = 1, uint32_t chunkSize = 1000)

    ReadParser(std::vector<std::vector<std::string>> files, uint32_t numConsumers, uint32_t numParsers = 1, uint32_t chunkSize = 1000)
    : inputStreams_(files),blockSize_(chunkSize)
    {

        numParsers_ = numParsers;
        numParsing_ = 0;

        readQueue_ = moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(
                4 * numConsumers, numParsers, 0);

        seqContainerQueue_ =
                moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(
                        4 * numConsumers, 1 + numConsumers, 0);

        workQueue_ = moodycamel::ConcurrentQueue<uint32_t>(numParsers_);

        // push all file ids on the queue
        for (size_t i = 0; i < files.size(); ++i) {
            workQueue_.enqueue(i);
        }

        // every parsing thread gets a consumer token for the seqContainerQueue
        // and a producer token for the readQueue.
        for (size_t i = 0; i < numParsers_; ++i) {
            consumeContainers_.emplace_back(
                    new moodycamel::ConsumerToken(seqContainerQueue_));
            produceReads_.emplace_back(new moodycamel::ProducerToken(readQueue_));
        }

        // enqueue the appropriate number of read chunks so that we can start
        // filling them once the parser has been started.
        moodycamel::ProducerToken produceContainer(seqContainerQueue_);
        for (size_t i = 0; i < 4 * numConsumers; ++i) {
            auto chunk = std::make_unique<ReadChunk>(blockSize_);
            seqContainerQueue_.enqueue(produceContainer, std::move(chunk));
        }
    };


    ~ReadParser()
    {
        if (isActive_ or numParsing_ > 0) {
            // Think about if this is too noisy --- but the user really shouldn't do this.
            std::cerr << "\n\nEncountered ReadParser destructor while parser was still marked active (or while parsing threads were still active). "
                      << "Be sure to call stop() before letting ReadParser leave scope!\n";
            try {
                stop();
            } catch (const std::exception& e) {
                // Should exiting here be a user-definable behavior?
                // What is the right mechanism for that.
                std::cerr << "\n\nParser encountered exception : " << e.what() << "\n";
                std::exit(-1);
            }
        }
        // Otherwise, we are good to go (i.e., destruct)
    };
    bool start() {
        if (numParsing_ == 0) {
            isActive_ = true;
            // Some basic checking to ensure the read files look "sane".
//            if (inputStreams_.size() != inputStreams2_.size()) {
//                throw std::invalid_argument("There should be the same number "
//                                            "of files for the left and right reads");
//            }


            for (size_t i = 0; i < inputStreams_.size(); ++i) {
                auto& s = inputStreams_[i];
 //               auto& s2 = inputStreams2_[i];
                /*if (s1 == s2) {
                    throw std::invalid_argument("You provided the same file " + s1 +
                                                " as both a left and right file");
                }*/
            }
            threadResults_.resize(numParsers_);
            std::fill(threadResults_.begin(), threadResults_.end(), 0);

            for (size_t i = 0; i < numParsers_; ++i) {
                ++numParsing_;
                parsingThreads_.emplace_back(new std::thread([this, i]() {
                    this->threadResults_[i] = parse_read_tuples(this->inputStreams_,
                                                               this->numParsing_, this->consumeContainers_[i].get(),
                                                               this->produceReads_[i].get(), this->workQueue_,
                                                               this->seqContainerQueue_, this->readQueue_);

                }));
                std::cout<<"or we here 153"<<std::endl;
            }
            std::cout<<"or we here 155"<<std::endl;

            return true;
        } else {
            std::cout<<"or we here 158"<<std::endl;

            return false;
        }
    }
    bool stop()
    {
        bool ret{false};
        if (isActive_) {
            for (auto& t : parsingThreads_) {
                t->join();
            }
            isActive_ = false;
            for (auto& res : threadResults_) {
                if (res == -3) {
                    throw std::range_error("Error reading from the FASTA/Q stream. Make sure the file is valid.");
                } else if (res < -1) {
                    std::stringstream ss;
                    ss << "Error reading from the FASTA/Q stream. Minimum return code for left and right read was ("
                       << res << "). Make sure the file is valid.";
                    throw std::range_error(ss.str());
                }
            }
            ret = true;
        } else {
            // Is this being too loud?  Again, if this triggers, the user has violated the API.
            std::cerr << "stop() was called on a FastxParser that was not marked active. Did you remember "
                      << "to call start() on this parser?\n";
        }
        std::cout<<"are we here 295"<<std::endl;
        return ret;

    };
    ReadGroup getReadGroup()
    {
        return ReadGroup(getProducerToken_(), getConsumerToken_());
    };

    bool refill(ReadGroup& seqs)
    {
        std::cout<<"are we here 306"<<std::endl;

        finishedWithGroup(seqs);
        auto curMaxDelay = MIN_BACKOFF_ITERS;
        std::cout<<"are we here 310"<<std::endl;

        while (numParsing_ > 0) {
            std::cout<<"are we here 313"<<std::endl;
             //dummy, should be w o !
            if (readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr()))
            {
                std::cout<<"are we here 315"<<std::endl;

                return true;
            }
            backoffOrYield(curMaxDelay);
        }
        std::cout<<"are we here 322"<<std::endl;

        return readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr());
    };
    void finishedWithGroup(ReadGroup& s)
    {
        {
            // If this read group is holding a valid chunk, then give it back
            if (!s.empty()) {
                seqContainerQueue_.enqueue(s.producerToken(), std::move(s.takeChunkPtr()));
                s.setChunkEmpty();
            }
        }
    };





private:
    moodycamel::ProducerToken getProducerToken_()
    {
        return moodycamel::ProducerToken(seqContainerQueue_);
    };
    moodycamel::ConsumerToken getConsumerToken_()
    {
        return moodycamel::ConsumerToken(readQueue_);
    };

    std::vector<std::vector<std::string>> inputStreams_;
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


    int parse_read_tuples(std::vector<std::vector<std::string>> inputStreams, std::atomic<uint32_t>& numParsing,
                          moodycamel::ConsumerToken* cCont, moodycamel::ProducerToken* pRead,
                          moodycamel::ConcurrentQueue<uint32_t>& workQueue,
                          moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>&
                          seqContainerQueue_,
                          moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>& readQueue_)
    {

        auto curMaxDelay = MIN_BACKOFF_ITERS;
        std::string* s;
        uint32_t fn{0};

        while (workQueue.try_dequeue(fn))
        {
           size_t numfiles = inputStreams.size();
           const int nf = numfiles;
           //constexpr int nuf = nf;
           // std::tuple::tuple<string>()
           std::tuple<std::vector<std::string>> files;
          // std::tuple<std::make_index_sequence<nf>> tpl;
            for(int i = 0; i <numfiles; i++)
            {

            }
            using index_sequence_for = std::make_index_sequence<sizeof(numfiles)>;

            auto& file = inputStreams[numfiles][fn];

            Read rd;

        }
        std::cout<<"are we here?398"<<std::endl;
        return 0;

        //while (rd and (kseq_read(seq2) > 0)) {

        }
    };


#endif //FQ_REIMPLEMENTED_READPARSER_H
