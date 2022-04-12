// ThreadPoolApi.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <windows.h>
#include <memory>
#include <stdio.h>
#include <functional>

#define THROW_THREADPOOL_ERROR(message) {throw threadpool_exception(message, __FILE__, __FUNCTION__ , __LINE__);}

class threadpool_exception : public  std::exception
{
public:
	threadpool_exception(const char* message, const char* file, const char* function, int line) noexcept : errorMsg{}
	{
		sprintf_s(errorMsg, 256, "%s %s %d %s", file, function, line, message);
	}

	const char* what() const noexcept override
	{
		return errorMsg;
	}
private:
	char errorMsg[256];
};


class ThreadPoolWrapper final
{
public:
	ThreadPoolWrapper(DWORD numberOfThreads)
		: m_maxNumberOfThreads(numberOfThreads)
		, m_pCleanupGroup(nullptr)
		, m_pThreadPool(nullptr)
		, m_pWorkObject(nullptr)
	{
		InitializeThreadpoolEnvironment(&m_CallBackEnviron);

		m_pThreadPool = CreateThreadpool(nullptr);
		if (nullptr == m_pThreadPool) {
			THROW_THREADPOOL_ERROR("Can not create thread pool");
		}

		const auto bRetValue = SetThreadpoolThreadMinimum(m_pThreadPool, 1);
		if (FALSE == bRetValue)	{
			CloseThreadpool(m_pThreadPool);
			THROW_THREADPOOL_ERROR("Can not set minimal number of threads");
		}

		m_pCleanupGroup = CreateThreadpoolCleanupGroup();
		if (nullptr == m_pCleanupGroup)	{
			CloseThreadpool(m_pThreadPool);
			THROW_THREADPOOL_ERROR("Can not create cleanup group");
		}

		SetThreadpoolCallbackCleanupGroup(&m_CallBackEnviron, m_pCleanupGroup, nullptr);
	}

	ThreadPoolWrapper(const ThreadPoolWrapper& other) = delete;
	ThreadPoolWrapper& operator = (const ThreadPoolWrapper& other) = delete;

	ThreadPoolWrapper(ThreadPoolWrapper&& other) = delete;
	ThreadPoolWrapper& operator = (ThreadPoolWrapper&& other) = delete;

	~ThreadPoolWrapper() {
		WaitForThreadpoolWorkCallbacks(m_pWorkObject, FALSE);
		CloseThreadpoolCleanupGroupMembers(m_pCleanupGroup, FALSE, nullptr);
		CloseThreadpoolCleanupGroup(m_pCleanupGroup);
		CloseThreadpool(m_pThreadPool);
	}

	void CreateWorkObjectAndAddToThreadPool(PTP_WORK_CALLBACK pFunctionForWorkObject, PVOID pParamsToFunction) {
		//create work object with thread funcion
		m_pWorkObject = CreateThreadpoolWork(pFunctionForWorkObject, pParamsToFunction, nullptr);

		if (nullptr == m_pWorkObject) {
			CloseThreadpoolCleanupGroup(m_pCleanupGroup);
			CloseThreadpool(m_pThreadPool);
			THROW_THREADPOOL_ERROR("Can not create work object");
		}
	}

	void SubmitWork() const noexcept {
		SubmitThreadpoolWork(m_pWorkObject);
	}

private:
	DWORD m_maxNumberOfThreads;
	PTP_CLEANUP_GROUP m_pCleanupGroup;
	PTP_POOL m_pThreadPool;
	PTP_WORK m_pWorkObject;
	TP_CALLBACK_ENVIRON m_CallBackEnviron;
};

typedef std::function<int(int, int)> thread_handler;


VOID WINAPI ThreadFunction(PTP_CALLBACK_INSTANCE Instance, LPVOID lpParam, PTP_WORK Work) noexcept {
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(Work);
	
	auto ret = (*reinterpret_cast<thread_handler *>(lpParam))(1,2);
	printf("%i\n", ret);
}


void TestThreadPool(thread_handler t) {
	constexpr size_t numOfTusk = 20;
	ThreadPoolWrapper threadPool(static_cast<DWORD>(10));

	threadPool.CreateWorkObjectAndAddToThreadPool(static_cast<PTP_WORK_CALLBACK>(ThreadFunction), reinterpret_cast<PVOID>(&t));

	for (size_t i = 0; i < numOfTusk; i++)
	{
		threadPool.SubmitWork();
	}
}

int main()
{	
	try {
		TestThreadPool([&](int lhs, int rhs) -> int {
			return lhs + rhs; 
		});
	}
	catch (const std::exception &e) {
		printf("%s", e.what());
	}	
}
