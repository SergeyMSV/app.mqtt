#include "main.h"

#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>

#include <boost/asio.hpp>

#include <utilsExits.h>
#include <utilsShare.h>
#include <utilsShareMQTT.h>

//#define MQTT_PUBLISH_QOS_0
//#define MQTT_PUBLISH_QOS_1
#define MQTT_PUBLISH_QOS_2

void TaskConnectionHandler(tcp::socket& socket, const std::string sensorData)
{
	constexpr std::uint16_t KeepAlive = 10; // sec.

	{
		//test::tMeasureDuration TCH("TCH-CONNECT");

		mqtt::tPacketCONNECT Pack(false, KeepAlive, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

		std::future<std::optional<mqtt::tPacketCONNECT::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketCONNECT>(socket, Pack); });

		utils::share::TaskTransactionWait(TaskFuture, 10000, "CONNECT");

		auto PackRsp = TaskFuture.get();
		if (PackRsp.has_value())
			std::cout << "SessPres: " << (int)PackRsp->GetVariableHeader().ConnectAcknowledgeFlags.Field.SessionPresent << '\n';
	}
	/////////////////////////////////////////////////////////////
#ifdef MQTT_PUBLISH_QOS_0
	{
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::AtMostOnceDelivery>;
		tPackPublish Pack(false, true, "SensorA_DateTime", std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBACK");
		TaskFuture.get(); // in case of exception - get it here
	}
#endif // MQTT_PUBLISH_QOS_0
#ifdef MQTT_PUBLISH_QOS_1
	{
		static std::uint16_t PacketId = 0;
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::AtLeastOnceDelivery>;
		tPackPublish Pack(false, true, "SensorA_DateTime", ++PacketId, std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBACK");
		auto PackRsp = TaskFuture.get(); // in case of exception - get it here
		if (PackRsp.has_value())
			std::cout << "rsp puback: " << (int)PackRsp->GetVariableHeader().PacketId.Value << '\n';
	}
#endif // MQTT_PUBLISH_QOS_1
#ifdef MQTT_PUBLISH_QOS_2
	{
		static std::uint16_t PacketId = 0;
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::ExactlyOnceDelivery>;
		tPackPublish Pack(false, true, "SensorA_DateTime", ++PacketId, std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBREC");
		auto PackRsp = TaskFuture.get(); // in case of exception - get it here
		if (PackRsp.has_value())
		{
			std::cout << "rsp pubrec: " << (int)PackRsp->GetVariableHeader().PacketId.Value << '\n';

			mqtt::tPacketPUBREL Pack(PacketId);
			std::future<std::optional<mqtt::tPacketPUBREL::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketPUBREL>(socket, Pack); });
			utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBCOMP");

			auto PackRsp = TaskFuture.get(); // in case of exception - get it here
			if (PackRsp.has_value())
			{
				std::cout << "rsp pubcomp: " << (int)PackRsp->GetVariableHeader().PacketId.Value << '\n';
			}
		}
	}
#endif // MQTT_PUBLISH_QOS_2
	/////////////////////////////////////////////////////////////
	{
		mqtt::tPacketDISCONNECT Pack;
		std::future<std::optional<mqtt::tPacketDISCONNECT::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketDISCONNECT>(socket, Pack); });

		utils::share::TaskTransactionWait(TaskFuture, 10000, "DISCONNECT");

		TaskFuture.get(); // in case of exception - get it here

		g_Log.TestMessage("Disconnected!");
	}
	/////////////////////////////////////////////////////////////
}
