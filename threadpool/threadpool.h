#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include "taskQueue.h"

#define THREADNUM 2

template<class T>
class ThreadPool{
public:
    ThreadPool(int minNUm, int maxNUm);
    ~ThreadPool();
    void append(T* task);
private:
    static void* worker(void* arg);
    static void* manager(void* arg);
    void threadExit();
private:
    pthread_mutex_t m_mutex;   //互斥锁
    pthread_cond_t m_cond;   //条件变量
    pthread_t m_manager;   //管理线程
    pthread_t* m_workers;  //工作线程
    TaskQueue<T>* m_taskqueue;   //任务队列
    int m_minNum;
    int m_maxNum;
    int m_busyNum;
    int m_aliveNum;
    int m_exitNum;
    bool m_stop;
};

template<class T>
ThreadPool<T>::ThreadPool(int minNum, int maxNum)
    :m_minNum(minNum)
     ,m_maxNum(maxNum)
     ,m_busyNum(0)
     ,m_aliveNum(minNum)
     ,m_exitNum(0)
     ,m_stop(false) {
    m_taskqueue = new TaskQueue<T>(); 
    if(m_taskqueue == NULL){
        throw std::exception();
    } 
    m_workers = new pthread_t[maxNum];
    if(m_workers == NULL){
        throw std::exception();
    }
    memset(m_workers, 0, sizeof(pthread_t) * maxNum);
    if(pthread_mutex_init(&m_mutex, NULL) != 0){
        throw std::exception();
    }
    if(pthread_cond_init(&m_cond, NULL) != 0){
        throw std::exception();
    }
    pthread_create(&m_manager, NULL, manager, this);
    for(int i = 0; i < m_minNum; i++){
        pthread_create(&m_workers[i], NULL, worker, this);
        
    }
    
}

template<typename T>
ThreadPool<T>::~ThreadPool(){
    m_stop = true;
    pthread_join(m_manager, NULL);
    for(int i = 0; i < m_aliveNum; i++){
        pthread_cond_signal(&m_cond);
    }
    for(int i = 0; i < m_maxNum; i++){
        if(m_workers[i] != 0){
            pthread_join(m_workers[i], NULL);
        }
    }
    if(m_taskqueue){
        delete m_taskqueue;
    }
    if(m_workers){
        delete []m_workers;
    }
    pthread_mutex_destroy(&this->m_mutex);
    pthread_cond_destroy(&this->m_cond);
}

template<class T>
void ThreadPool<T>::append(T* task){
    if(m_stop){
        return;
    }
    m_taskqueue->addTask(task);
    pthread_cond_signal(&this->m_cond);
}

template<class T>
void* ThreadPool<T>::worker(void* arg){
    ThreadPool<T>* tp = static_cast<ThreadPool<T>*>(arg);
    while(1){
        pthread_mutex_lock(&tp->m_mutex);
        while(tp->m_taskqueue->getTaskNum() == 0 && !tp->m_stop){
            pthread_cond_wait(&tp->m_cond, &tp->m_mutex);
            if(tp->m_exitNum > 0){
                tp->m_exitNum--;
                if(tp->m_aliveNum > tp->m_minNum
        ){
                    tp->m_aliveNum--;
                    pthread_mutex_unlock(&tp->m_mutex);
                    tp->threadExit();
                }
            }
        } 
        if(tp->m_stop){
            pthread_mutex_unlock(&tp->m_mutex);
            tp->threadExit();
        }
        T* task = tp->m_taskqueue->getTask();
        tp->m_busyNum++;
        pthread_mutex_unlock(&tp->m_mutex);
        task->process();
        pthread_mutex_lock(&tp->m_mutex);
        tp->m_busyNum--;
        pthread_mutex_unlock(&tp->m_mutex);
    }
    return NULL;
}

template<class T>
void* ThreadPool<T>::manager(void* arg){
    ThreadPool<T>* tp = static_cast<ThreadPool<T>*>(arg); 
    while(!tp->m_stop){
        sleep(5);
        pthread_mutex_lock(&tp->m_mutex);
        int taskQueueNUm = tp->m_taskqueue->getTaskNum();
        pthread_mutex_unlock(&tp->m_mutex);
        pthread_mutex_lock(&tp->m_mutex);
        if(taskQueueNUm > tp->m_aliveNum && tp->m_aliveNum < tp->m_maxNum){
            int num = 0;
            for(int i = 0; i < tp->m_maxNum && num < THREADNUM && tp->m_aliveNum < tp->m_maxNum; i++){
                if(tp->m_workers[i] == 0){
                    pthread_create(&tp->m_workers[i], NULL, worker, tp);
                    tp->m_aliveNum++;
                    num++;
                }
            }
        }
        if(tp->m_busyNum * 2 < tp->m_aliveNum && tp->m_aliveNum > tp->m_minNum
){
            tp->m_exitNum = THREADNUM;
            for(int i = 0; i < THREADNUM; i++){
                pthread_cond_signal(&tp->m_cond);
            }
        }
        pthread_mutex_unlock(&tp->m_mutex);
    }
    return NULL;
}


template<class T>
void ThreadPool<T>::threadExit(){
    pthread_t tid = pthread_self();
    for(int i = 0; i < m_maxNum; i++){
        if(m_workers[i] == tid){
            m_workers[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}



#endif
