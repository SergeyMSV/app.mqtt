#include "main.h"

#include <future>
#include <thread>
#include <utility>

#include <utilsException.h>
#include <utilsExits.h>
#include <shareLog.h>
#include <utilsTime.h>

void TaskConnectionHandler(std::string_view host, std::string_view service, const std::string& sensorData);

int main()
{
	while (true)
	{
		try
		{
			share::tMeasureDuration Measure("Sending measurements...");

			const std::string SensorData = utils::time::tDateTime::Now().ToString();

			try
			{
				std::future<void> TaskConnectionFuture = std::async(std::launch::async, TaskConnectionHandler, "test.mosquitto.org", "1883", SensorData);
				//std::future<void> TaskConnectionFuture = std::async(std::launch::deferred, TaskConnectHandler, std::ref(Socket)); // a task is not started by wait_for(..), it'll be deferred forever

				TaskConnectionFuture.get();
			}
			catch (std::exception& ex)
			{
				g_Log.Exception(ex.what());
			}
		}
		catch (std::exception& ex)
		{
			g_Log.Exception(ex.what());
		}

		g_Log.TestMessage("NO CONNECTION");

		share::tMeasureDuration Measure("Sleeping...");
		std::this_thread::sleep_for(std::chrono::seconds(60)); // [#] pause - it can be in the settings
	}

	return utils::exit_code::EX_OK;
}