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
        function<void(Clump *, int)> threadFunction;
        function<void(Clump *, int)> threadDoneFunction;

        ClumpsThread(int maxThreads,
                     vector<Clump> *clumps,
                     function<void(Clump *, int)> threadFunction,
                     function<void(Clump *, int)> threadDoneFunction);
        void start();
    };

}

#endif //CLUMPSTHREAD_H
