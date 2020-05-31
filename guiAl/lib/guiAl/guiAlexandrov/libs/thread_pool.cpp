#include "thread_pool.h"

namespace gui
{

	thread_pool::thread_pool(size_t threads)
	{
		size = MIN(std::thread::hardware_concurrency(), threads);
		stopping = false;
		start();
	}

	thread_pool::~thread_pool() { stop(); }

	template <typename T>
	auto thread_pool::add_task(T task)->std::future<decltype(task())>
	{
		auto wrapper = std::make_shared<std::packaged_task<decltype(task()) ()>>(std::move(task));
		{
			std::unique_lock<std::mutex> lock(event_mutex);
			tasks.push([=]() { (*wrapper)(); });
		}
		event.notify_one();
		return wrapper->get_future();
	}


	//private

	void thread_pool::start()
	{
		for (int i = 0; i < size; i++)
		{
			pool.push_back(std::thread([&]() {
				while (true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(event_mutex);

						event.wait(lock, [&]() { return stopping || !tasks.empty(); });
						if (stopping && tasks.empty()) break;

						task = std::move(tasks.front());
						tasks.pop();
					}
					task();
				}
				}));
		}
	}

	void thread_pool::stop() noexcept
	{
		{
			std::unique_lock<std::mutex> lock(event_mutex);
			stopping = true;
		}

		event.notify_all();

		for (std::thread& thread : pool)
			thread.join();
	}

	// global
	thread_pool workers(MAX_THREADS);

}