#include "main.h"

#include <functional>
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
namespace mqtt = utils::packet::mqtt;

using tReceivePacketResult = std::pair<mqtt::tControlPacketType, mqtt::tSpan>;

std::optional<tReceivePacketResult> ReceivePacket(tcp::socket& socket, std::vector<std::uint8_t>& buffer)
{
	boost::system::error_code Error;
	std::size_t SizeRcv = socket.read_some(boost::asio::buffer(buffer), Error);
	if (!SizeRcv)
		return {};

	mqtt::tSpan Span(buffer, SizeRcv);
	auto Res = mqtt::TestPacket(Span);
	if (!Res.has_value())
		THROW_RUNTIME_ERROR("ER: NO PACKET 1"); // Res.error() - put it into the message

	return tReceivePacketResult{ *Res, mqtt::tSpan(buffer, SizeRcv) };
}

template <class tCmd, class tRsp>
tRsp TaskTransactionHandler(tcp::socket& socket, const tCmd& packet)
{
	test::tMeasureDuration Measure("TTH");

	g_Log.PacketSent(packet.ToString());

	auto PackVector = packet.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	std::vector<std::uint8_t> ReceiveBuffer(128);

	auto Received = ReceivePacket(socket, ReceiveBuffer);
	if (!Received.has_value())
		THROW_RUNTIME_ERROR("No data has been received.");

	if (Received->first != tRsp::GetControlPacketType())
		THROW_RUNTIME_ERROR("Received response of an unexpected type.");

	auto Pack_parsed = tRsp::Parse(Received->second);
	if (!Pack_parsed.has_value())
		THROW_RUNTIME_ERROR("Received response has not been parsed."); // Res.error() - put it into the message

	g_Log.PacketReceived(Pack_parsed->ToString());

	return std::move(Pack_parsed.value());
}

template<class T>
void TaskTransactionWait(std::future<T>& future, std::uint32_t time_ms, const std::string& label)
{
	const std::uint32_t Pause = 10; // ms
	std::uint32_t PauseCount = time_ms / Pause;
	while (future.wait_for(std::chrono::milliseconds(Pause)) != std::future_status::ready)
	{
		if (!PauseCount)
			THROW_RUNTIME_ERROR(label + " exceeded timeout.");

		--PauseCount;
	}
}

void TaskConnectionHandler(tcp::socket& socket)
{
	constexpr std::uint16_t KeepAlive = 10; // sec.

	{
		//test::tMeasureDuration TCH("TCH-CONNECT");

		mqtt::tPacketCONNECT PackCONNECT(false, KeepAlive, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

		std::future<mqtt::tPacketCONNACK> TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketCONNECT, mqtt::tPacketCONNACK>(socket, PackCONNECT); });
		//std::future<mqtt::tPacketCONNACK> TaskFuture = std::async(std::launch::async, TaskTransactionHandler, std::ref(socket), std::ref(PackCONNECT), mqtt::tControlPacketType::CONNACK);

		TaskTransactionWait(TaskFuture, 10000, "CONNECT");

		auto Rsp = TaskFuture.get();
		if (Rsp.GetVariableHeader().has_value())
			std::cout << "SessPres: " << (int)Rsp.GetVariableHeader()->ConnectAcknowledgeFlags.Field.SessionPresent << '\n';
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
		std::future<mqtt::tPacketPINGRESP>TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketPINGREQ, mqtt::tPacketPINGRESP>(socket, PackPINGREQ); });

		TaskTransactionWait(TaskFuture, 10000, "PING");

		TaskFuture.get(); // in case of exception - get it here
	}
	/////////////////////////////////////////////////////////////
}
