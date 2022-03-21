//
// Created by Ming on 2022/2/24.
//

#ifndef INTEL_NUMUTILS_H
#define INTEL_NUMUTILS_H
#include<iostream>
#include<cstdlib>
#include<time.h>

class numUtils {
public:
    int numberRandom(){
        return rand()%100;
    }

    int aNumberRandom(){
        srand(time(NULL));
        return numberRandom();
    }

    void bNumberRandom(double data[]){
        srand(time(NULL));
        for (int i = 0; i < sizeof(data); i++) {
            data[i] = numberRandom()*0.01;
        }
    }
};


#endif //INTEL_NUMUTILS_H
