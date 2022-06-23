//
// Created by Ivan Radkevich on 6/13/22.
// Rewrote by Bo Li on 6/18/22.
// This is a simplified reimplementation of Rob Patro's FQFeeder: https://github.com/rob-p/FQFeeder [BSD-3 license]
//

#ifndef READPARSER_HPP
#define READPARSER_HPP

#include <atomic>
#include <vector>
#include <cstdio>
#include <cassert>
#include <memory>
#include <iosfwd>
#include <sstream>

#include "gzip_utils.hpp"
#include "external/concurrentqueue.h"
#include "external/thread_utils.hpp"


struct ReadTuple {


    ReadTuple(int n_mates) : mates(n_mates) {}
    Read& operator[](int i) { return mates[i]; }
    std::vector<Read> getMates(){return mates;}
private:
    std::vector<Read> mates;
};

struct ReadChunk {


    ReadChunk(size_t size, int n_mates) : reads(size, ReadTuple(n_mates)) {}
    inline void have(size_t num) { nreads= num; }

    ReadTuple& operator[](size_t i) { return reads[i]; }
    std::vector<ReadTuple>::iterator begin() { return reads.begin(); }
    std::vector<ReadTuple>::iterator end() { return reads.begin() + nreads; }
    size_t getNreads(){ return nreads;}
    void pushToChunk(ReadTuple rd){ reads.push_back(rd);nreads++;}
private:
    size_t nreads; // how many reads we currently have?
    std::vector<ReadTuple> reads;
};

struct ReadGroup {

    moodycamel::ConsumerToken& consumerToken() { return ctRead; }
    moodycamel::ProducerToken& producerToken() { return ptSpace; }
    std::unique_ptr<ReadChunk>&& takeChunkPtr() { return std::move(chunk_); }
    void setChunkEmpty() { chunk_.release(); }


    //  a reference to the chunk this ReadGroup owns
    std::unique_ptr<ReadChunk>& chunkPtr() { return chunk_; }
    ReadGroup(moodycamel::ProducerToken&& pt, moodycamel::ConsumerToken&& ct) : ptSpace(std::move(pt)), ctRead(std::move(ct)) {}

    bool empty() const { return chunk_.get() == nullptr; }

    ReadTuple& operator[](size_t i) { return (*chunk_)[i]; };
    std::vector<ReadTuple>::iterator begin() { return chunk_->begin(); }
    std::vector<ReadTuple>::iterator end() { return chunk_->end(); }
private:
    std::unique_ptr<ReadChunk> chunk_{nullptr};
    moodycamel::ProducerToken ptSpace;
    moodycamel::ConsumerToken ctRead;
};

class ReadParser {
public:
    ReadParser(std::vector<std::vector<std::string>>& input_files, int numConsumers, int numParsers = 1, size_t chunkSize = 100000) : blockSize_(chunkSize), filestreams_(input_files) {
        int n_files = input_files.size();
        n_mates_ = input_files[0].size();

        if (numParsers > n_files) {
            printf("Detected more parsers than number of files; setting numParsers to %d.\n", n_files);
            numParsers = n_files;
        }
        
        // Initialize queues
        workQueue_ = moodycamel::ConcurrentQueue<int>(numParsers); // Only need one producer
        moodycamel::ProducerToken ptFile(workQueue_);
        for (int i = 0; i < n_files; ++i) assert(workQueue_.enqueue(ptFile, i));

        seqContainerQueue_ = moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(4 * numConsumers, 1 + numConsumers, 0); // blocks of empty space (ReadChunk) to fill in, the extra one is for this thread
        readQueue_ = moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>>(4 * numConsumers, numParsers, 0); // blocks of filled in reads

        // Each parser should have a consumer token for seqContainerQueue_ to get empty spaces and one producer token for the readQueue_ to load reads
        for (int i = 0; i < numParsers; ++i) {
            consumeContainers_.emplace_back(new moodycamel::ConsumerToken(seqContainerQueue_));
            produceReads_.emplace_back(new moodycamel::ProducerToken(readQueue_));
        }

        // create empty chunks and push to seqContainerQueue_
        moodycamel::ProducerToken ptoken(seqContainerQueue_);
        for (int i = 0; i < 4 * numConsumers; ++i) {
          auto chunk = std::unique_ptr<ReadChunk>(new ReadChunk(blockSize_, n_mates_));
          assert(seqContainerQueue_.enqueue(ptoken, std::move(chunk)));
        }


        numParsing_ = 0;
        for (int i = 0; i < numParsers; ++i) {
          ++numParsing_;
          parsingThreads_.emplace_back(new std::thread([this, i]() {
                this->parse_read_tuples(this->consumeContainers_[i].get(), this->produceReads_[i].get());
            }));
        }
    }

    ~ReadParser() {
        for (auto& thread: parsingThreads_) thread->join();
    }


    ReadGroup getReadGroup() {
        return ReadGroup(moodycamel::ProducerToken(seqContainerQueue_), moodycamel::ConsumerToken(readQueue_));
    }


    void finishedWithGroup(ReadGroup &s) {
        {
            // If this read group is holding a valid chunk, then give it back
            if (!s.empty()) {
                seqContainerQueue_.enqueue(s.producerToken(), std::move(s.takeChunkPtr()));
                s.setChunkEmpty();
            }
        }
    };
    bool refill(ReadGroup& seqs)  {

        finishedWithGroup(seqs);
        auto curMaxDelay = MIN_BACKOFF_ITERS;
        while (numParsing_ > 0) {
            if (readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr())) {
                return true;
            }
            backoffOrYield(curMaxDelay);
        }
        return readQueue_.try_dequeue(seqs.consumerToken(), seqs.chunkPtr());
    }
private:
    size_t blockSize_;
    int n_mates_;
    std::vector<std::vector<std::string>> filestreams_;
    std::atomic<int> numParsing_;
    std::vector<int> threadResults_;

    moodycamel::ConcurrentQueue<int> workQueue_;
    moodycamel::ConcurrentQueue<std::unique_ptr<ReadChunk>> seqContainerQueue_, readQueue_;

    std::vector<std::unique_ptr<moodycamel::ProducerToken>> produceReads_;
    std::vector<std::unique_ptr<moodycamel::ConsumerToken>> consumeContainers_;

    std::vector<std::unique_ptr<std::thread>> parsingThreads_;







    int parse_read_tuples(
            moodycamel::ConsumerToken *cCont, moodycamel::ProducerToken *pRead) {


        size_t curMaxDelay = MIN_BACKOFF_ITERS;
        std::unique_ptr<ReadChunk> local;
        std::vector<iGZipFile> input_streams;
        uint32_t cur_waiting;

        cur_waiting = 0;
        while (!seqContainerQueue_.try_dequeue(*cCont, local)) backoffOrYield(curMaxDelay);

        ReadTuple *s;

        uint32_t fn{0};
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
                if (cnt != n_mates_)
                {
                    std::cout<<"Detected mate files with different number of lines!"<<std::endl;
                    exit(-1);
                }



                ++cur_waiting;
                if (cur_waiting == blockSize_) {
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

            if (cur_waiting > 0) {
                local->have(cur_waiting);
                curMaxDelay = MIN_BACKOFF_ITERS;
                while (!readQueue_.try_enqueue(*pRead, std::move(local))) backoffOrYield(curMaxDelay);
            }


            --numParsing_;
        }

    };
};

#endif
