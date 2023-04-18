#include <iostream>
#include "threadPool.h"
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#define THREADNUM 2

template<class T>
ThreadPool<T>::ThreadPool(int minNum, int maxNum)
    :_minNum(minNum)
     ,_maxNum(maxNum)
     ,_busyNum(0)
     ,_aliveNum(minNum)
     ,_exitNum(0)
     ,_stop(false)
{
    do{
        _task_queue = new TaskQueue<T>(); 
        if(_task_queue == NULL){
            std::cout << "new TaskQueue failed" << std::endl;
            break;
        } 
        _workerIds = new pthread_t[maxNum];
        if(_workerIds == NULL){
            std::cout << "new _workerIds failed" << std::endl;
            break;
        }
        memset(_workerIds, 0, sizeof(pthread_t) * maxNum);
        if(pthread_mutex_init(&_p_mutex, NULL) != 0){
            std::cout << "pthread_mutex_init failed" << std::endl;
            break;
        }
        if(pthread_cond_init(&_p_cond, NULL) != 0){
            std::cout << "pthread_cond_init failed" << std::endl;
            break;
        }
        pthread_create(&_managerId, NULL, manager, this);
        std::cout << "create manager pthread..." << _managerId << std::endl; 
        for(int i = 0; i < _minNum; i++){
            pthread_create(&_workerIds[i], NULL, worker, this);
            std::cout << "create worker pthread..." << _workerIds[i] << std::endl; 
        }
    }while(0);
}

template<typename T>
ThreadPool<T>::~ThreadPool(){
    std::cout << "start ~ThreadPool alive threadnumber is : " << _aliveNum << std::endl;
    _stop = true;
    pthread_join(_managerId, NULL);
    for(int i = 0; i < _aliveNum; i++){
        pthread_cond_signal(&_p_cond);
    }
    for(int i = 0; i < _maxNum; i++){
        if(_workerIds[i] != 0){
            pthread_join(_workerIds[i], NULL);
        }
    }
    if(_task_queue){
        delete _task_queue;
    }
    if(_workerIds){
        delete []_workerIds;
    }
    pthread_mutex_destroy(&this->_p_mutex);
    pthread_cond_destroy(&this->_p_cond);
}

template<class T>
void ThreadPool<T>::append(T* task){
    if(_stop){
        return;
    }
    _task_queue->addTask(task);
    pthread_cond_signal(&this->_p_cond);
}

template<class T>
void* ThreadPool<T>::worker(void* arg){
    ThreadPool<T>* tp = static_cast<ThreadPool<T>*>(arg);
    while(1){
        pthread_mutex_lock(&tp->_p_mutex);
        while(tp->_task_queue->getTaskNum() == 0 && !tp->_stop){
            std::cout << "thread " << pthread_self() << "waiting..." << std::endl;
            pthread_cond_wait(&tp->_p_cond, &tp->_p_mutex);
            if(tp->_exitNum > 0){
                tp->_exitNum--;
                if(tp->_aliveNum > tp->_minNum){
                    tp->_aliveNum--;
                    pthread_mutex_unlock(&tp->_p_mutex);
                    tp->threadExit();
                }
            }
        } 
        if(tp->_stop){
            pthread_mutex_unlock(&tp->_p_mutex);
            tp->threadExit();
        }
        T* task = tp->_task_queue->getTask();
        tp->_busyNum++;
        pthread_mutex_unlock(&tp->_p_mutex);
        task->process();
        delete task->_arg;
        task->_arg = NULL;
        pthread_mutex_lock(&tp->_p_mutex);
        tp->_busyNum--;
        pthread_mutex_unlock(&tp->_p_mutex);
    }
    return NULL;
}

template<class T>
void* ThreadPool<T>::manager(void* arg){
    ThreadPool<T>* tp = static_cast<ThreadPool<T>*>(arg); 
    while(!tp->_stop){
        sleep(5);
        pthread_mutex_lock(&tp->_p_mutex);
        int taskQueueNUm = tp->_task_queue->getTaskNum();
        pthread_mutex_unlock(&tp->_p_mutex);
        pthread_mutex_lock(&tp->_p_mutex);
        if(taskQueueNUm > tp->_aliveNum && tp->_aliveNum < tp->_maxNum){
            int num = 0;
            std::cout << "thread add num : " << THREADNUM << std::endl;
            for(int i = 0; i < tp->_maxNum && num < THREADNUM && tp->_aliveNum < tp->_maxNum; i++){
                if(tp->_workerIds[i] == 0){
                    pthread_create(&tp->_workerIds[i], NULL, worker, tp);
                    tp->_aliveNum++;
                    num++;
                }
            }
        }
        if(tp->_busyNum * 2 < tp->_aliveNum && tp->_aliveNum > tp->_minNum){
            std::cout << "thread reduce num : " << THREADNUM << std::endl;
            tp->_exitNum = THREADNUM;
            for(int i = 0; i < THREADNUM; i++){
                pthread_cond_signal(&tp->_p_cond);
            }
        }
        pthread_mutex_unlock(&tp->_p_mutex);
    }
    return NULL;
}


template<class T>
void ThreadPool<T>::threadExit(){
    pthread_t tid = pthread_self();
    for(int i = 0; i < _maxNum; i++){
        if(_workerIds[i] == tid){
            std::cout << "thread " << pthread_self() << " exiting..." << std::endl;
            _workerIds[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}
