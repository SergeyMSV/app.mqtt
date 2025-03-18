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

std::vector<std::uint8_t> g_ReceiveBuffer(128);
//std::atomic<bool> g_Exit = false;

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
		THROW_RUNTIME_ERROR("PREVED: NO PACKET 1"); // Res.error() - put it into the message

	return tReceivePacketResult{ *Res, mqtt::tSpan(buffer, SizeRcv) };
}

template <class tCmd, class tRsp>
tRsp TaskTransactionHandler(tcp::socket& socket, const tCmd& packet)
{
	std::cout << packet.ToString() << '\n';

	auto PackVector = packet.ToVector();

	socket.write_some(boost::asio::buffer(PackVector));

	auto Received = ReceivePacket(socket, g_ReceiveBuffer);
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

void TaskConnectionHandler(tcp::socket& socket)
{
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//THROW_RUNTIME_ERROR("Hello world!");
	{
		mqtt::tPacketCONNECT PackCONNECT(false, 10, "duper_star", mqtt::tQoS::AtLeastOnceDelivery, true, "SensorA_will", "something wrong has happened"); // 1883

		std::future<mqtt::tPacketCONNACK>TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketCONNECT, mqtt::tPacketCONNACK>(socket, PackCONNECT); });
		//std::future<mqtt::tPacketCONNACK>TaskFuture = std::async(std::launch::async, TaskTransactionHandler, std::ref(socket), std::ref(PackCONNECT), mqtt::tControlPacketType::CONNACK);

		std::future_status Status;

		do
		{
			Status = TaskFuture.wait_for(std::chrono::milliseconds(100));
			switch (Status)
			{
			case std::future_status::deferred:
				std::cout << "-deferred\n";
				break;
			case std::future_status::timeout:
				std::cout << "-timeout\n";
				break;
			case std::future_status::ready:
				std::cout << "-ready!\n";
				break;
			}
		} while (Status != std::future_status::ready);

		auto Rsp = TaskFuture.get();
		if (Rsp.GetVariableHeader().has_value())
			std::cout << "SessPres: " << (int)Rsp.GetVariableHeader()->ConnectAcknowledgeFlags.Field.SessionPresent << '\n';
	}
	/////////////////////////////////////////////////////////////
	{
		mqtt::tPacketPINGREQ PackPINGREQ;

		std::future<mqtt::tPacketPINGRESP>TaskFuture = std::async(std::launch::async, [&]() { return TaskTransactionHandler<mqtt::tPacketPINGREQ, mqtt::tPacketPINGRESP>(socket, PackPINGREQ); });

		while (TaskFuture.wait_for(std::chrono::milliseconds(1)) != std::future_status::ready)
		//while (TaskFuture.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready)
		{
			// [TBD] if it lasts too long - just throw an exception and then restart the socket in main thread
			THROW_RUNTIME_ERROR("Hello world!");
		}

		auto Rsp = TaskFuture.get();
		if (Rsp.GetVariableHeader().has_value())
			std::cout << "PINGRESP" <<'\n';
	}
	/////////////////////////////////////////////////////////////
}
