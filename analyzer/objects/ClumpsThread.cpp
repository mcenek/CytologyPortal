#include "ClumpsThread.h"
#include <future>
#include <thread>
#include <functional>

using namespace std;

namespace segment {
    /*
     * Constructor for ClumpsThread
     * maxThreads: maximum number of threads run simutaneously
     * clumps: list of Clumps to run the threads on
     * threadFunction: function to run the treads on, thread function's parameters must be Clump *clump, int clumpIdx
     * threadDoneFunction: function to run after its thread finishes. Parameters must be Clump *clump, int clumpIdx
     */
    ClumpsThread::ClumpsThread(int maxThreads,
                               vector<Clump> *clumps,
                               const function<void(Clump *, int)> &threadFunction,
                               const function<void(Clump *, int)> &threadDoneFunction) {
        this->maxThreads = maxThreads;
        this->clumps = clumps;
        this->threadFunction = threadFunction;
        this->threadDoneFunction = threadDoneFunction;
        start();
    }

    /*
     * start starts the clump threads
     * Threads are continuously spawned until the max thread threshold is reached.
     * The function continously checks for threads that are finished and then runs the thread done function.
     */
    void ClumpsThread::start() {
        vector<shared_future<void>> allThreads;
        vector<int> threadClumpIdx;
        vector<Clump>::iterator clumpIterator = this->clumps->begin();
        do {
            // Fill thread queue
            while (allThreads.size() < this->maxThreads && clumpIterator < this->clumps->end()) {
                // Find the next clump
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - this->clumps->begin();
                clumpIterator++;
                // Create a new thread with the clump
                shared_future<void> thread_object = async(this->threadFunction, clump, clumpIdx);
                //Track the threads
                allThreads.push_back(thread_object);
                threadClumpIdx.push_back(clumpIdx);
            }

            // Wait for a thread to finish every 10 milliseconds
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<void> thread = allThreads[i];
                int clumpIdx = threadClumpIdx[i];
                Clump *clump = &(*this->clumps)[clumpIdx];
                // If thread is finished
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    thread.get();
                    // Run a thread done function if it exists
                    if (this->threadDoneFunction) {
                        this->threadDoneFunction(clump, clumpIdx);
                    }
                    allThreads.erase(allThreads.begin() + i);
                    threadClumpIdx.erase(threadClumpIdx.begin() + i);
                    // Print threads that are still running when the last clump thread finishes running
                    if (allThreads.size() > 0 && clumpIterator == this->clumps->end()) {
                        for (int waitingClumpIdx : threadClumpIdx) {
                            printf("Still waiting for clump: %d\n", waitingClumpIdx);
                        }
                    }
                    break;
                }
            }
        // Repeat if there are threads still running or if we haven't traversed all clumps,
        } while(allThreads.size() > 0 || clumpIterator < this->clumps->end());
    }
}