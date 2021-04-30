#ifndef CLUMPSTHREAD_H
#define CLUMPSTHREAD_H

#include "Clump.h"
#include <functional>

using namespace std;

namespace segment {
    class ClumpsThread {
    public:
        int maxThreads;
        vector<Clump> *clumps;
        void (*threadFunction)(*Clump, int);
        void (*threadDoneFunction)(*Clump, int);

        ClumpsThread(int maxThreads,
                     vector<Clump> *clumps,
                     void (*threadFunction)(*Clump, int),
                     void (*threadDoneFunction)(*Clump, int));
        void start();
    };

}

#endif //CLUMPSTHREAD_H
