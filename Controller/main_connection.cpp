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

static bool TaskConnection_Connect(tcp::socket& socket, std::uint16_t keepAlive)
{
	mqtt::tPacketCONNECT Pack(mqtt::tSessionStateRequest::Continue, keepAlive, "duper_star"); // 1883 // [!] it differs from SensorA
	std::future<std::optional<mqtt::tPacketCONNECT::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketCONNECT>(socket, Pack); });
	utils::share::TaskTransactionWait(TaskFuture, 10000, "CONNECT");
	auto PackRsp = TaskFuture.get();
	return PackRsp.has_value() && PackRsp->GetVariableHeader().ConnectAcknowledgeFlags.Field.SessionPresent;
}

static void TaskConnection_Subscribe(tcp::socket& socket, std::uint16_t& packetId)
{
	std::vector<mqtt::tSubscribeTopicFilter> Filters;
	Filters.emplace_back("SensorA_will", mqtt::tQoS::AtMostOnceDelivery);		// [!] it differs from SensorA
	Filters.emplace_back("SensorA_DateTime", mqtt::tQoS::AtMostOnceDelivery);	// [!] it differs from SensorA
	mqtt::tPacketSUBSCRIBE Pack(++packetId, Filters); // [TBD] Packet Id should be set.
	std::future<std::optional<mqtt::tPacketSUBSCRIBE::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketSUBSCRIBE>(socket, Pack); });
	utils::share::TaskTransactionWait(TaskFuture, 10000, "SUBACK");
	TaskFuture.get(); // in case of exception - get it here
}

static void TaskConnection_Publish(tcp::socket& socket, std::uint16_t& packetId, const std::string& sensorData)
{
#ifdef MQTT_PUBLISH_QOS_0
	{
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::AtMostOnceDelivery>;
		tPackPublish Pack(false, true, "SensorA_Settings", std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBACK");
		TaskFuture.get(); // in case of exception - get it here
	}
#endif // MQTT_PUBLISH_QOS_0
#ifdef MQTT_PUBLISH_QOS_1
	{
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::AtLeastOnceDelivery>;
		tPackPublish Pack(true, true, "SensorA_Settings", ++packetId, std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBACK");
		auto PackRsp = TaskFuture.get(); // in case of exception - get it here
		if (PackRsp.has_value())
			std::cout << "rsp puback: " << (int)PackRsp->GetVariableHeader().PacketId.Value << '\n';
	}
#endif // MQTT_PUBLISH_QOS_1
#ifdef MQTT_PUBLISH_QOS_2
	{
		using tPackPublish = mqtt::tPacketPUBLISH<mqtt::tQoS::ExactlyOnceDelivery>;
		tPackPublish Pack(true, true, "SensorA_Settings", ++packetId, std::vector<std::uint8_t>(sensorData.begin(), sensorData.end()));
		std::future<std::optional<tPackPublish::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<tPackPublish>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "PUBREC");
		auto PackRsp = TaskFuture.get(); // in case of exception - get it here
		if (PackRsp.has_value())
		{
			std::cout << "rsp pubrec: " << (int)PackRsp->GetVariableHeader().PacketId.Value << '\n';

			mqtt::tPacketPUBREL Pack(++packetId);
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
}

static void TaskConnection_Ping(tcp::socket& socket)
{
	mqtt::tPacketPINGREQ PackPINGREQ;
	std::future<std::optional<mqtt::tPacketPINGREQ::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketPINGREQ>(socket, PackPINGREQ); });
	utils::share::TaskTransactionWait(TaskFuture, 10000, "PING");
	TaskFuture.get(); // in case of exception - get it here
}


static void TaskConnection_Disconnect(tcp::socket& socket)
{
	mqtt::tPacketDISCONNECT Pack;
	std::future<std::optional<mqtt::tPacketDISCONNECT::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketDISCONNECT>(socket, Pack); });
	utils::share::TaskTransactionWait(TaskFuture, 10000, "DISCONNECT");
	TaskFuture.get(); // in case of exception - get it here
}

void TaskConnectionHandler(tcp::socket& socket)
{
	std::future<void> TaskFutureReceiving = std::async(std::launch::async, [&]() { return utils::share::TaskReceiveHandler(socket); });

	constexpr std::uint16_t KeepAlive = 15; // sec. //[#] - TaskTransactionWait(..) should be taken into consideration
	std::uint16_t PacketId = 0;

	const bool SessionContinue = TaskConnection_Connect(socket, KeepAlive);
	//if (!SessionContinue)
		TaskConnection_Subscribe(socket, PacketId);


	TaskConnection_Publish(socket, PacketId, "Hello World! 2");
	/////////////////////////////////////////////////////////////
	constexpr std::uint16_t KeepAlive10ms = 100;// *100;
	std::uint16_t PingCounter = KeepAlive10ms;
	while(true)
	{
		//if ()
		//{
			// Send Data
		//}

		if (--PingCounter)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		PingCounter = KeepAlive10ms;

		TaskConnection_Ping(socket);

		TaskConnection_Subscribe(socket, PacketId);
	}
	/////////////////////////////////////////////////////////////
	// DISCONNECT ??
	/////////////////////////////////////////////////////////////

	TaskFutureReceiving.get();
}
