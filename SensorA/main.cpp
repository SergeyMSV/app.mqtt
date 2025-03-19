#include <future>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include <utilsException.h>
#include <utilsExits.h>
#include <utilsPacketMQTT.h>

using boost::asio::ip::tcp;

void TaskConnectionHandler(tcp::socket& socket);

int main()
{
	int ExitCode = utils::exit_code::EX_OK;

	try
	{
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

				std::cout << "SOME IMPORTANT WORK STARTED\n";
				Sleep(5000); // some important work...
				std::cout << "SOME IMPORTANT WORK FINISHED\n";

				//MeasureData = TaskMeasureFuture.get();

				//TaskConnectFuture.wait();

				Sleep(5); // some important work...

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

				std::cout << "SOME NOT IMPORTANT WORK\n";

				TaskConnectionFuture.get();
			}
			catch (std::exception& ex)
			{
				std::cerr << ex.what() << '\n';
			}

			Socket.close();
			std::cout << "WAIT\n";
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << '\n';

		// g_DataSetMainControl.Thread_GNSS_State = tDataSetMainControl::tStateGNSS::Exit; notify all the threads that it's time to exit

		ExitCode = utils::exit_code::EX_IOERR;
	}

	return ExitCode;
}