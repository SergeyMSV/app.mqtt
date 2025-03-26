#include "main.h"

#include <future>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsExits.h>
#include <utilsPacketMQTT.h>

test::tLogger g_Log;

using boost::asio::ip::tcp;

void TaskConnectionHandler(tcp::socket& socket);

int main()
{
	int ExitCode = utils::exit_code::EX_OK;

	try
	{
		utils::GetLogMessage
		test::tMeasureDuration Measure("MAIN");
		//for (;;)
		{
			boost::asio::io_context ioc;

			tcp::resolver Resolver(ioc);
			tcp::resolver::results_type Ep = Resolver.resolve("test.mosquitto.org", "1883"); // MQTT, unencrypted, unauthenticated
			//tcp::resolver::results_type Ep = resolver.resolve("test.mosquitto.org", "1884"); // 1884 : MQTT, unencrypted, authenticated

			tcp::socket Socket(ioc);

			try
			{
				boost::asio::connect(Socket, Ep);

				std::future<void>TaskConnectionFuture = std::async(std::launch::async, TaskConnectionHandler, std::ref(Socket));
				//std::future<void>TaskConnectFuture = std::async(std::launch::deferred, TaskConnectHandler, std::ref(Socket)); // a task is not started by wait_for(..), it'll be deferred forever

				//std::future<void>TaskMeasureFuture = std::async(std::launch::async, TaskMeasureHandler);

				g_Log.TestMessage("SOME IMPORTANT WORK STARTED");
				std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // some important work...
				g_Log.TestMessage("SOME IMPORTANT WORK FINISHED");

				//MeasureData = TaskMeasureFuture.get();

				//TaskConnectFuture.wait();

				std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // some important work...

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

				g_Log.TestMessage("SOME NOT IMPORTANT WORK");

				TaskConnectionFuture.get();
			}
			catch (std::exception& ex)
			{
				g_Log.Exception(ex.what());
			}

			Socket.close();

			g_Log.Operation("WAIT");

			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
	catch (std::exception& ex)
	{
		g_Log.Exception(ex.what());

		// g_DataSetMainControl.Thread_GNSS_State = tDataSetMainControl::tStateGNSS::Exit; notify all the threads that it's time to exit

		ExitCode = utils::exit_code::EX_IOERR;
	}

	return ExitCode;
}