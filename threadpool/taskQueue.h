#ifndef _TASKQUEUE_H
#define _TASKQUEUE_H

#include <iostream>
#include <queue>
#include <pthread.h>



template<class T>
class TaskQueue{
public:
    TaskQueue();
    ~TaskQueue();
    // 添加任务
    void addTask(T* task);
    // 获取任务
    T* getTask();
    // 获取当前任务数量
    inline int getTaskNum(){
        return _t_queue.size();
    }
private:
    std::queue<T*> _t_queue; 
    pthread_mutex_t _t_mutex;
};


template<class T>
TaskQueue<T>::TaskQueue(){
    pthread_mutex_init(&_t_mutex, NULL);
}

template<class T>
TaskQueue<T>::~TaskQueue(){
    pthread_mutex_destroy(&this->_t_mutex);
}

template<class T>
void TaskQueue<T>::addTask(T* task){
    pthread_mutex_lock(&_t_mutex);
    _t_queue.push(task);
    pthread_mutex_unlock(&_t_mutex);
}



template<class T>
T* TaskQueue<T>::getTask(){
    pthread_mutex_lock(&_t_mutex);
    T* task;
    if(_t_queue.size() > 0){
        task = _t_queue.front(); 
        _t_queue.pop();
    }
    pthread_mutex_unlock(&_t_mutex);
    return task; 
}

#endif 
