#include <iostream>
#include "taskQueue.h"

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


