#include "main.h"

#include <utilsShare.h>
#include <utilsShareMQTT.h>

void TaskConnectionHandler(std::string_view host, std::string_view service)
{
	constexpr std::uint16_t KeepAlive = 15; // sec. //[#] - TaskTransactionWait(..) should be taken into consideration

	utils::share::tConnection Connection(host, service, KeepAlive);

	const bool SessionPresent = Connection.Connect(mqtt::tSessionStateRequest::Continue, "duper_star_Controller"); // 1883
	//if (!SessionPresent)
	{
		std::vector<mqtt::tSubscribeTopicFilter> Filters;
		Filters.emplace_back("SensorA_will", mqtt::tQoS::AtMostOnceDelivery);		// [!] it differs from SensorA
		Filters.emplace_back("SensorA_DateTime", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
		Filters.emplace_back("SensorA_DateTime_0", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
		Filters.emplace_back("SensorA_DateTime_1", mqtt::tQoS::AtLeastOnceDelivery);	// [!] it differs from SensorA
		Filters.emplace_back("SensorA_DateTime_2", mqtt::tQoS::ExactlyOnceDelivery);	// [!] it differs from SensorA
		//Filters.emplace_back("SensorA_DateTime_0", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
		//Filters.emplace_back("SensorA_DateTime_1", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
		//Filters.emplace_back("SensorA_DateTime_2", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
		Connection.Subscribe(Filters);
	}
	
	std::string Settings = "Hello World!";
	//Connection.Publish_AtMostOnceDelivery(true, "SensorA_Settings", { Settings.begin(), Settings.end() });
	//Connection.Publish_AtLeastOnceDelivery(true, false, "SensorA_Settings", { Settings.begin(), Settings.end() });
	Connection.Publish_ExactlyOnceDelivery(true, false, "SensorA_Settings", { Settings.begin(), Settings.end() });

	/////////////////////////////////////
	for (int i = 0; i < 1500; ++i)
	{
		if (!Connection.IsConnected())
			THROW_RUNTIME_ERROR("The connection has been broken by the MQTT broker.");

		while (!Connection.IsIncomingEmpty())
		{
			auto IncMsg = Connection.GetIncoming();
			g_Log.PublishMessage(IncMsg.TopicName, IncMsg.Payload);
		}

		std::this_thread::sleep_for(std::chrono::seconds(1));

		/*switch (i)
		{
		case 150:
		{
			std::vector<mqtt::tString> Filters;
			Filters.push_back("SensorA_DateTime_0");
			Filters.push_back("SensorA_DateTime_1");
			Filters.push_back("SensorA_DateTime_2");
			Connection.Unsubscribe(Filters);
			break;
		}
		case 300:
		{
			std::vector<mqtt::tSubscribeTopicFilter> Filters;
			Filters.emplace_back("SensorA_will", mqtt::tQoS::AtMostOnceDelivery);		// [!] it differs from SensorA
			Filters.emplace_back("SensorA_DateTime", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
			Filters.emplace_back("SensorA_DateTime_0", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
			Filters.emplace_back("SensorA_DateTime_1", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
			Filters.emplace_back("SensorA_DateTime_2", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
			Connection.Subscribe(Filters);
			break;
		}
		}*/
	}
	/////////////////////////////////////

	Connection.Disconnect();
}
