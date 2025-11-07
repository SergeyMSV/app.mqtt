#include "main.h"

#include <future>
#include <iostream>
#include <thread>
#include <utility>

#include <utilsException.h>
#include <utilsExits.h>
#include <shareLog.h>

void TaskConnectionHandler(std::string_view host, std::string_view service);

int main()
{
	int ExitCode = utils::exit_code::EX_OK;

	try
	{
		share::tMeasureDuration Measure("MAIN");
		//for (;;)
		{

			try
			{
				std::future<void> TaskConnectionFuture = std::async(std::launch::async, TaskConnectionHandler, "test.mosquitto.org", "1883");
				//std::future<void> TaskConnectionFuture = std::async(std::launch::deferred, TaskConnectHandler, std::ref(Socket)); // a task is not started by wait_for(..), it'll be deferred forever

				g_Log.TestMessage("SOME IMPORTANT WORK STARTED");
				for (int i = 0; i < 5; ++i)
				{
					if (TaskConnectionFuture.wait_for(std::chrono::seconds(1)) == std::future_status::ready)
					{
						g_Log.TestMessage("SOME IMPORTANT WORK INTERRUPTED");
						break;
					}
				}
				g_Log.TestMessage("SOME IMPORTANT WORK FINISHED");

				//std::future_status Status;
				//do
				//{
				//	Status = TaskConnectionFuture.wait_for(std::chrono::milliseconds(1));
				//	switch (Status)
				//	{
				//	case std::future_status::deferred:
				//		std::cout << "deferred\n";
				//		break;
				//	case std::future_status::timeout:
				//		std::cout << "timeout\n";
				//		break;
				//	case std::future_status::ready:
				//		std::cout << "ready!\n";
				//		break;
				//	}
				//} while (Status != std::future_status::ready);

				TaskConnectionFuture.get();
			}
			catch (std::exception& ex)
			{
				g_Log.Exception(ex.what());
			}

			g_Log.Operation("WAIT");

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
	catch (std::exception& ex)
	{
		g_Log.Exception(ex.what());
		ExitCode = utils::exit_code::EX_IOERR;
	}

	return ExitCode;
}