#include <iostream>
#include <exception>
#include <pthread.h>
#include <semaphore.h>

#ifndef LOCKER_H
#define LOCKER_H

class sem {
private:
	sem_t m_sem;
public:
	sem() {
		if (sem_init(&m_sem, 0, 0) != 0) {
			throw::std::exception();
		}
	}
	sem(int num)//numΪ��Դ��
	{
		if (sem_init(&m_sem, 0, num) != 0)
		{
			throw std::exception();
		}
	}

	~sem() {
		sem_destroy(&m_sem);
	}

	bool wait() {
		return sem_wait(&m_sem) == 0;
	}

	bool post() {
		return sem_post(&m_sem) == 0;
	}
};

class locker {
public:
	//���������ٻ�����
	locker() {
		if (pthread_mutex_init(&m_mutex, NULL) != 0) {
			throw std::exception();
		}
	}

	//���ٻ�����
	~locker() {
		pthread_mutex_destroy(&m_mutex);
	}

	//��ȡ������
	bool lock() {
		return pthread_mutex_lock(&m_mutex) == 0;
	}

	//�ͷŻ�����
	bool unlock() {
		return pthread_mutex_unlock(&m_mutex) == 0;
	}
private:
	pthread_mutex_t m_mutex;
};


class cond {

public:
	//��������ʼ����������
	cond() {
		if (pthread_mutex_init(&m_mutex, NULL) != 0) {
			throw std::exception();
		}
		if (pthread_cond_init(&m_cond, NULL) != 0) {
			//���캯���������⣬�ͷ��Ѿ��������Դ
			pthread_mutex_destroy(&m_mutex);
			throw std::exception();
		}
	}

	//������������
	~cond() {
		pthread_mutex_destroy(&m_mutex);
		pthread_cond_destroy(&m_cond);
	}

	//�ȴ���������
	bool wait() {
		int ret = 0;
		pthread_mutex_lock(&m_mutex);
		ret = pthread_cond_wait(&m_cond, &m_mutex);
		pthread_mutex_unlock(&m_mutex);
		return ret == 0;
	}

	//���ѵȴ������������߳�
	bool signal() {
		return pthread_cond_signal(&m_cond) == 0;
	}
private:
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;
};


// class LockMap {
// 	LockMap()

// private:
// 	unordered_map<int, int> mp; 	//userId -> sockfd
// 	locker lock;
// };

#endif