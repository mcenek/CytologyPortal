#include "ClumpsThread.h"
#include <future>
#include <thread>
#include <functional>

using namespace std;

namespace segment {
    ClumpsThread::ClumpsThread(int maxThreads,
                               vector<Clump> *clumps,
                               const function<void(Clump *, int)> &threadFunction,
                               const function<void(Clump *, int)> &threadDoneFunction) {
        this->maxThreads = maxThreads;
        this->clumps = clumps;
        this->threadFunction = threadFunction;
        this->threadDoneFunction = threadDoneFunction;
    }

    void ClumpsThread::start() {
        vector<shared_future<void>> allThreads;
        vector<int> threadClumpIdx;
        vector<Clump>::iterator clumpIterator = this->clumps->begin();
        do {
            // Fill thread queue
            while (allThreads.size() < this->maxThreads && clumpIterator < this->clumps->end()) {
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - this->clumps->begin();
                clumpIterator++;
                shared_future<void> thread_object = async(this->threadFunction, clump, clumpIdx);
                allThreads.push_back(thread_object);
                threadClumpIdx.push_back(clumpIdx);
            }

            // Wait for a thread to finish
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<void> thread = allThreads[i];
                int clumpIdx = threadClumpIdx[i];
                Clump *clump = &(*this->clumps)[clumpIdx];
                // If thread is finished
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    thread.get();
                    if (this->threadDoneFunction) {
                        this->threadDoneFunction(clump, clumpIdx);
                    }
                    allThreads.erase(allThreads.begin() + i);
                    threadClumpIdx.erase(threadClumpIdx.begin() + i);
                    break;
                }
            }
        // Repeat if there are threads still running or if we haven't traversed all clumps,
        } while(allThreads.size() > 0 || clumpIterator < this->clumps->end());
    }
}