#include "ClumpsThread.h"
#include "Clump.h"
#include <future>
#include <thread>
#include <functional>

using namespace std;

namespace segment {
    ClumpsThread::ClumpsThread(int maxThreads,
                               vector<Clump> *clumps,
                               void (*threadFunction)(*Clump, int),
                               void (*threadDoneFunction)(*Clump, int)) {
        this->maxThreads = maxThreads;
        this->clumps = clumps;
        this->threadFunction = threadFunction;
        this->threadDoneFunction = threadDoneFunction;
    }

    void ClumpsThread::start() {
        vector<shared_future<void>> allThreads;
        vector<int> threadClumpId;

        vector<Clump>::iterator clumpIterator = this->clumps->begin();
        do {
            // Fill thread queue
            while (allThreads.size() < this->maxThreads && clumpIterator < this->clumps->end()) {
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - this->clumps->begin();
                clumpIterator++;
                shared_future<void> thread_object = async(this->threadFunction, clump, clumpIdx);
                allThreads.push_back(thread_object);
                threadClumpId.push_back(clumpIdx);
            }

            // Wait for a thread to finish
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<void> thread = allThreads[i];
                int clumpIdx = threadClumpId[i];
                Clump *clump = &(*this->clumps)[clumpIdx];
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    thread.get();
                    this->threadDoneFunction(clump, clumpIdx);
                    allThreads.erase(allThreads.begin() + i);
                    threadClumpId.erase(threadClumpId.begin() + i);
                    break;
                }
            }
        } while(allThreads.size() > 0 || clumpIterator < this->clumps->end());
    }
}