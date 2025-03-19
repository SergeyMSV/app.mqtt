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
	std::cout << packet.ToString() << '\n';

	auto PackVector = packet.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	std::vector<std::uint8_t> ReceiveBuffer(128);

	auto Received = ReceivePacket(socket, ReceiveBuffer);
	if (!Received.has_value())
		THROW_RUNTIME_ERROR("ER: no connection");

	if (Received->first != tRsp::GetControlPacketType())
		THROW_RUNTIME_ERROR("ER: WRONG PACKET 1");

	auto Pack_parsed = tRsp::Parse(Received->second);
	if (!Pack_parsed.has_value())
		THROW_RUNTIME_ERROR("ER: WRONG PACKET 2"); // Res.error() - put it into the message

	std::cout << Pack_parsed->ToString() << '\n';

	return std::move(Pack_parsed.value());
}

template<class T>
bool FutureWait(std::future<T>& future, std::uint32_t count10ms)
{
	while (future.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready)
	{
		if (!count10ms)
			return false;
		--count10ms;
	}
	return true;
}

void TaskConnectionHandler(tcp::socket& socket)
{
	{
		mqtt::tPacketCONNECT PackCONNECT(false, 10, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

		std::future<mqtt::tPacketCONNACK> TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketCONNECT, mqtt::tPacketCONNACK>(socket, PackCONNECT); });
		//std::future<mqtt::tPacketCONNACK> TaskFuture = std::async(std::launch::async, TaskTransactionHandler, std::ref(socket), std::ref(PackCONNECT), mqtt::tControlPacketType::CONNACK);

		if (!FutureWait(TaskFuture, 10))
			THROW_RUNTIME_ERROR("CONNECT error: exceed timeout.");

		auto Rsp = TaskFuture.get();
		if (Rsp.GetVariableHeader().has_value())
			std::cout << "SessPres: " << (int)Rsp.GetVariableHeader()->ConnectAcknowledgeFlags.Field.SessionPresent << '\n';
	}
	/////////////////////////////////////////////////////////////
	{
		mqtt::tPacketPINGREQ PackPINGREQ;

		std::future<mqtt::tPacketPINGRESP>TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketPINGREQ, mqtt::tPacketPINGRESP>(socket, PackPINGREQ); });

		if (!FutureWait(TaskFuture, 10))
			THROW_RUNTIME_ERROR("PING error: exceed timeout.");

		TaskFuture.get();
	}
	/////////////////////////////////////////////////////////////
}
