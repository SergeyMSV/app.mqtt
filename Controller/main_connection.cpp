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

void TaskConnectionHandler(tcp::socket& socket)
{
	constexpr std::uint16_t KeepAlive = 10; // sec.

	{
		//test::tMeasureDuration TCH("TCH-CONNECT");

		mqtt::tPacketCONNECT Pack(false, KeepAlive, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

		std::future<std::optional<mqtt::tPacketCONNECT::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketCONNECT>(socket, Pack); });

		utils::share::TaskTransactionWait(TaskFuture, 10000, "CONNECT");

		auto Rsp = TaskFuture.get();
		if (Rsp.has_value())
			std::cout << "SessPres: " << (int)Rsp->GetVariableHeader().ConnectAcknowledgeFlags.Field.SessionPresent << '\n';
	}
	/////////////////////////////////////////////////////////////
	{
		std::vector<mqtt::tSubscribeTopicFilter> Filters;
		Filters.emplace_back("SensorA_will", mqtt::tQoS::AtMostOnceDelivery);
		Filters.emplace_back("SensorA_DateTime", mqtt::tQoS::AtMostOnceDelivery);
		mqtt::tPacketSUBSCRIBE Pack(0x1234, Filters); // [TBD] Packet Id should be set.
		std::future<std::optional<mqtt::tPacketSUBSCRIBE::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketSUBSCRIBE>(socket, Pack); });
		utils::share::TaskTransactionWait(TaskFuture, 10000, "SUBACK");
		TaskFuture.get(); // in case of exception - get it here
	}
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

		//test::tMeasureDuration TCH("TCH-PING");

		mqtt::tPacketPINGREQ PackPINGREQ;
		std::future<std::optional<mqtt::tPacketPINGREQ::response_type>> TaskFuture = std::async(std::launch::async, [&]() { return utils::share::TaskTransactionHandler<mqtt::tPacketPINGREQ>(socket, PackPINGREQ); });

		utils::share::TaskTransactionWait(TaskFuture, 10000, "PING");

		TaskFuture.get(); // in case of exception - get it here
	}
	/////////////////////////////////////////////////////////////
}
