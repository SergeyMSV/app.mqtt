#include "main.h"

#include <format>
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

void TaskConnectionHandler(tcp::socket& socket, const std::string sensorData);

int main()
{
	while (true)
	{
		try
		{
			utils::tMeasureDuration Measure("Sending measurements...");

			boost::asio::io_context ioc;

			tcp::resolver Resolver(ioc);
			tcp::resolver::results_type Ep = Resolver.resolve("test.mosquitto.org", "1883"); // MQTT, unencrypted, unauthenticated
			//tcp::resolver::results_type Ep = resolver.resolve("test.mosquitto.org", "1884"); // 1884 : MQTT, unencrypted, authenticated
			tcp::socket Socket(ioc);

			const std::string SensorData = std::format("{:%F %T}", floor<std::chrono::seconds>(std::chrono::system_clock::now()));

			try
			{
				boost::asio::connect(Socket, Ep);

				std::future<void>TaskConnectionFuture = std::async(std::launch::async, TaskConnectionHandler, std::ref(Socket), SensorData);
				//std::future<void>TaskConnectFuture = std::async(std::launch::deferred, TaskConnectHandler, std::ref(Socket)); // a task is not started by wait_for(..), it'll be deferred forever

				TaskConnectionFuture.get();
			}
			catch (std::exception& ex)
			{
				g_Log.Exception(ex.what());
			}

			Socket.close();
		}
		catch (std::exception& ex)
		{
			g_Log.Exception(ex.what());
		}

		utils::tMeasureDuration Measure("Sleeping...");
		std::this_thread::sleep_for(std::chrono::seconds(10)); // [#] pause - it can be in the settings
	}

	return utils::exit_code::EX_OK;
}