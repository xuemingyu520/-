#include<iostream>
#include<chrono>
#include<thread>

using namespace std;
using Ulong = unsigned long long;
#include"threadpool.h"

/*
��Щ��������ϣ���ܹ���ȡȥ�߳�ִ������ķ���ֵ
������
1 + ��������+30000�ĺ�
thread1 1 + ������+ 10000�ĺ�
thread2 10001 +������+ 20000�ĺ�
������

main thread : ��ÿ�����̷߳����������䣬���ȴ��������귵�ؽ�����ϲ����յĽ������
*/
class MyTask : public Task
{
public:
	MyTask(int begin, int end)
		:begin_(begin)
		, end_(end)
	{}
	//����һ:��ô���run�����ķ���ֵ�����Ա�ʾ���������
	//Java Python    Object   ���������������͵Ļ���
	//C++17  Any����
	Any run()     // run�������վ����̳߳ط�����߳���ȥ��ִ����
	{
		std::cout << " tid:" << std::this_thread::get_id()
			<< "begin!"<<std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		Ulong sum = 0;
		for (Ulong i = begin_; i <= end_; i++)
		{
			sum += i;
		}
		std::cout << " tid:" << std::this_thread::get_id()
			<< "end!" << std::endl;
		return sum;
	}

private:
	int begin_;
	int end_;
};
int main()
{
	Threadpool pool;
	//�û��Լ������̳߳صĹ���ģʽ
	pool.setMode(PoolMode::MODE_CACHED);

	//��ʼ�����̳߳�
	pool.start(4);


	//�����������Result������
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	//����task��ִ���꣬task����û�ˣ�������task�����Result����Ҳû��
	Ulong sum1 = res1.get().cast_<Ulong>(); // get������һ��Any���ͣ���ôת�ɾ��������
	Ulong sum2 = res2.get().cast_<Ulong>();
	Ulong sum3 = res3.get().cast_<Ulong>();

	//Master - Slave�߳�ģ��
	//Master�߳������ֽ�����Ȼ�������Salve�̷ֽ߳�����
	//�ȴ�����Slave�߳�ִ�������񣬷��ؽ��
	//Master�̺߳ϲ����������������
	cout << (sum1 + sum2 + sum3) << endl;

	/*pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());
	pool.submitTask(std::make_shared<MyTask>());*/

	getchar();
} 