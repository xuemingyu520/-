#include"threadpool.h"

#include<functional>
#include<thread>
#include<iostream>

const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_IDLE_TIME = 60;     //��λ��s
//�̳߳ع��캯��
Threadpool::Threadpool()
	:initThreadSize_(4)
	, taskSize_(0)
	, idleThreadSize_(0)
	, curThreadSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	,isPoolRunning_(false)
{}

//�̳߳���������
Threadpool::~Threadpool()
{}

//�����̳߳صĹ���ģʽ
void Threadpool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_  =  mode;
}

/*
//���ó�ʼ���߳�����
void Threadpool::setInitThreadSize(int size)
{
	initThreadSize_ = size;
}
*/
// ����task�������������ֵ
void Threadpool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())
	{
		return;
	}
	taskQueMaxThreshHold_ = threshhold;
}

// �����̳߳�cacheģʽ���߳���ֵ
void Threadpool::setThreadSizeThreshHold(int threshhold)
{
	if (checkRunningState())
	{
		return;
	}
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;

	}
}

//���̳߳��ύ����    �û����øýӿڣ��������������������
Result Threadpool::submitTask(std::shared_ptr<Task>sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//�̵߳�ͨ�� �ȴ���������п���
	//�û��ύ�����������������1s�������ж��ύ����ʧ�ܣ�����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		//��ʾnotFull�ȴ�1s�ӣ�������Ȼû������
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp,false);    //Task Result
	}
		
	//����п��࣬������������������
	taskQue_.emplace(sp);
	taskSize_++;

	//��Ϊ�·�������������п϶������ˣ�notEmpty_֪ͨ
	notEmpty_.notify_all();

	// cachedģʽ����������ȽϽ�����������С��������� ��Ҫ�������������Ϳ����̵߳��������ж��Ƿ���Ҫ�����µ��̳߳�����
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idleThreadSize_
		&& curThreadSize_ < threadSizeThreshHold_)
	{
		std::cout << ">>> create new thread..." << std::endl;

		// �����µ��̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start();
		//�޸��̸߳�����صı���
		curThreadSize_++;
		idleThreadSize_++;
	}

	//���������Result����
	return Result(sp);
} 

//�����̳߳�
void Threadpool::start(int initThreadSize)
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;

	//��¼��ʼ�̸߳���
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;

	//�����̶߳���
	for (int i = 0; i < initThreadSize_; i++)
	{
		//����thread�̶߳����ʱ�򣬰��̺߳�������thread�̶߳���
		auto ptr = std::make_unique<Thread>(std::bind(&Threadpool::threadFunc, this, std::placeholders::_1));
		int threadId = ptr->getId();
		threads_.emplace(threadId,std::move(ptr));
	}

	//���������߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();    //��Ҫȥִ��һ���̺߳���
		idleThreadSize_++;       //��¼��ʼ�����̵߳�����
	}
}

//�����̺߳���    �̳߳ص������̴߳��������������������
void Threadpool::threadFunc(int threadid)   //�̺߳������أ���Ӧ���߳�Ҳ�ͽ�����
{
	auto lastTime = std::chrono::high_resolution_clock().now();
	
	for (;;)
	{
		std::shared_ptr<Task> task;
		{	//�Ȼ�ȡ��
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << " tid:" << std::this_thread::get_id()
				<< "���Ի�ȡ����..." << std::endl;

			// cachedģʽ�£��п����Ѿ������˺ܶ���̣߳����ǿ���ʱ�䳬��60s��Ӧ�ðѶ�����߳̽������յ�
			// �������յ�(����initThreadSize_�������߳�Ҫ���л���)
			// ��ǰʱ�䣬��һ���߳�ִ�е�ʱ�� > 60s
			if (poolMode_ == PoolMode::MODE_CACHED)
			{
				//ÿһ���з���һ��  ��ô���֣���ʱ���ػ����������ִ�з���
				while (taskQue_.size() == 0)
				{
					//������������ʱ������
					if(std::cv_status::timeout ==
						notEmpty_.wait_for(lock, std::chrono::seconds(1)))
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= THREAD_IDLE_TIME
							&&curThreadSize_>initThreadSize_)
						{
							// ��ʼ���յ�ǰ�߳�
							// ��¼�߳���������ر������޸�
							// ���̶߳�����߳��б�������ɾ��,û�а취 threadFunc <=>thread����
							// threadid =>thread����=>ɾ��
							threads_.erase(threadid);   //std::this_thread::getid()
							curThreadSize_--;
							idleThreadSize_--;

							std::cout << "threadid:" << std::this_thread::get_id() << "exit!"
								<< std::endl;
							return;
						}
					}
				}
			}
			else
			{
				//�ȴ�notEmpty����
				notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			}

			//�ȴ�notEmpty����
			notEmpty_.wait(lock, [&]()->bool {return taskQue_.size() > 0; });
			idleThreadSize_--;

			std::cout << " tid:" << std::this_thread::get_id()
				<< "��ȡ����ɹ�..." << std::endl;

			//�����������ȡһ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//�����Ȼ��ʣ�����񣬼���֪ͨ�������߳�ִ������
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			//ȡ��һ�����񣬽���֪ͨ,֪ͨ���Լ����ύ��������
			notFull_.notify_all();
		} //��Ӧ�ð����ͷŵ�

		//��ǰ�̸߳���ִ���������
		if (task != nullptr)
		{
			//task->run();	//ִ����һ�����񣬵ý���֪ͨ��������ķ���ֵsetVal��������Result
			task->exec();
		}
		

		idleThreadSize_++;
		lastTime = std::chrono::high_resolution_clock().now();    //�����߳�ִ���������ʱ��
	}
}

bool Threadpool::checkRunningState() const
{
	return isPoolRunning_;
}
/////////////////////////�̷߳���ʵ��
int Thread::generateId_ = 0;

//�̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{}
//�߳�����
Thread::~Thread()
{}
//�����߳�
void Thread::start()
{
	//����һ���߳���ִ��һ���̺߳���
	std::thread t(func_,threadId_);   //C++11��˵ �̶߳���t ���̺߳���func_
	t.detach();             //���÷����߳�   pthread_detach pthread_t���óɷ����߳�
}

int Thread::getId()const
{
	return threadId_;
}
/////////////////////Task������ʵ��
Task::Task()
	:result_(nullptr)
{}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());  //���﷢�Ͷ�̬����
	}
}
 
void Task::setResult(Result* res)
{
	result_ = res;
}
///////////////////// Result������ʵ��
Result::Result(std::shared_ptr<Task>task, bool isValid)
	: isValid_(isValid)
	, task_(task)
{
	task_->setResult(this);
}
Any Result::get()
{
	if (!isValid_)
	{
		return "";

	}

	sem_.wait();    //task�������û��ִ���꣬����������û����߳�
	return std::move(any_);
}
void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post();   //�Ѿ���ȡ������ķ���ֵ�������ź�����Դ
}